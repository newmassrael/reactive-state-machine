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
    shutdown(true);
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

    std::lock_guard<std::mutex> lock(schedulerMutex_);

    // Lazy thread initialization to prevent constructor deadlock
    ensureThreadsStarted();

    // Generate or use provided send ID
    std::string actualSendId = sendId.empty() ? generateSendId() : sendId;

    // Cancel existing event with same send ID (W3C SCXML behavior)
    // CRITICAL FIX: Atomic cancellation of existing event under mutex lock
    auto existingIt = sendIdIndex_.find(actualSendId);
    if (existingIt != sendIdIndex_.end()) {
        LOG_DEBUG("EventSchedulerImpl: Cancelling existing event with sendId: {}", actualSendId);
        existingIt->second->cancelled = true;
        sendIdIndex_.erase(existingIt);
        // Priority queue entry will be cleaned up during processReadyEvents()
    }

    // Calculate execution time
    auto now = std::chrono::steady_clock::now();
    auto executeAt = now + delay;

    // Create scheduled event as shared_ptr for safe async access
    auto scheduledEvent = std::make_shared<ScheduledEvent>(event, executeAt, target, actualSendId, sessionId);
    auto future = scheduledEvent->sendIdPromise.get_future();

    // Set the send ID promise immediately
    scheduledEvent->sendIdPromise.set_value(actualSendId);

    // Store in both data structures
    // CRITICAL FIX: Store shared_ptr instead of raw pointer for memory safety
    sendIdIndex_[actualSendId] = scheduledEvent;
    executionQueue_.push(scheduledEvent);

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

    std::lock_guard<std::mutex> lock(schedulerMutex_);

    auto it = sendIdIndex_.find(sendId);
    if (it != sendIdIndex_.end() && !it->second->cancelled) {
        // W3C SCXML 6.3: Cross-session isolation - events can only be cancelled from the same session
        if (!sessionId.empty() && it->second->sessionId != sessionId) {
            LOG_DEBUG("EventSchedulerImpl: Cross-session cancel blocked - event from '{}', cancel from '{}'",
                      it->second->sessionId, sessionId);
            return false;
        }

        LOG_DEBUG("EventSchedulerImpl: Cancelling event with sendId: {}", sendId);
        // CRITICAL FIX: Atomic cancellation - mark cancelled then remove from index
        // Priority queue entry will be cleaned up during processReadyEvents()
        it->second->cancelled = true;
        sendIdIndex_.erase(it);

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

    std::lock_guard<std::mutex> lock(schedulerMutex_);

    size_t cancelledCount = 0;
    auto it = sendIdIndex_.begin();

    while (it != sendIdIndex_.end()) {
        if (it->second->sessionId == sessionId && !it->second->cancelled) {
            LOG_DEBUG("EventSchedulerImpl: Cancelling event '{}' with sendId '{}' for session '{}'",
                      it->second->event.eventName, it->first, sessionId);
            it->second->cancelled = true;
            it = sendIdIndex_.erase(it);
            cancelledCount++;
        } else {
            ++it;
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

    std::lock_guard<std::mutex> lock(schedulerMutex_);
    auto it = sendIdIndex_.find(sendId);
    return it != sendIdIndex_.end() && !it->second->cancelled;
}

size_t EventSchedulerImpl::getScheduledEventCount() const {
    std::lock_guard<std::mutex> lock(schedulerMutex_);
    return sendIdIndex_.size();
}

void EventSchedulerImpl::shutdown(bool waitForCompletion) {
    if (!running_) {
        return;  // Already shut down
    }

    LOG_DEBUG("EventSchedulerImpl: Shutting down scheduler (waitForCompletion={})", waitForCompletion);

    shutdownRequested_ = true;
    callbackShutdownRequested_ = true;
    running_ = false;

    // Wake up callback threads
    callbackCondition_.notify_all();

    // CRITICAL FIX: Use thread_local flag to detect if we're in scheduler's own thread
    // This is more reliable than comparing thread IDs
    bool calledFromSchedulerThread = isInSchedulerThread_;

    // Wait for callback threads to finish
    for (auto &thread : callbackThreads_) {
        if (thread.joinable()) {
            // CRITICAL: If called from scheduler thread, must detach to avoid deadlock
            if (calledFromSchedulerThread || !waitForCompletion) {
                thread.detach();
            } else {
                thread.join();
            }
        }
    }

    // Wake up timer thread
    timerCondition_.notify_all();

    // Wait for timer thread to finish BEFORE acquiring mutex to prevent deadlock
    if (timerThread_.joinable()) {
        // CRITICAL: If called from scheduler thread, must detach to avoid deadlock
        if (calledFromSchedulerThread || !waitForCompletion) {
            timerThread_.detach();
        } else {
            timerThread_.join();
        }
    }

    // Clear all scheduled events AFTER timer thread has terminated
    // CRITICAL FIX: This prevents deadlock where timer thread holds mutex while shutdown() tries to acquire it
    {
        std::lock_guard<std::mutex> lock(schedulerMutex_);
        size_t cancelledCount = sendIdIndex_.size();
        sendIdIndex_.clear();
        // Clear the priority queue by creating a new empty one
        // CRITICAL FIX: Use shared_ptr type for memory safety
        std::priority_queue<std::shared_ptr<ScheduledEvent>, std::vector<std::shared_ptr<ScheduledEvent>>,
                            ExecutionTimeComparator>
            emptyQueue;
        executionQueue_.swap(emptyQueue);

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
        std::unique_lock<std::mutex> lock(schedulerMutex_);

        // Calculate when we need to wake up next
        // CRITICAL FIX: Use unlocked version to prevent deadlock (mutex already held)
        auto nextExecutionTime = getNextExecutionTimeUnlocked();

        if (nextExecutionTime == std::chrono::steady_clock::time_point::max()) {
            // No events scheduled, wait indefinitely until notified
            LOG_DEBUG("EventSchedulerImpl: No events scheduled, waiting for notification");
            timerCondition_.wait(lock, [&] { return shutdownRequested_.load() || !executionQueue_.empty(); });
        } else {
            // Wait until next event time or notification
            auto now = std::chrono::steady_clock::now();
            if (nextExecutionTime > now) {
                auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(nextExecutionTime - now);
                LOG_DEBUG("EventSchedulerImpl: Waiting {}ms for next event", waitTime.count());

                // Include check for new events that might need earlier execution
                // CRITICAL FIX: Use unlocked version to prevent deadlock (mutex already held by wait_until)
                timerCondition_.wait_until(lock, nextExecutionTime, [&] {
                    return shutdownRequested_.load() || getNextExecutionTimeUnlocked() < nextExecutionTime;
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
        std::lock_guard<std::mutex> lock(schedulerMutex_);

        // Process events from priority queue in execution time order
        while (!executionQueue_.empty()) {
            // CRITICAL FIX: Use shared_ptr instead of raw pointer for memory safety
            std::shared_ptr<ScheduledEvent> topEvent = executionQueue_.top();

            // If event is cancelled, remove from both structures atomically
            if (topEvent->cancelled) {
                executionQueue_.pop();
                // CRITICAL FIX: Also remove from sendIdIndex_ to prevent memory leak
                auto cancelledIt = sendIdIndex_.find(topEvent->sendId);
                if (cancelledIt != sendIdIndex_.end()) {
                    sendIdIndex_.erase(cancelledIt);
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
            auto it = sendIdIndex_.find(topEvent->sendId);
            if (it != sendIdIndex_.end()) {
                readyEvents.push_back(it->second);
                sendIdIndex_.erase(it);
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
    std::lock_guard<std::mutex> lock(schedulerMutex_);
    return getNextExecutionTimeUnlocked();
}

std::chrono::steady_clock::time_point EventSchedulerImpl::getNextExecutionTimeUnlocked() const {
    // CRITICAL FIX: Internal method assumes mutex is already locked by caller to prevent deadlock

    if (executionQueue_.empty()) {
        return std::chrono::steady_clock::time_point::max();
    }

    // Find first non-cancelled event without modifying the queue
    // We cannot modify the queue in a const method, so we accept some cancelled events
    // The actual cleanup happens in processReadyEvents()
    // CRITICAL FIX: Use shared_ptr for memory safety
    std::shared_ptr<ScheduledEvent> topEvent = executionQueue_.top();

    // If the top event is cancelled, we still return its time
    // This is safe because processReadyEvents() will skip cancelled events
    return topEvent->executeAt;
}

// Thread-local variable definition
thread_local bool EventSchedulerImpl::isInSchedulerThread_ = false;

}  // namespace RSM
