#include "runtime/CoreEventProcessor.h"
#include "common/Logger.h"
#include <iomanip>
#include <random>
#include <sstream>

namespace SCXML {
namespace Events {

CoreEventProcessor::CoreEventProcessor()
    : state_(State::STOPPED), internalQueue_(std::make_unique<SCXML::Events::EventQueue>()),
      externalQueue_(std::make_unique<SCXML::Events::EventQueue>()) {
    Logger::info("CoreEventProcessor created");
}

CoreEventProcessor::~CoreEventProcessor() {
    stop();
    Logger::info("CoreEventProcessor destroyed");
}

// ========== Lifecycle Management ==========

bool CoreEventProcessor::start() {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != State::STOPPED) {
            Logger::warning("CoreEventProcessor::start() - Already started or starting");
            return false;
        }

        Logger::info("Starting SCXML Event Processor...");
        state_ = State::STARTING;

        // Start processing thread
        processingThread_ = std::make_unique<std::thread>(&CoreEventProcessor::processingLoop, this);
    }  // Release lock here before waiting

    try {
        // Wait for thread to start
        Logger::info("Waiting for processing thread to start...");
        if (!waitForState(State::RUNNING, 1000)) {
            Logger::error("CoreEventProcessor::start() - Failed to start within timeout");
            state_ = State::STOPPED;
            if (processingThread_ && processingThread_->joinable()) {
                processingThread_->join();
            }
            processingThread_.reset();
            return false;
        }

        Logger::info("SCXML Event Processor started successfully");
        return true;

    } catch (const std::exception &e) {
        Logger::error("CoreEventProcessor::start() - Exception: " + std::string(e.what()));
        state_ = State::STOPPED;
        return false;
    }
}

bool CoreEventProcessor::stop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == State::STOPPED) {
        return true;
    }

    Logger::info("Stopping SCXML Event Processor...");
    state_ = State::STOPPING;
    stateCondition_.notify_all();

    // Wait for processing thread to finish
    if (processingThread_ && processingThread_->joinable()) {
        mutex_.unlock();  // Unlock to allow thread to finish
        processingThread_->join();
        mutex_.lock();
    }

    processingThread_.reset();
    state_ = State::STOPPED;

    // Clear all queues
    internalQueue_->clear();
    externalQueue_->clear();

    // Clear delayed events
    {
        std::lock_guard<std::mutex> delayedLock(delayedEventsMutex_);
        delayedEvents_.clear();
    }

    Logger::info("SCXML Event Processor stopped");
    return true;
}

bool CoreEventProcessor::pause() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != State::RUNNING) {
        Logger::warning("CoreEventProcessor::pause() - Not running");
        return false;
    }

    Logger::info("Pausing SCXML Event Processor");
    state_ = State::PAUSED;
    return true;
}

bool CoreEventProcessor::resume() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != State::PAUSED) {
        Logger::warning("CoreEventProcessor::resume() - Not paused");
        return false;
    }

    Logger::info("Resuming SCXML Event Processor");
    state_ = State::RUNNING;
    stateCondition_.notify_all();
    return true;
}

bool CoreEventProcessor::isRunning() const {
    return state_ == State::RUNNING;
}

bool CoreEventProcessor::isPaused() const {
    return state_ == State::PAUSED;
}

// ========== Event Queue Operations ==========

bool CoreEventProcessor::sendEvent(const std::string &eventName, const std::string &data, const std::string &sendId) {
    if (eventName.empty()) {
        Logger::warning("CoreEventProcessor::sendEvent() - Empty event name");
        return false;
    }

    auto event = makeEvent(eventName, EventType::EXTERNAL, data, sendId);

    if (eventTracing_) {
        Logger::info("Sending external event: " + event->toString());
    }

    externalQueue_->enqueue(event, EventQueue::Priority::NORMAL);
    bool success = true;

    if (success) {
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.totalExternalEvents++;
    } else {
        Logger::warning("CoreEventProcessor::sendEvent() - Failed to enqueue event: " + eventName);
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.totalDroppedEvents++;
    }

    return success;
}

