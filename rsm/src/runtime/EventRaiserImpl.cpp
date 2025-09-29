#include "runtime/EventRaiserImpl.h"
#include "common/Logger.h"
#include <mutex>

namespace RSM {

EventRaiserImpl::EventRaiserImpl(EventCallback callback)
    : eventCallback_(std::move(callback)), shutdownRequested_(false), isRunning_(false), immediateMode_(false) {
    LOG_DEBUG("EventRaiserImpl: Created with callback: {} (instance: {})", (eventCallback_ ? "set" : "none"),
              (void *)this);

    // Start the async processing thread
    isRunning_.store(true);
    processingThread_ = std::thread(&EventRaiserImpl::eventProcessingWorker, this);

    LOG_DEBUG("EventRaiserImpl: Async processing thread started");
}

EventRaiserImpl::~EventRaiserImpl() {
    shutdown();
}

void EventRaiserImpl::shutdown() {
    if (!isRunning_.load()) {
        return;  // Already shut down
    }

    LOG_DEBUG("EventRaiserImpl: Shutting down async processing");

    // Signal shutdown
    shutdownRequested_.store(true);
    queueCondition_.notify_all();

    // Wait for worker thread to complete
    if (processingThread_.joinable()) {
        processingThread_.join();
    }

    isRunning_.store(false);
    LOG_DEBUG("EventRaiserImpl: Shutdown complete");
}

void EventRaiserImpl::setEventCallback(EventCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    bool hadCallback = (eventCallback_ != nullptr);
    eventCallback_ = std::move(callback);
    bool hasCallback = (eventCallback_ != nullptr);
    LOG_DEBUG(
        "EventRaiserImpl: Callback status changed - EventRaiser: {}, previous: {}, current: {}, immediateMode: {}",
        (void *)this, hadCallback ? "set" : "none", hasCallback ? "set" : "none", immediateMode_.load());
}

void EventRaiserImpl::clearEventCallback() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    eventCallback_ = nullptr;
    LOG_DEBUG("EventRaiserImpl: Event callback cleared");
}

bool EventRaiserImpl::raiseEvent(const std::string &eventName, const std::string &eventData) {
    // Default to INTERNAL priority for backward compatibility (raise actions and #_internal targets)
    return raiseEventWithPriority(eventName, eventData, EventPriority::INTERNAL);
}

bool EventRaiserImpl::raiseEventWithPriority(const std::string &eventName, const std::string &eventData,
                                             EventPriority priority) {
    LOG_DEBUG("EventRaiserImpl::raiseEventWithPriority 호출 - 이벤트: '{}', 데이터: '{}', 우선순위: {}, EventRaiser "
              "인스턴스: {}",
              eventName, eventData, (priority == EventPriority::INTERNAL ? "INTERNAL" : "EXTERNAL"), (void *)this);

    if (!isRunning_.load()) {
        LOG_WARN("EventRaiserImpl: Cannot raise event '{}' - processor is shut down", eventName);
        return false;
    }

    // W3C SCXML compliance: Check if immediate mode is enabled
    if (immediateMode_.load()) {
        // Immediate processing for SCXML executable content
        LOG_DEBUG("EventRaiserImpl: Processing event '{}' immediately (SCXML mode)", eventName);

        // Get callback under lock
        EventCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            callback = eventCallback_;
        }

        if (callback) {
            try {
                return callback(eventName, eventData);
            } catch (const std::exception &e) {
                LOG_ERROR("EventRaiserImpl: Exception in immediate processing: {}", e.what());
                return false;
            }
        } else {
            LOG_WARN("EventRaiserImpl: No callback set for immediate event: {} - EventRaiser: {}, immediateMode: {}",
                     eventName, (void *)this, immediateMode_.load());
            return false;
        }
    }

    // SCXML compliance: Use synchronous queue when immediate mode is disabled
    {
        std::lock_guard<std::mutex> lock(synchronousQueueMutex_);
        synchronousQueue_.emplace(eventName, eventData, priority);
        LOG_DEBUG("EventRaiserImpl: [W3C193 DEBUG] Event '{}' queued with priority {} - queue size now: {}", eventName,
                  (priority == EventPriority::INTERNAL ? "INTERNAL" : "EXTERNAL"), synchronousQueue_.size());
        LOG_DEBUG("EventRaiserImpl: Event '{}' queued for synchronous processing (SCXML compliance) with {} priority",
                  eventName, (priority == EventPriority::INTERNAL ? "INTERNAL" : "EXTERNAL"));
        LOG_DEBUG("EventRaiserImpl: Synchronous queue size after queueing: {}", synchronousQueue_.size());
    }

    // SCXML "fire and forget" - always return true for queuing
    return true;
}

bool EventRaiserImpl::isReady() const {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    return eventCallback_ != nullptr && isRunning_.load();
}

