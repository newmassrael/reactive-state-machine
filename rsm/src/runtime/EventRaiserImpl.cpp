#include "runtime/EventRaiserImpl.h"
#include "common/Logger.h"
#include <mutex>

namespace RSM {

EventRaiserImpl::EventRaiserImpl(EventCallback callback)
    : eventCallback_(std::move(callback)), shutdownRequested_(false), isRunning_(false) {
    Logger::debug("EventRaiserImpl: Created with callback: {}", (eventCallback_ ? "set" : "none"));

    // Start the async processing thread
    isRunning_.store(true);
    processingThread_ = std::thread(&EventRaiserImpl::eventProcessingWorker, this);

    Logger::debug("EventRaiserImpl: Async processing thread started");
}

EventRaiserImpl::~EventRaiserImpl() {
    shutdown();
}

void EventRaiserImpl::shutdown() {
    if (!isRunning_.load()) {
        return;  // Already shut down
    }

    Logger::debug("EventRaiserImpl: Shutting down async processing");

    // Signal shutdown
    shutdownRequested_.store(true);
    queueCondition_.notify_all();

    // Wait for worker thread to complete
    if (processingThread_.joinable()) {
        processingThread_.join();
    }

    isRunning_.store(false);
    Logger::debug("EventRaiserImpl: Shutdown complete");
}

void EventRaiserImpl::setEventCallback(EventCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    eventCallback_ = std::move(callback);
    Logger::debug("EventRaiserImpl: Event callback {}", (eventCallback_ ? "set" : "cleared"));
}

void EventRaiserImpl::clearEventCallback() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    eventCallback_ = nullptr;
    Logger::debug("EventRaiserImpl: Event callback cleared");
}

bool EventRaiserImpl::raiseEvent(const std::string &eventName, const std::string &eventData) {
    if (!isRunning_.load()) {
        Logger::warn("EventRaiserImpl: Cannot raise event '{}' - processor is shut down", eventName);
        return false;
    }

    // SCXML Compliance: "fire and forget" - queue the event and return immediately
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        eventQueue_.emplace(eventName, eventData);
        Logger::debug("EventRaiserImpl: Event '{}' queued for async processing", eventName);
    }

    // Notify the worker thread
    queueCondition_.notify_one();

    // SCXML "fire and forget" - always return true immediately
    return true;
}

bool EventRaiserImpl::isReady() const {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    return eventCallback_ != nullptr && isRunning_.load();
}

void EventRaiserImpl::eventProcessingWorker() {
    Logger::debug("EventRaiserImpl: Worker thread started");

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

    Logger::debug("EventRaiserImpl: Worker thread stopped");
}

void EventRaiserImpl::processEvent(const QueuedEvent &event) {
    // Get callback under lock
    EventCallback callback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback = eventCallback_;
    }

    if (!callback) {
        Logger::warn("EventRaiserImpl: No callback set for event: {}", event.eventName);
        return;
    }

    try {
        Logger::debug("EventRaiserImpl: Processing event '{}' with data: {}", event.eventName, event.eventData);

        // Execute the callback (this is where the actual event processing happens)
        bool result = callback(event.eventName, event.eventData);

        // SCXML "fire and forget": Log result but don't propagate failures
        // Event processing failures don't affect the async queue operation
        Logger::debug("EventRaiserImpl: Event '{}' processed with result: {}", event.eventName, result);

    } catch (const std::exception &e) {
        Logger::error("EventRaiserImpl: Exception while processing event '{}': {}", event.eventName, e.what());
    }
}

}  // namespace RSM