bool CoreEventProcessor::raiseEvent(const std::string &eventName, const std::string &data) {
    if (eventName.empty()) {
        Logger::warning("CoreEventProcessor::raiseEvent() - Empty event name");
        return false;
    }

    auto event = makeEvent(eventName, EventType::INTERNAL, data);

    if (eventTracing_) {
        Logger::info("Raising internal event: " + event->toString());
    }

    internalQueue_->enqueue(event, EventQueue::Priority::HIGH);
    bool success = true;

    if (success) {
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.totalInternalEvents++;
    } else {
        Logger::warning("CoreEventProcessor::raiseEvent() - Failed to enqueue event: " + eventName);
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.totalDroppedEvents++;
    }

    return success;
}

bool CoreEventProcessor::sendDelayedEvent(const std::string &eventName, const std::string &data, uint64_t delayMs,
                                          const std::string &sendId) {
    if (eventName.empty()) {
        Logger::warning("CoreEventProcessor::sendDelayedEvent() - Empty event name");
        return false;
    }

    auto event = makeEvent(eventName, EventType::EXTERNAL, data, sendId);
    auto executeTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);

    std::string actualSendId = sendId.empty() ? generateSendId() : sendId;

    {
        std::lock_guard<std::mutex> lock(delayedEventsMutex_);
        delayedEvents_.push_back({event, executeTime, actualSendId, false});

        // Keep delayed events sorted by execution time for efficient processing
        std::sort(delayedEvents_.begin(), delayedEvents_.end(),
                  [](const DelayedEventEntry &a, const DelayedEventEntry &b) { return a.executeTime < b.executeTime; });
    }

    if (eventTracing_) {
        Logger::info("Scheduled delayed event: " + event->toString() + " (delay: " + std::to_string(delayMs) +
                     "ms, sendId: " + actualSendId + ")");
    }

    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.totalDelayedEvents++;

    return true;
}

bool CoreEventProcessor::cancelEvent(const std::string &sendId) {
    if (sendId.empty()) {
        Logger::warning("CoreEventProcessor::cancelEvent() - Empty sendId");
        return false;
    }

    std::lock_guard<std::mutex> lock(delayedEventsMutex_);

    for (auto &entry : delayedEvents_) {
        if (entry.sendId == sendId && !entry.cancelled) {
            entry.cancelled = true;

            if (eventTracing_) {
                Logger::info("Cancelled delayed event with sendId: " + sendId);
            }

            std::lock_guard<std::mutex> statsLock(statisticsMutex_);
            statistics_.totalCancelledEvents++;
            return true;
        }
    }

    Logger::warning("CoreEventProcessor::cancelEvent() - Event not found: " + sendId);
    return false;
}

bool CoreEventProcessor::sendEventDirect(EventPtr event, EventQueue::Priority priority) {
    if (!event) {
        Logger::warning("CoreEventProcessor::sendEventDirect() - Null event");
        return false;
    }

    if (eventTracing_) {
        Logger::info("Sending direct event: " + event->toString());
    }

    bool success = true;  // EventQueue::enqueue() returns void, assume success
    if (priority == EventQueue::Priority::HIGH || event->getType() == EventType::INTERNAL) {
        internalQueue_->enqueue(event, EventQueue::Priority::HIGH);
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.totalInternalEvents++;
    } else {
        externalQueue_->enqueue(event, EventQueue::Priority::NORMAL);
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.totalExternalEvents++;
    }

    if (!success) {
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.totalDroppedEvents++;
    }

    return success;
}

// ========== Queue Status ==========

size_t CoreEventProcessor::getPendingEventCount() const {
    return getInternalEventCount() + getExternalEventCount();
}

size_t CoreEventProcessor::getInternalEventCount() const {
    return internalQueue_->size();
}

size_t CoreEventProcessor::getExternalEventCount() const {
    return externalQueue_->size();
}

void CoreEventProcessor::clearAllEvents() {
    internalQueue_->clear();
    externalQueue_->clear();

    std::lock_guard<std::mutex> lock(delayedEventsMutex_);
    delayedEvents_.clear();

    Logger::info("Cleared all events from queues");
}

