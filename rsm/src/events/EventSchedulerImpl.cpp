#include "events/EventSchedulerImpl.h"
#include "common/Logger.h"
#include "common/UniqueIdGenerator.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace RSM {

EventSchedulerImpl::EventSchedulerImpl(EventExecutionCallback executionCallback)
    : executionCallback_(std::move(executionCallback)) {
    if (!executionCallback_) {
        throw std::invalid_argument("EventSchedulerImpl requires a valid execution callback");
    }

    // Initialize running state but DON'T start threads yet to prevent constructor deadlock
    running_ = true;

    // CRITICAL: Defer thread creation to prevent deadlock during object construction
    // Threads will be started lazily on first scheduleEvent() call

    LOG_DEBUG("EventSchedulerImpl: Scheduler started with timer thread and {} callback threads",
              CALLBACK_THREAD_POOL_SIZE);
}

EventSchedulerImpl::~EventSchedulerImpl() {
    // CRITICAL: Destructor must ALWAYS wait for threads, even if shutdown() was called previously
    // We can't detach in destructor context because object is being destroyed

    // Signal shutdown
    shutdownRequested_ = true;
    callbackShutdownRequested_ = true;
    callbackCondition_.notify_all();
    timerCondition_.notify_all();

    // Force join callback threads (can't detach in destructor)
    for (auto &thread : callbackThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // Force join timer thread (can't detach in destructor)
    if (timerThread_.joinable()) {
        timerThread_.join();
    }

    // Now safe to destroy other members
    running_ = false;
}

std::future<std::string> EventSchedulerImpl::scheduleEvent(const EventDescriptor &event,
                                                           std::chrono::milliseconds delay,
                                                           std::shared_ptr<IEventTarget> target,
                                                           const std::string &sendId, const std::string &sessionId) {
    if (!isRunning()) {
        std::promise<std::string> errorPromise;
        errorPromise.set_exception(std::make_exception_ptr(std::runtime_error("EventScheduler is not running")));
        return errorPromise.get_future();
    }

    if (!target) {
        std::promise<std::string> errorPromise;
        errorPromise.set_exception(std::make_exception_ptr(std::invalid_argument("Event target cannot be null")));
        return errorPromise.get_future();
    }

    // TSAN FIX: Use unique_lock for write operations (insert/erase)
    std::unique_lock<std::shared_mutex> lock(schedulerMutex_);

    // Lazy thread initialization to prevent constructor deadlock
    ensureThreadsStarted();

    // Generate or use provided send ID
    std::string actualSendId = sendId.empty() ? generateSendId() : sendId;

    // Cancel existing event with same send ID (W3C SCXML behavior)
    // TSAN FIX: Only mark as cancelled, don't erase from sendIdIndex_
    // Timer thread will cleanup during processReadyEvents() - single-writer pattern
    auto existingIt = sendIdIndex_.find(actualSendId);
    if (existingIt != sendIdIndex_.end()) {
        LOG_DEBUG("EventSchedulerImpl: Cancelling existing event with sendId: {}", actualSendId);
        existingIt->second->cancelled = true;
        // Priority queue entry will be cleaned up during processReadyEvents()
    }

    // Calculate execution time
    auto now = std::chrono::steady_clock::now();
    auto executeAt = now + delay;

    // Assign sequence number for FIFO ordering (events with same executeAt)
    uint64_t sequenceNum = eventSequenceCounter_.fetch_add(1, std::memory_order_relaxed);

    // Create scheduled event as shared_ptr for safe async access
    auto scheduledEvent =
        std::make_shared<ScheduledEvent>(event, executeAt, target, actualSendId, sessionId, sequenceNum);
    auto future = scheduledEvent->sendIdPromise.get_future();

    // Set the send ID promise immediately
    scheduledEvent->sendIdPromise.set_value(actualSendId);

    // Store in both data structures
    // CRITICAL FIX: Store shared_ptr instead of raw pointer for memory safety
    sendIdIndex_[actualSendId] = scheduledEvent;
    indexSize_.fetch_add(1, std::memory_order_release);  // TSAN FIX: Atomic size tracking
    executionQueue_.push(scheduledEvent);
    queueSize_.fetch_add(1, std::memory_order_release);  // TSAN FIX: Atomic size tracking

    // TSAN FIX: Update cached next event time to avoid queue access in wait_until predicate
    if (executeAt < nextEventTime_) {
        nextEventTime_ = executeAt;
    }

    LOG_DEBUG("EventSchedulerImpl: Scheduled event '{}' with sendId '{}' for {}ms delay in session '{}'",
              event.eventName, actualSendId, delay.count(), sessionId);

    // Notify timer thread about new event
    timerCondition_.notify_one();

    return future;
}

bool EventSchedulerImpl::cancelEvent(const std::string &sendId, const std::string &sessionId) {
    if (sendId.empty()) {
        LOG_WARN("EventSchedulerImpl: Cannot cancel event with empty sendId");
        return false;
    }

    // TSAN FIX: Use shared_lock for find(), only read access needed
    std::shared_lock<std::shared_mutex> lock(schedulerMutex_);

    auto it = sendIdIndex_.find(sendId);
    if (it != sendIdIndex_.end() && !it->second->cancelled) {
        // W3C SCXML 6.3: Cross-session isolation - events can only be cancelled from the same session
        if (!sessionId.empty() && it->second->sessionId != sessionId) {
            LOG_DEBUG("EventSchedulerImpl: Cross-session cancel blocked - event from '{}', cancel from '{}'",
                      it->second->sessionId, sessionId);
            return false;
        }

        LOG_DEBUG("EventSchedulerImpl: Cancelling event with sendId: {}", sendId);
        // TSAN FIX: Only mark as cancelled, don't erase from sendIdIndex_
        // Timer thread will cleanup during processReadyEvents() - single-writer pattern
        it->second->cancelled = true;

        // Notify timer thread about cancellation
        timerCondition_.notify_one();
        return true;
    }

    LOG_DEBUG("EventSchedulerImpl: Event with sendId '{}' not found or already cancelled (Cross-session cancel attempt "
              "may be blocked)",
              sendId);
    return false;
}

size_t EventSchedulerImpl::cancelEventsForSession(const std::string &sessionId) {
    if (sessionId.empty()) {
        LOG_WARN("EventSchedulerImpl: Cannot cancel events for empty sessionId");
        return 0;
    }

    // TSAN FIX: Use shared_lock for iteration, only marking cancelled flag
    std::shared_lock<std::shared_mutex> lock(schedulerMutex_);

    size_t cancelledCount = 0;

    // TSAN FIX: Only mark as cancelled, don't erase from sendIdIndex_
    // Timer thread will cleanup during processReadyEvents() - single-writer pattern
    for (auto &[sendId, event] : sendIdIndex_) {
        if (event->sessionId == sessionId && !event->cancelled) {
            LOG_DEBUG("EventSchedulerImpl: Cancelling event '{}' with sendId '{}' for session '{}'",
                      event->event.eventName, sendId, sessionId);
            event->cancelled = true;
            cancelledCount++;
        }
    }

    if (cancelledCount > 0) {
        LOG_DEBUG("EventSchedulerImpl: Cancelled {} events for session '{}'", cancelledCount, sessionId);
        // Notify timer thread about cancellations
        timerCondition_.notify_one();
    }

    return cancelledCount;
}

bool EventSchedulerImpl::hasEvent(const std::string &sendId) const {
    if (sendId.empty()) {
        return false;
    }

    // TSAN FIX: Use shared_lock for read-only find() operation
    std::shared_lock<std::shared_mutex> lock(schedulerMutex_);
    auto it = sendIdIndex_.find(sendId);
    return it != sendIdIndex_.end() && !it->second->cancelled;
}

size_t EventSchedulerImpl::getScheduledEventCount() const {
    // CONSISTENCY FIX: Use mutex to ensure consistency with actual container
    // Atomic counters are kept for internal use (wait predicates), but public API must be consistent
    // TSAN FIX: Use shared_lock for read-only size() access
    std::shared_lock<std::shared_mutex> lock(schedulerMutex_);
    return sendIdIndex_.size();
}

void EventSchedulerImpl::shutdown(bool waitForCompletion) {
    // CRITICAL FIX: Always signal shutdown and check threads, even if already marked as not running
    // This ensures destructor can safely join threads that were previously detached
    bool alreadyShutdown = !running_.exchange(false);

    if (!alreadyShutdown) {
        LOG_DEBUG("EventSchedulerImpl: Shutting down scheduler (waitForCompletion={})", waitForCompletion);
    }

    // Always set shutdown flags to signal threads
    shutdownRequested_ = true;
    callbackShutdownRequested_ = true;

    // Wake up callback threads
    callbackCondition_.notify_all();

    // CRITICAL FIX: Use thread_local flag to detect if we're in scheduler's own thread
    // This is more reliable than comparing thread IDs
    bool calledFromSchedulerThread = isInSchedulerThread_;

    // CRITICAL: Never detach threads - this creates unsafe scenario where threads
    // can outlive the object and access destroyed member variables
    // If called from scheduler thread, skip join to avoid deadlock, but threads
    // will be joined in destructor when called from external context

    // Wait for callback threads to finish
    if (!calledFromSchedulerThread && waitForCompletion) {
        for (auto &thread : callbackThreads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    // Wake up timer thread
    timerCondition_.notify_all();

    // Wait for timer thread to finish BEFORE acquiring mutex to prevent deadlock
    if (!calledFromSchedulerThread && waitForCompletion && timerThread_.joinable()) {
        timerThread_.join();
    }

    // Clear all scheduled events AFTER timer thread has terminated
    // CRITICAL FIX: This prevents deadlock where timer thread holds mutex while shutdown() tries to acquire it
    {
        // TSAN FIX: Use unique_lock for write operations (clear/erase)
        std::unique_lock<std::shared_mutex> lock(schedulerMutex_);
        size_t cancelledCount = sendIdIndex_.size();
        sendIdIndex_.clear();
        indexSize_.store(0, std::memory_order_release);  // TSAN FIX: Reset atomic counter
        // Clear the priority queue by creating a new empty one
        // CRITICAL FIX: Use shared_ptr type for memory safety
        std::priority_queue<std::shared_ptr<ScheduledEvent>, std::vector<std::shared_ptr<ScheduledEvent>>,
                            ExecutionTimeComparator>
            emptyQueue;
        executionQueue_.swap(emptyQueue);
        queueSize_.store(0, std::memory_order_release);  // TSAN FIX: Reset atomic counter

        if (cancelledCount > 0) {
            LOG_DEBUG("EventSchedulerImpl: Cancelled {} pending events during shutdown", cancelledCount);
        }
    }

    LOG_DEBUG("EventSchedulerImpl: Scheduler shutdown complete");
}

bool EventSchedulerImpl::isRunning() const {
    return running_;
}

void EventSchedulerImpl::timerThreadMain() {
    // Mark this thread as a scheduler thread to prevent deadlock on shutdown
    isInSchedulerThread_ = true;

    LOG_DEBUG("EventSchedulerImpl: Timer thread started");

    while (!shutdownRequested_) {
        // TSAN FIX: Use unique_lock with shared_mutex for condition variable wait
        std::unique_lock<std::shared_mutex> lock(schedulerMutex_);

        // TSAN FIX: Use atomic queue size to avoid calling empty() which triggers races
        size_t currentSize = queueSize_.load(std::memory_order_acquire);

        // TSAN FIX: Use cached next event time to avoid queue access in predicate
        // Update cache from queue (mutex already held)
        if (currentSize > 0) {
            nextEventTime_ = executionQueue_.top()->executeAt;
        } else {
            nextEventTime_ = std::chrono::steady_clock::time_point::max();
        }
        auto nextExecutionTime = nextEventTime_;

        if (nextExecutionTime == std::chrono::steady_clock::time_point::max()) {
            // No events scheduled, wait indefinitely until notified
            LOG_DEBUG("EventSchedulerImpl: No events scheduled, waiting for notification");
            timerCondition_.wait(
                lock, [&] { return shutdownRequested_.load() || queueSize_.load(std::memory_order_acquire) > 0; });
        } else {
            // Wait until next event time or notification
            auto now = std::chrono::steady_clock::now();
            if (nextExecutionTime > now) {
                auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(nextExecutionTime - now);
                LOG_DEBUG("EventSchedulerImpl: Waiting {}ms for next event", waitTime.count());

                // TSAN FIX: Use cached nextEventTime_ instead of accessing queue in predicate
                // This prevents data race when vector reallocation happens during predicate evaluation
                timerCondition_.wait_until(lock, nextExecutionTime, [&] {
                    return shutdownRequested_.load() || nextEventTime_ < nextExecutionTime;
                });
            }
        }

        if (shutdownRequested_) {
            break;
        }

        // Process ready events (releases lock temporarily)
        lock.unlock();
        size_t processedCount = processReadyEvents();
        if (processedCount > 0) {
            LOG_DEBUG("EventSchedulerImpl: Processed {} ready events", processedCount);
        }
    }

    LOG_DEBUG("EventSchedulerImpl: Timer thread stopped");
}

size_t EventSchedulerImpl::processReadyEvents() {
    std::vector<std::shared_ptr<ScheduledEvent>> readyEvents;
    auto now = std::chrono::steady_clock::now();

    // Collect ready events from priority queue under lock
    {
        // TSAN FIX: Use unique_lock for write operations (erase from sendIdIndex_)
        std::unique_lock<std::shared_mutex> lock(schedulerMutex_);

        // Process events from priority queue in execution time order
        // TSAN FIX: Use atomic size instead of empty() to avoid vector pointer races
        while (queueSize_.load(std::memory_order_acquire) > 0) {
            // CRITICAL BUG FIX: Copy shared_ptr BEFORE pop() to avoid dangling reference
            // Using const reference after pop() causes use-after-free bug
            std::shared_ptr<ScheduledEvent> topEvent = executionQueue_.top();

            // If event is cancelled, remove from both structures atomically
            if (topEvent->cancelled) {
                executionQueue_.pop();
                queueSize_.fetch_sub(1, std::memory_order_release);  // TSAN FIX: Atomic size tracking
                // CRITICAL FIX: Also remove from sendIdIndex_ to prevent memory leak
                auto cancelledIt = sendIdIndex_.find(topEvent->sendId);
                if (cancelledIt != sendIdIndex_.end()) {
                    sendIdIndex_.erase(cancelledIt);
                    indexSize_.fetch_sub(1, std::memory_order_release);  // TSAN FIX: Atomic size tracking
                    LOG_DEBUG("EventSchedulerImpl: Cleaned up cancelled event from sendIdIndex_: {}", topEvent->sendId);
                }
                continue;
            }

            // If event is not ready yet, break (all remaining events are later)
            if (topEvent->executeAt > now) {
                break;
            }

            // Event is ready - remove from both structures atomically
            // CRITICAL FIX: Ensure atomic update of dual data structures under mutex lock
            executionQueue_.pop();
            queueSize_.fetch_sub(1, std::memory_order_release);  // TSAN FIX: Atomic size tracking
            auto it = sendIdIndex_.find(topEvent->sendId);
            if (it != sendIdIndex_.end()) {
                readyEvents.push_back(it->second);
                sendIdIndex_.erase(it);
                indexSize_.fetch_sub(1, std::memory_order_release);  // TSAN FIX: Atomic size tracking
                // Both structures are now consistent - event removed from queue and index
            } else {
                // This should not happen in normal operation - log for debugging
                LOG_WARN("EventSchedulerImpl: Event in queue but not in index - sendId: {}", topEvent->sendId);
            }
        }
    }

    // Process events with per-session sequential execution + inter-session parallelism
    std::unordered_map<std::string, std::vector<std::shared_ptr<ScheduledEvent>>> sessionEventGroups;

    // Group events by session (shared_ptr allows safe copying)
    for (auto &event : readyEvents) {
        sessionEventGroups[event->sessionId].emplace_back(event);
    }

    // Execute each session's events asynchronously (sessions run in parallel, events within session are sequential)
    for (auto &[sessionId, sessionEvents] : sessionEventGroups) {
        if (sessionEvents.empty()) {
            continue;
        }

        // Create async task for this session's sequential execution
        auto sessionTask = [this, sessionId, sessionEvents]() {
            LOG_DEBUG("EventSchedulerImpl: Processing {} events for session '{}'", sessionEvents.size(), sessionId);

            // Execute events within this session SEQUENTIALLY
            for (auto &eventPtr : sessionEvents) {
                if (!eventPtr) {
                    LOG_ERROR("EventSchedulerImpl: NULL shared_ptr in session '{}'", sessionId);
                    continue;
                }
                try {
                    LOG_DEBUG("EventSchedulerImpl: Executing event '{}' sequentially in session '{}'",
                              eventPtr->event.eventName, sessionId);

                    // Execute the callback
                    bool success = executionCallback_(eventPtr->event, eventPtr->target, eventPtr->sendId);

                    if (success) {
                        LOG_DEBUG("EventSchedulerImpl: Event '{}' executed successfully", eventPtr->event.eventName);
                    } else {
                        LOG_WARN("EventSchedulerImpl: Event '{}' execution failed", eventPtr->event.eventName);
                    }

                } catch (const std::exception &e) {
                    LOG_ERROR("EventSchedulerImpl: Error executing event '{}': {}", eventPtr->event.eventName,
                              e.what());
                }
            }
        };

        // Add to callback queue for asynchronous execution
        {
            std::lock_guard<std::mutex> callbackLock(callbackQueueMutex_);
            callbackQueue_.push(std::move(sessionTask));
        }

        // Notify callback workers
        callbackCondition_.notify_one();
    }

    return readyEvents.size();
}

void EventSchedulerImpl::ensureThreadsStarted() {
    // Note: This method assumes schedulerMutex_ is already locked by caller

    std::call_once(threadsStartedFlag_, [this]() {
        LOG_DEBUG("EventSchedulerImpl: Starting threads lazily to prevent constructor deadlock");

        // Start callback execution thread pool
        for (size_t i = 0; i < CALLBACK_THREAD_POOL_SIZE; ++i) {
            callbackThreads_.emplace_back(&EventSchedulerImpl::callbackWorker, this);
        }

        // Start timer thread
        timerThread_ = std::thread(&EventSchedulerImpl::timerThreadMain, this);

        LOG_DEBUG("EventSchedulerImpl: All threads started successfully");
    });
}

void EventSchedulerImpl::callbackWorker() {
    // Mark this thread as a scheduler thread to prevent deadlock on shutdown
    isInSchedulerThread_ = true;

    LOG_DEBUG("EventSchedulerImpl: Callback worker thread started");

    while (!callbackShutdownRequested_) {
        std::unique_lock<std::mutex> lock(callbackQueueMutex_);

        // Wait for callback tasks or shutdown
        callbackCondition_.wait(lock, [this] { return !callbackQueue_.empty() || callbackShutdownRequested_.load(); });

        if (callbackShutdownRequested_) {
            break;
        }

        // Get next callback task
        if (!callbackQueue_.empty()) {
            auto task = std::move(callbackQueue_.front());
            callbackQueue_.pop();
            lock.unlock();

            // Execute callback without holding any locks - THIS PREVENTS DEADLOCK
            try {
                task();
            } catch (const std::exception &e) {
                LOG_ERROR("EventSchedulerImpl: Exception in callback worker: {}", e.what());
            } catch (...) {
                LOG_ERROR("EventSchedulerImpl: Unknown exception in callback worker");
            }
        }
    }

    LOG_DEBUG("EventSchedulerImpl: Callback worker thread stopped");
}

std::string EventSchedulerImpl::generateSendId() {
    // REFACTOR: Use centralized UniqueIdGenerator instead of duplicate logic
    return UniqueIdGenerator::generateSendId();
}

std::chrono::steady_clock::time_point EventSchedulerImpl::getNextExecutionTime() const {
    // CRITICAL FIX: External interface with proper mutex protection
    // TSAN FIX: Use shared_lock for read-only access to queue
    std::shared_lock<std::shared_mutex> lock(schedulerMutex_);
    return getNextExecutionTimeUnlocked();
}

std::chrono::steady_clock::time_point EventSchedulerImpl::getNextExecutionTimeUnlocked() const {
    // CRITICAL FIX: Internal method assumes mutex is already locked by caller to prevent deadlock

    // TSAN FIX: Use atomic size instead of empty() to avoid vector pointer races
    if (queueSize_.load(std::memory_order_acquire) == 0) {
        return std::chrono::steady_clock::time_point::max();
    }

    // TSAN FIX: Access executeAt directly without copying shared_ptr to avoid data race
    // during vector reallocation. The const reference is safe because mutex is held.
    const std::shared_ptr<ScheduledEvent> &topEvent = executionQueue_.top();

    // If the top event is cancelled, we still return its time
    // This is safe because processReadyEvents() will skip cancelled events
    return topEvent->executeAt;
}

// Thread-local variable definition
thread_local bool EventSchedulerImpl::isInSchedulerThread_ = false;

}  // namespace RSM
