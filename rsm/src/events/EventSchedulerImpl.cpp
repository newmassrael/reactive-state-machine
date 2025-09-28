#include "events/EventSchedulerImpl.h"
#include "common/Logger.h"
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
    auto existingIt = scheduledEvents_.find(actualSendId);
    if (existingIt != scheduledEvents_.end()) {
        LOG_DEBUG("EventSchedulerImpl: Cancelling existing event with sendId: {}", actualSendId);
        existingIt->second->cancelled = true;
        scheduledEvents_.erase(existingIt);
    }

    // Calculate execution time
    auto executeAt = std::chrono::steady_clock::now() + delay;

    // Create scheduled event
    auto scheduledEvent = std::make_unique<ScheduledEvent>(event, executeAt, target, actualSendId, sessionId);
    auto future = scheduledEvent->sendIdPromise.get_future();

    // Set the send ID promise immediately
    scheduledEvent->sendIdPromise.set_value(actualSendId);

    // Store the event
    scheduledEvents_[actualSendId] = std::move(scheduledEvent);

    LOG_DEBUG("EventSchedulerImpl: Scheduled event '{}' with sendId '{}' for {}ms delay", event.eventName, actualSendId,
              delay.count());

    // Notify timer thread about new event
    timerCondition_.notify_one();

    return future;
}

bool EventSchedulerImpl::cancelEvent(const std::string &sendId) {
    if (sendId.empty()) {
        LOG_WARN("EventSchedulerImpl: Cannot cancel event with empty sendId");
        return false;
    }

    std::lock_guard<std::mutex> lock(schedulerMutex_);

    auto it = scheduledEvents_.find(sendId);
    if (it != scheduledEvents_.end() && !it->second->cancelled) {
        LOG_DEBUG("EventSchedulerImpl: Cancelling event with sendId: {}", sendId);
        it->second->cancelled = true;
        scheduledEvents_.erase(it);

        // Notify timer thread about cancellation
        timerCondition_.notify_one();
        return true;
    }

    LOG_DEBUG("EventSchedulerImpl: Event with sendId '{}' not found or already cancelled", sendId);
    return false;
}

size_t EventSchedulerImpl::cancelEventsForSession(const std::string &sessionId) {
    if (sessionId.empty()) {
        LOG_WARN("EventSchedulerImpl: Cannot cancel events for empty sessionId");
        return 0;
    }

    std::lock_guard<std::mutex> lock(schedulerMutex_);

    size_t cancelledCount = 0;
    auto it = scheduledEvents_.begin();

    while (it != scheduledEvents_.end()) {
        if (it->second->sessionId == sessionId && !it->second->cancelled) {
            LOG_DEBUG("EventSchedulerImpl: Cancelling event '{}' with sendId '{}' for session '{}'",
                      it->second->event.eventName, it->first, sessionId);
            it->second->cancelled = true;
            it = scheduledEvents_.erase(it);
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
    auto it = scheduledEvents_.find(sendId);
    return it != scheduledEvents_.end() && !it->second->cancelled;
}

size_t EventSchedulerImpl::getScheduledEventCount() const {
    std::lock_guard<std::mutex> lock(schedulerMutex_);
    return scheduledEvents_.size();
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

    // Wait for callback threads to finish
    for (auto &thread : callbackThreads_) {
        if (thread.joinable()) {
            if (waitForCompletion) {
                thread.join();
            } else {
                thread.detach();
            }
        }
    }

    // Wake up timer thread
    timerCondition_.notify_all();

    // Wait for timer thread to finish
    if (timerThread_.joinable()) {
        if (waitForCompletion) {
            timerThread_.join();
        } else {
            timerThread_.detach();
        }
    }

    // Clear all scheduled events
    {
        std::lock_guard<std::mutex> lock(schedulerMutex_);
        size_t cancelledCount = scheduledEvents_.size();
        scheduledEvents_.clear();

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
    LOG_DEBUG("EventSchedulerImpl: Timer thread started");

    while (!shutdownRequested_) {
        std::unique_lock<std::mutex> lock(schedulerMutex_);

        // Calculate when we need to wake up next
        auto nextExecutionTime = getNextExecutionTime();

        if (nextExecutionTime == std::chrono::steady_clock::time_point::max()) {
            // No events scheduled, wait indefinitely until notified
            LOG_DEBUG("EventSchedulerImpl: No events scheduled, waiting for notification");
            timerCondition_.wait(lock, [&] { return shutdownRequested_.load() || !scheduledEvents_.empty(); });
        } else {
            // Wait until next event time or notification
            auto now = std::chrono::steady_clock::now();
            if (nextExecutionTime > now) {
                auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(nextExecutionTime - now);
                LOG_DEBUG("EventSchedulerImpl: Waiting {}ms for next event", waitTime.count());

                timerCondition_.wait_until(lock, nextExecutionTime, [&] { return shutdownRequested_.load(); });
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
    std::vector<std::unique_ptr<ScheduledEvent>> readyEvents;
    auto now = std::chrono::steady_clock::now();

    // Collect ready events under lock
    {
        std::lock_guard<std::mutex> lock(schedulerMutex_);

        auto it = scheduledEvents_.begin();
        while (it != scheduledEvents_.end()) {
            if (!it->second->cancelled && it->second->executeAt <= now) {
                readyEvents.push_back(std::move(it->second));
                it = scheduledEvents_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Queue events for asynchronous callback execution (PREVENTS DEADLOCK)
    for (auto &event : readyEvents) {
        LOG_DEBUG("EventSchedulerImpl: Queueing event '{}' with sendId '{}' for async execution",
                  event->event.eventName, event->sendId);

        // Create callback task that captures the event data
        auto callbackTask = [this, eventDescriptor = event->event, target = event->target, sendId = event->sendId]() {
            try {
                LOG_DEBUG("EventSchedulerImpl: Executing event '{}' asynchronously", eventDescriptor.eventName);

                // Execute the callback without holding any scheduler locks
                bool success = executionCallback_(eventDescriptor, target, sendId);

                if (success) {
                    LOG_DEBUG("EventSchedulerImpl: Event '{}' executed successfully", eventDescriptor.eventName);
                } else {
                    LOG_WARN("EventSchedulerImpl: Event '{}' execution failed", eventDescriptor.eventName);
                }

            } catch (const std::exception &e) {
                LOG_ERROR("EventSchedulerImpl: Error executing event '{}': {}", eventDescriptor.eventName, e.what());
            }
        };

        // Add to callback queue
        {
            std::lock_guard<std::mutex> callbackLock(callbackQueueMutex_);
            callbackQueue_.push(std::move(callbackTask));
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
    auto now =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());

    uint64_t counter = sendIdCounter_.fetch_add(1);

    std::ostringstream oss;
    oss << "auto_" << now.count() << "_" << std::setfill('0') << std::setw(6) << counter;
    return oss.str();
}

std::chrono::steady_clock::time_point EventSchedulerImpl::getNextExecutionTime() const {
    if (scheduledEvents_.empty()) {
        return std::chrono::steady_clock::time_point::max();
    }

    auto earliestTime = std::chrono::steady_clock::time_point::max();
    for (const auto &pair : scheduledEvents_) {
        if (!pair.second->cancelled && pair.second->executeAt < earliestTime) {
            earliestTime = pair.second->executeAt;
        }
    }

    return earliestTime;
}

}  // namespace RSM