void CoreEventProcessor::clearExternalEvents() {
    externalQueue_->clear();
    Logger::info("Cleared external events from queue");
}

// ========== Callbacks ==========

void CoreEventProcessor::setEventHandler(EventHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    eventHandler_ = std::move(handler);
    Logger::info("Event handler callback set");
}

void CoreEventProcessor::setStateChangeCallback(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    stateChangeCallback_ = std::move(callback);
    Logger::info("State change callback set");
}

void CoreEventProcessor::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    errorCallback_ = std::move(callback);
    Logger::info("Error callback set");
}

// ========== Configuration ==========

void CoreEventProcessor::setMaxQueueSizes(size_t internalMaxSize, size_t externalMaxSize) {
    // Note: Current EventQueue implementation doesn't support dynamic max size change
    // This would require extending EventQueue interface
    Logger::info("Queue size limits: internal=" + std::to_string(internalMaxSize) +
                 ", external=" + std::to_string(externalMaxSize));
}

void CoreEventProcessor::setEventTracing(bool enabled) {
    eventTracing_ = enabled;
    Logger::info("Event tracing " + std::string(enabled ? "enabled" : "disabled"));
}

bool CoreEventProcessor::isEventTracingEnabled() const {
    return eventTracing_;
}

// ========== Statistics ==========

CoreEventProcessor::Statistics CoreEventProcessor::getStatistics() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);

    // Update current queue sizes
    statistics_.totalEventsProcessed = statistics_.totalInternalEvents + statistics_.totalExternalEvents;

    // Calculate average processing time
    if (statistics_.totalEventsProcessed > 0) {
        statistics_.averageProcessingTime = statistics_.totalProcessingTime / statistics_.totalEventsProcessed;
    }

    return statistics_;
}

void CoreEventProcessor::resetStatistics() {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_ = Statistics{};
    Logger::info("Statistics reset");
}

// ========== Advanced Features ==========

void CoreEventProcessor::setEventFilter(EventQueue::EventFilter filter) {
    // TODO: EventQueue doesn't support setFilter() - need to implement filtering differently
    (void)filter;  // Suppress unused parameter warning
    Logger::warning("Event filtering not yet supported by EventQueue implementation");
}

