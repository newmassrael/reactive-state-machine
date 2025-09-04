#include "runtime/EventSystemBridge.h"
#include "common/Logger.h"
#include "common/GracefulJoin.h"
#include "runtime/EventSystemBridge.h"
#include "runtime/RuntimeContext.h"

#include <chrono>
#include <thread>

using namespace SCXML::Events;

namespace SCXML {
namespace Runtime {

EventSystemBridge::EventSystemBridge()
    : runtimeContext_(nullptr), initialized_(false), processingActive_(false), shouldStop_(false),
      totalProcessedEvents_(0), totalDroppedEvents_(0), sendIdCounter_(0) {
    SCXML::Common::Logger::debug("EventSystemBridge::Constructor - Creating event system bridge (no context)");
}

EventSystemBridge::EventSystemBridge(std::shared_ptr<RuntimeContext> context)
    : runtimeContext_(context), initialized_(false), processingActive_(false), shouldStop_(false),
      totalProcessedEvents_(0), totalDroppedEvents_(0), sendIdCounter_(0) {
    SCXML::Common::Logger::debug("EventSystemBridge::Constructor - Creating event system bridge");
}

EventSystemBridge::~EventSystemBridge() {
    SCXML::Common::Logger::debug("EventSystemBridge::Destructor - Shutting down event system bridge");
    shutdown();
}

bool EventSystemBridge::initialize() {
    SCXML::Common::Logger::debug("EventSystemBridge::initialize - Initializing event system integration");

    if (initialized_) {
        SCXML::Common::Logger::warning("EventSystemBridge::initialize - Already initialized");
        return true;
    }

    try {
        // Create event queues according to SCXML specification
        // Internal events have higher priority
        internalQueue_ = std::make_shared<EventQueue>();  // Internal events
        externalQueue_ = std::make_shared<EventQueue>();  // External events

        // Create event dispatcher
        dispatcher_ = std::make_shared<EventDispatcher>();

        // EventDispatcher is ready to use after construction
        SCXML::Common::Logger::debug("EventSystemBridge::initialize - Event dispatcher created successfully");

        initialized_ = true;
        SCXML::Common::Logger::info("EventSystemBridge::initialize - Event system integration initialized successfully");
        return true;

    } catch (const std::exception &ex) {
        SCXML::Common::Logger::error("EventSystemBridge::initialize - Exception during initialization: " + std::string(ex.what()));
        return false;
    }
}

void EventSystemBridge::shutdown() {
    SCXML::Common::Logger::debug("EventSystemBridge::shutdown - Shutting down event system");

    if (!initialized_) {
        return;
    }

    // Stop event processing
    stopEventProcessing();

    // Clear queues
    if (internalQueue_) {
        internalQueue_->clear();
    }
    if (externalQueue_) {
        externalQueue_->clear();
    }

    // Clear dispatcher handlers
    if (dispatcher_) {
        auto targets = dispatcher_->getRegisteredTargets();
        for (const auto &target : targets) {
            dispatcher_->unregisterHandler(target);
        }
    }

    initialized_ = false;
    SCXML::Common::Logger::info("EventSystemBridge::shutdown - Event system shutdown complete");
}

bool EventSystemBridge::isInitialized() const {
    return initialized_;
}

void EventSystemBridge::setRuntimeContext(std::shared_ptr<RuntimeContext> context) {
    SCXML::Common::Logger::debug("EventSystemBridge::setRuntimeContext - Setting runtime context");
    runtimeContext_ = context;
}

bool EventSystemBridge::sendEvent(const std::string &eventName, const EventData &data, const std::string &sendId) {
    if (!initialized_) {
        SCXML::Common::Logger::warning("EventSystemBridge::sendEvent - Event system not initialized");
        return false;
    }

    SCXML::Common::Logger::debug("EventSystemBridge::sendEvent - Sending external event: " + eventName);

    try {
        // Create external event
        auto event = makeEvent(eventName, EventType::EXTERNAL, data, sendId);

        // Add to external queue (normal priority)
        externalQueue_->enqueue(event);
        bool success = true;

        if (!success) {
            SCXML::Common::Logger::warning("EventSystemBridge::sendEvent - Failed to enqueue external event: " + eventName);
            std::lock_guard<std::mutex> lock(statsMutex_);
            totalDroppedEvents_++;
        } else {
            SCXML::Common::Logger::debug("EventSystemBridge::sendEvent - External event queued successfully: " + eventName);
        }

        return success;

    } catch (const std::exception &ex) {
        SCXML::Common::Logger::error("EventSystemBridge::sendEvent - Exception sending event: " + std::string(ex.what()));
        return false;
    }
}

bool EventSystemBridge::raiseEvent(const std::string &eventName, const EventData &data) {
    if (!initialized_) {
        SCXML::Common::Logger::warning("EventSystemBridge::raiseEvent - Event system not initialized");
        return false;
    }

    SCXML::Common::Logger::debug("EventSystemBridge::raiseEvent - Raising internal event: " + eventName);

    try {
        // Create internal event
        auto event = makeEvent(eventName, EventType::INTERNAL, data);

        // Add to internal queue (high priority)
        internalQueue_->enqueue(event);
        bool success = true;

        if (!success) {
            SCXML::Common::Logger::warning("EventSystemBridge::raiseEvent - Failed to enqueue internal event: " + eventName);
            std::lock_guard<std::mutex> lock(statsMutex_);
            totalDroppedEvents_++;
        } else {
            SCXML::Common::Logger::debug("EventSystemBridge::raiseEvent - Internal event queued successfully: " + eventName);
        }

        return success;

    } catch (const std::exception &ex) {
        SCXML::Common::Logger::error("EventSystemBridge::raiseEvent - Exception raising event: " + std::string(ex.what()));
        return false;
    }
}

bool EventSystemBridge::sendDelayedEvent(const std::string &eventName, int delayMs, const EventData &data,
                                         const std::string &sendId) {
    if (!initialized_) {
        SCXML::Common::Logger::warning("EventSystemBridge::sendDelayedEvent - Event system not initialized");
        return false;
    }

    SCXML::Common::Logger::debug("EventSystemBridge::sendDelayedEvent - Scheduling delayed event: " + eventName +
                  " (delay: " + std::to_string(delayMs) + "ms)");

    try {
        // Create delayed event
        auto event = makeEvent(eventName, EventType::EXTERNAL, data, sendId);

        // Schedule delayed event using timer
        if (delayMs <= 0) {
            // No delay - send immediately
            return sendEvent(eventName, data, sendId);
        }

        // Store the event with its timer for cancellation
        std::string actualSendId = sendId.empty() ? generateSendId() : sendId;
        // event->setSendId(actualSendId); // Event class doesn't have setSendId method

        // Create timer for delayed execution
        auto timer = std::make_shared<std::thread>([this, event, delayMs, actualSendId]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

            // Check if event was cancelled
            {
                std::lock_guard<std::mutex> lock(delayedEventsMutex_);
                auto it = delayedEvents_.find(actualSendId);
                if (it == delayedEvents_.end()) {
                    // Event was cancelled
                    return;
                }
                // Remove from delayed events map
                delayedEvents_.erase(it);
            }

            // Execute the delayed event
            if (internalQueue_) {
                internalQueue_->enqueue(event, SCXML::Events::EventQueue::Priority::NORMAL);
                SCXML::Common::Logger::debug("EventSystemBridge::sendDelayedEvent - Executed delayed event: " + event->getName());
            }
        });

        // Store timer for potential cancellation
        {
            std::lock_guard<std::mutex> lock(delayedEventsMutex_);
            delayedEvents_[actualSendId] = {timer, event};
        }

        SCXML::Common::Logger::debug("EventSystemBridge::sendDelayedEvent - Scheduled delayed event: " + eventName + " with delay " +
                      std::to_string(delayMs) + "ms, sendId: " + actualSendId);
        return true;

    } catch (const std::exception &ex) {
        SCXML::Common::Logger::error("EventSystemBridge::sendDelayedEvent - Exception scheduling delayed event: " +
                      std::string(ex.what()));
        return false;
    }
}

bool EventSystemBridge::cancelEvent(const std::string &sendId) {
    if (!initialized_) {
        SCXML::Common::Logger::warning("EventSystemBridge::cancelEvent - Event system not initialized");
        return false;
    }

    SCXML::Common::Logger::debug("EventSystemBridge::cancelEvent - Cancelling event with sendId: " + sendId);

    try {
        std::lock_guard<std::mutex> lock(delayedEventsMutex_);
        auto it = delayedEvents_.find(sendId);

        if (it != delayedEvents_.end()) {
            // Event found - cancel it
            // The timer thread will check if the event still exists
            // Removing it from the map will prevent execution
            delayedEvents_.erase(it);

            SCXML::Common::Logger::debug("EventSystemBridge::cancelEvent - Successfully cancelled delayed event: " + sendId);
            return true;
        } else {
            // Event not found - might have already executed or was never scheduled
            SCXML::Common::Logger::debug("EventSystemBridge::cancelEvent - Event not found or already executed: " + sendId);
            return false;
        }

    } catch (const std::exception &ex) {
        SCXML::Common::Logger::error("EventSystemBridge::cancelEvent - Exception cancelling event: " + std::string(ex.what()));
        return false;
    }
}

bool EventSystemBridge::processNextEvent(int timeoutMs) {
    (void)timeoutMs;  // Mark as intentionally unused
    if (!initialized_) {
        SCXML::Common::Logger::warning("EventSystemBridge::processNextEvent - Event system not initialized");
        return false;
    }

    try {
        EventPtr nextEvent = nullptr;

        // SCXML specification: Internal events have priority over external events
        // Try internal queue first
        EventPtr internalEvent = nullptr;
        if (internalQueue_->tryDequeue(internalEvent)) {
            nextEvent = internalEvent;
        } else {
            // Try external queue
            nextEvent = externalQueue_->dequeue();  // Block until event available
        }

        if (nextEvent) {
            SCXML::Common::Logger::debug("EventSystemBridge::processNextEvent - Processing event: " + nextEvent->getName());
            processEvent(nextEvent);

            std::lock_guard<std::mutex> lock(statsMutex_);
            totalProcessedEvents_++;

            return true;
        } else {
            SCXML::Common::Logger::debug("EventSystemBridge::processNextEvent - No events available within timeout");
            return false;
        }

    } catch (const std::exception &ex) {
        SCXML::Common::Logger::error("EventSystemBridge::processNextEvent - Exception processing event: " + std::string(ex.what()));
        return false;
    }
}

bool EventSystemBridge::startEventProcessing(EventProcessor processor) {
    if (!initialized_) {
        SCXML::Common::Logger::warning("EventSystemBridge::startEventProcessing - Event system not initialized");
        return false;
    }

    if (processingActive_) {
        SCXML::Common::Logger::warning("EventSystemBridge::startEventProcessing - Event processing already active");
        return true;
    }

    SCXML::Common::Logger::info("EventSystemBridge::startEventProcessing - Starting continuous event processing");

    eventProcessor_ = processor;
    processingActive_ = true;
    shouldStop_ = false;

    // Start processing thread
    processingThread_ = std::thread(&EventSystemBridge::eventProcessingLoop, this);

    return true;
}

void EventSystemBridge::stopEventProcessing() {
    if (!processingActive_) {
        return;
    }

    SCXML::Common::Logger::info("EventSystemBridge::stopEventProcessing - Stopping event processing");

    shouldStop_ = true;
    processingActive_ = false;

    // Wait for thread to finish
    if (processingThread_.joinable()) {
        SCXML::Common::GracefulJoin::joinWithTimeout(processingThread_, 3, "EventSystemBridge_Processing");
    }

    SCXML::Common::Logger::info("EventSystemBridge::stopEventProcessing - Event processing stopped");
}

bool EventSystemBridge::isProcessingEvents() const {
    return processingActive_;
}

void EventSystemBridge::eventProcessingLoop() {
    SCXML::Common::Logger::debug("EventSystemBridge::eventProcessingLoop - Event processing thread started");

    while (!shouldStop_) {
        try {
            bool processed = processNextEvent(100);  // 100ms timeout

            if (!processed) {
                // No events, short sleep to avoid busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

        } catch (const std::exception &ex) {
            SCXML::Common::Logger::error("EventSystemBridge::eventProcessingLoop - Exception in processing loop: " +
                          std::string(ex.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    SCXML::Common::Logger::debug("EventSystemBridge::eventProcessingLoop - Event processing thread ended");
}

void EventSystemBridge::processEvent(EventPtr event) {
    if (!event) {
        SCXML::Common::Logger::warning("EventSystemBridge::processEvent - Null event received");
        return;
    }

    SCXML::Common::Logger::debug("EventSystemBridge::processEvent - Processing event: " + event->getName() +
                  " (type: " + std::to_string(static_cast<int>(event->getType())) + ")");

    try {
        // Call the registered event processor
        if (eventProcessor_) {
            eventProcessor_(event);
        } else {
            SCXML::Common::Logger::warning("EventSystemBridge::processEvent - No event processor registered");
        }

        // Update runtime context with current event (only if context is available)
        if (runtimeContext_) {
            // Note: This requires proper include of RuntimeContext.h
            SCXML::Common::Logger::debug("EventSystemBridge::processEvent - Updating runtime context with current event");

            // Update runtime context with current event
            // Convert EventData variant to string
            std::string eventDataStr = std::visit([](const auto& v) -> std::string {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    return "";
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return v;
                } else if constexpr (std::is_same_v<T, int>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, double>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, bool>) {
                    return v ? "true" : "false";
                }
                return "";
            }, event->getData());
            runtimeContext_->setCurrentEvent(event->getName(), eventDataStr);
            // runtimeContext_->updateEventProcessingStats(); // Method not available

            // Mark event as processed in context
            // runtimeContext_->recordProcessedEvent(event->getName()); // Method not available
        }

    } catch (const std::exception &ex) {
        SCXML::Common::Logger::error("EventSystemBridge::processEvent - Exception processing event " + event->getName() + ": " +
                      std::string(ex.what()));
    }
}

std::shared_ptr<EventQueue> EventSystemBridge::getInternalQueue() const {
    return internalQueue_;
}

std::shared_ptr<EventQueue> EventSystemBridge::getExternalQueue() const {
    return externalQueue_;
}

std::shared_ptr<EventDispatcher> EventSystemBridge::getDispatcher() const {
    return dispatcher_;
}

EventSystemBridge::QueueStats EventSystemBridge::getQueueStats() const {
    QueueStats stats;

    if (internalQueue_) {
        stats.internalQueueSize = internalQueue_->size();
    }
    if (externalQueue_) {
        stats.externalQueueSize = externalQueue_->size();
    }

    std::lock_guard<std::mutex> lock(statsMutex_);
    stats.totalProcessed = totalProcessedEvents_;
    stats.totalDropped = totalDroppedEvents_;

    return stats;
}

void EventSystemBridge::clearAllEvents() {
    SCXML::Common::Logger::debug("EventSystemBridge::clearAllEvents - Clearing all queued events");

    if (internalQueue_) {
        internalQueue_->clear();
    }
    if (externalQueue_) {
        externalQueue_->clear();
    }

    SCXML::Common::Logger::info("EventSystemBridge::clearAllEvents - All events cleared");
}

void EventSystemBridge::setQueueLimits(size_t internalMaxSize, size_t externalMaxSize) {
    SCXML::Common::Logger::debug("EventSystemBridge::setQueueLimits - Setting queue limits: internal=" +
                  std::to_string(internalMaxSize) + ", external=" + std::to_string(externalMaxSize));

    // Note: Current EventQueue doesn't support runtime size changes
    // This would require extending the EventQueue interface
    SCXML::Common::Logger::warning("EventSystemBridge::setQueueLimits - Runtime queue limit changes not yet implemented");
}

bool EventSystemBridge::raiseDoneEvent(const std::string &stateId, const EventData &data) {
    std::string doneEventName = "done.state." + stateId;
    SCXML::Common::Logger::debug("EventSystemBridge::raiseDoneEvent - Raising done event for state: " + stateId);

    return raiseEvent(doneEventName, data);
}

bool EventSystemBridge::raiseErrorEvent(const std::string &errorType, const std::string &errorMessage,
                                        const EventData &data) {
    std::string errorEventName = "error." + errorType;
    SCXML::Common::Logger::debug("EventSystemBridge::raiseErrorEvent - Raising error event: " + errorEventName + " (" + errorMessage +
                  ")");

    // Add error message to event data if data is empty
    EventData errorData = data;
    if (std::holds_alternative<std::monostate>(data)) {
        errorData = errorMessage;
    }

    return raiseEvent(errorEventName, errorData);
}

std::shared_ptr<EventQueue> EventSystemBridge::selectQueue(EventType eventType) {
    switch (eventType) {
    case EventType::INTERNAL:
    case EventType::PLATFORM:
        return internalQueue_;
    case EventType::EXTERNAL:
    default:
        return externalQueue_;
    }
}

std::string EventSystemBridge::generateSendId() {
    size_t id = sendIdCounter_.fetch_add(1);
    return "send_" + std::to_string(id) + "_" +
           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

}  // namespace Runtime
}  // namespace SCXML