void EventRaiserImpl::eventProcessingWorker() {
    LOG_DEBUG("EventRaiserImpl: Worker thread started");

    while (!shutdownRequested_.load()) {
        std::unique_lock<std::mutex> lock(queueMutex_);

        // Wait for events or shutdown signal
        queueCondition_.wait(lock, [this] { return !eventQueue_.empty() || shutdownRequested_.load(); });

        // Process all queued events
        while (!eventQueue_.empty() && !shutdownRequested_.load()) {
            QueuedEvent event = eventQueue_.front();
            eventQueue_.pop();

            // Release lock during event processing to prevent deadlock
            lock.unlock();

            // Process the event
            processEvent(event);

            // Reacquire lock for next iteration
            lock.lock();
        }
    }

    LOG_DEBUG("EventRaiserImpl: Worker thread stopped");
}

void EventRaiserImpl::processEvent(const QueuedEvent &event) {
    // Get callback under lock
    EventCallback callback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback = eventCallback_;
    }

    if (!callback) {
        LOG_WARN("EventRaiserImpl: No callback set for event: {}", event.eventName);
        return;
    }

    try {
        LOG_DEBUG("EventRaiserImpl: Processing event '{}' with data: {}", event.eventName, event.eventData);

        // Execute the callback (this is where the actual event processing happens)
        bool result = callback(event.eventName, event.eventData);

        // SCXML "fire and forget": Log result but don't propagate failures
        // Event processing failures don't affect the async queue operation
        LOG_DEBUG("EventRaiserImpl: Event '{}' processed with result: {}", event.eventName, result);

    } catch (const std::exception &e) {
        LOG_ERROR("EventRaiserImpl: Exception while processing event '{}': {}", event.eventName, e.what());
    }
}

void EventRaiserImpl::setImmediateMode(bool immediate) {
    immediateMode_.store(immediate);
    LOG_DEBUG("EventRaiserImpl: Immediate mode {}", immediate ? "enabled" : "disabled");
}

void EventRaiserImpl::processQueuedEvents() {
    LOG_DEBUG("EventRaiserImpl: Processing all queued events synchronously");

    // Process all currently queued synchronous events with W3C SCXML priority ordering
    std::vector<QueuedEvent> eventsToProcess;

    // Move all synchronous queued events to local vector under lock
    {
        std::lock_guard<std::mutex> lock(synchronousQueueMutex_);
        LOG_DEBUG("EventRaiserImpl: Synchronous queue size before processing: {}", synchronousQueue_.size());

        // Convert queue to vector for sorting
        while (!synchronousQueue_.empty()) {
            eventsToProcess.push_back(synchronousQueue_.front());
            synchronousQueue_.pop();
        }

        LOG_DEBUG("EventRaiserImpl: Events moved to local vector for processing: {}", eventsToProcess.size());
    }

    // W3C SCXML compliance: Sort events by priority (INTERNAL first, then EXTERNAL)
    // Within same priority, maintain FIFO order using timestamp
    std::stable_sort(eventsToProcess.begin(), eventsToProcess.end(), [](const QueuedEvent &a, const QueuedEvent &b) {
        if (a.priority != b.priority) {
            return a.priority < b.priority;  // INTERNAL (0) before EXTERNAL (1)
        }
        return a.timestamp < b.timestamp;  // FIFO within same priority
    });

    LOG_DEBUG("EventRaiserImpl: Events sorted by W3C SCXML priority (INTERNAL first, then EXTERNAL)");

    // [W3C193 DEBUG] Log the event processing order
    for (size_t i = 0; i < eventsToProcess.size(); ++i) {
        const auto &event = eventsToProcess[i];
        LOG_DEBUG("EventRaiserImpl: [W3C193 DEBUG] Event processing order[{}]: '{}' with priority {}", i,
                  event.eventName, (event.priority == EventPriority::INTERNAL ? "INTERNAL" : "EXTERNAL"));
    }

    // Process events without holding the queue lock
    for (const auto &event : eventsToProcess) {
        LOG_DEBUG("EventRaiserImpl: Synchronously processing queued event '{}' with {} priority", event.eventName,
                  (event.priority == EventPriority::INTERNAL ? "INTERNAL" : "EXTERNAL"));

        // Get callback under lock and process immediately
        EventCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            callback = eventCallback_;
        }

        LOG_DEBUG("EventRaiserImpl: Callback availability for event '{}': {} (instance: {})", event.eventName,
                  (callback ? "available" : "null"), (void *)this);

        if (callback) {
            try {
                LOG_DEBUG("EventRaiserImpl: About to call callback for event '{}' with data: '{}'", event.eventName,
                          event.eventData);
                bool result = callback(event.eventName, event.eventData);
                LOG_DEBUG("EventRaiserImpl: Synchronous event '{}' processed with result: {}", event.eventName, result);
            } catch (const std::exception &e) {
                LOG_ERROR("EventRaiserImpl: Exception in synchronous processing: {}", e.what());
            }
        } else {
            LOG_WARN("EventRaiserImpl: No callback set for synchronous event: {}", event.eventName);
        }
    }

    LOG_DEBUG("EventRaiserImpl: Finished processing all queued events");
}

}  // namespace RSM