bool CoreEventProcessor::processEventImmediate(EventPtr event) {
    if (!event) {
        return false;
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    (void)handleEvent(event);  // Suppress unused variable warning
    auto endTime = std::chrono::high_resolution_clock::now();

    auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    updateStatistics(processingTime);

    return true;  // Event processed successfully
}

size_t CoreEventProcessor::processAllEvents() {
    size_t processedCount = 0;

    while (processNextEvent()) {
        processedCount++;
    }

    processedCount += processDelayedEvents();

    return processedCount;
}

// ========== Internal Methods ==========

void CoreEventProcessor::processingLoop() {
    Logger::info("Event processing thread started");

    try {
        // Signal that we're running
        Logger::info("Setting state to RUNNING...");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::RUNNING;
            stateCondition_.notify_all();
        }
        Logger::info("State set to RUNNING, notified waiting threads");
    } catch (const std::exception &e) {
        Logger::error("Exception in processingLoop initialization: " + std::string(e.what()));
        return;
    }

    while (state_ != State::STOPPING) {
        try {
            // Process delayed events first
            processDelayedEvents();

            // Process next event from queues
            if (!processNextEvent()) {
                // No events to process, wait a bit
                std::unique_lock<std::mutex> lock(mutex_);
                if (state_ == State::PAUSED) {
                    // Wait while paused
                    stateCondition_.wait(lock, [this] { return state_ != State::PAUSED; });
                } else {
                    // Short wait to avoid busy loop
                    stateCondition_.wait_for(lock, std::chrono::milliseconds(1));
                }
            }

        } catch (const std::exception &e) {
            reportError("processing.exception", "Exception in processing loop: " + std::string(e.what()));

            // Brief pause after exception to prevent rapid error loops
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    Logger::info("Event processing thread finished");
}

bool CoreEventProcessor::processNextEvent() {
    EventPtr event = nullptr;

    // SCXML priority: Internal events (high priority) before external events
    // Try internal queue first
    std::shared_ptr<SCXML::Events::Event> internalEvent;
    if (internalQueue_->tryDequeue(internalEvent)) {
        event = internalEvent;
    } else {
        // Try external queue
        std::shared_ptr<SCXML::Events::Event> externalEvent;
        if (externalQueue_->tryDequeue(externalEvent)) {
            event = externalEvent;
        }
    }

    if (!event) {
        return false;  // No events available
    }

    // Process the event
    auto startTime = std::chrono::high_resolution_clock::now();
    (void)handleEvent(event);  // Suppress unused variable warning
    auto endTime = std::chrono::high_resolution_clock::now();

    auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    updateStatistics(processingTime);

    return true;
}

size_t CoreEventProcessor::processDelayedEvents() {
    size_t processedCount = 0;
    auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(delayedEventsMutex_);

    // Process events that are ready (sorted by time)
    auto it = delayedEvents_.begin();
    while (it != delayedEvents_.end() && it->executeTime <= now) {
        if (!it->cancelled) {
            // Move event to appropriate queue
            if (it->event->getType() == EventType::INTERNAL) {
                internalQueue_->enqueue(it->event, EventQueue::Priority::HIGH);
                processedCount++;
            } else {
                externalQueue_->enqueue(it->event, EventQueue::Priority::NORMAL);
                processedCount++;
            }

            if (eventTracing_) {
                Logger::info("Executed delayed event: " + it->event->toString());
            }
        }

        it = delayedEvents_.erase(it);
    }

    return processedCount;
}

bool CoreEventProcessor::handleEvent(const EventPtr &event) {
    if (!event) {
        return false;
    }

    if (eventTracing_) {
        Logger::info("Processing event: " + event->toString());
    }

    try {
        // Call user-defined event handler
        if (eventHandler_) {
            bool handled = eventHandler_(event);

            if (eventTracing_) {
                Logger::info("Event " + event->getName() + " " + (handled ? "handled successfully" : "not handled"));
            }

            return handled;
        } else {
            Logger::warning("No event handler set - event ignored: " + event->getName());
            return false;
        }

    } catch (const std::exception &e) {
        reportError("event.processing", "Exception processing event " + event->getName() + ": " + e.what(), event);
        return false;
    }
}

void CoreEventProcessor::reportError(const std::string &errorType, const std::string &message, const EventPtr &event) {
    Logger::error("CoreEventProcessor: " + errorType + " - " + message);

    {
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.totalErrorEvents++;
    }

    if (errorCallback_) {
        try {
            errorCallback_(errorType, message, event);
        } catch (const std::exception &e) {
            Logger::error("Exception in error callback: " + std::string(e.what()));
        }
    }
}

void CoreEventProcessor::updateStatistics(std::chrono::microseconds processingTime) {
    std::lock_guard<std::mutex> lock(statisticsMutex_);

    statistics_.totalProcessingTime += std::chrono::duration_cast<std::chrono::milliseconds>(processingTime);
    statistics_.totalEventsProcessed++;
}

std::string CoreEventProcessor::generateSendId() {
    static std::atomic<uint64_t> counter{0};
    static std::random_device rd;
    static std::mt19937 gen(rd());

    auto now = std::chrono::steady_clock::now();
    auto timestamp = now.time_since_epoch().count();
    auto sequence = counter.fetch_add(1);
    auto random = gen();

    std::ostringstream oss;
    oss << "send_" << std::hex << timestamp << "_" << sequence << "_" << random;
    return oss.str();
}

bool CoreEventProcessor::waitForState(State expectedState, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    return stateCondition_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                    [this, expectedState] { return state_ == expectedState; });
}

// ========== Factory Function ==========

std::unique_ptr<CoreEventProcessor> createEventProcessor() {
    return std::make_unique<CoreEventProcessor>();
}

}  // namespace Events
}  // namespace SCXML