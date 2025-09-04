#include "runtime/PlatformEventHandler.h"
#include "common/Logger.h"
#include "common/RuntimeContext.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace SCXML {
namespace Runtime {

// PlatformEventManager Implementation
PlatformEventManager::PlatformEventManager(size_t maxHistorySize)
    : loggingEnabled_(true), maxHistorySize_(maxHistorySize) {
    eventHistory_.reserve(maxHistorySize_);
}

SCXML::Common::Result<void> PlatformEventManager::registerHandler(PlatformEventType type,
                                                                  PlatformEventHandler handler) {
    if (!handler) {
        return SCXML::Common::Result<void>::error("Handler cannot be null");
    }

    try {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        handlers_[type].push_back(std::move(handler));

        SCXML::Common::Logger::info("PlatformEventManager", "Registered handler for platform event type: " +
                                                                std::to_string(static_cast<int>(type)));

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to register handler: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> PlatformEventManager::fireEvent(const PlatformEvent &event) {
    try {
        // Add to history first
        addToHistory(event);

        // Get handlers for this event type
        std::vector<PlatformEventHandler> handlersToCall;
        {
            std::lock_guard<std::mutex> lock(handlerMutex_);
            auto it = handlers_.find(event.type);
            if (it != handlers_.end()) {
                handlersToCall = it->second;
            }
        }

        // Call all handlers
        bool hasErrors = false;
        std::string errorMessages;

        for (const auto &handler : handlersToCall) {
            try {
                auto result = handler(event);
                if (!result.isSuccess()) {
                    hasErrors = true;
                    if (!errorMessages.empty()) {
                        errorMessages += "; ";
                    }
                    errorMessages += result.getError();
                }
            } catch (const std::exception &e) {
                hasErrors = true;
                if (!errorMessages.empty()) {
                    errorMessages += "; ";
                }
                errorMessages += std::string("Handler exception: ") + e.what();
            }
        }

        if (hasErrors) {
            return SCXML::Common::Result<void>::error("Some handlers failed: " + errorMessages);
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to fire event: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> PlatformEventManager::fireErrorEvent(const std::string &errorType,
                                                                 const std::string &errorMessage,
                                                                 const std::string &errorCode,
                                                                 const std::string &sourceLocation) {
    try {
        PlatformEventType type = getErrorEventType(errorType);
        std::string eventName = generateEventName(type, "");

        PlatformEvent event(type, eventName);
        event.errorMessage = errorMessage;
        event.errorCode = errorCode;
        event.sourceLocation = sourceLocation;

        // Create event data
        event.eventData["error"] = errorMessage;
        if (!errorCode.empty()) {
            event.eventData["code"] = errorCode;
        }
        if (!sourceLocation.empty()) {
            event.eventData["location"] = sourceLocation;
        }

        return fireEvent(event);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to fire error event: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> PlatformEventManager::fireDoneStateEvent(const std::string &stateId,
                                                                     const std::string &eventData) {
    try {
        std::string eventName = generateEventName(PlatformEventType::DONE_STATE, stateId);
        PlatformEvent event(PlatformEventType::DONE_STATE, eventName, stateId, eventData);

        return fireEvent(event);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to fire done state event: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> PlatformEventManager::fireDoneInvokeEvent(const std::string &invokeId,
                                                                      const std::string &eventData) {
    try {
        std::string eventName = generateEventName(PlatformEventType::DONE_INVOKE, invokeId);
        PlatformEvent event(PlatformEventType::DONE_INVOKE, eventName, invokeId, eventData);

        return fireEvent(event);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to fire done invoke event: " + std::string(e.what()));
    }
}

std::vector<PlatformEvent> PlatformEventManager::getEventHistory(size_t maxEvents) const {
    std::lock_guard<std::mutex> lock(historyMutex_);

    if (eventHistory_.size() <= maxEvents) {
        return eventHistory_;
    }

    // Return the most recent events
    std::vector<PlatformEvent> result;
    size_t start = eventHistory_.size() - maxEvents;
    result.assign(eventHistory_.begin() + start, eventHistory_.end());

    return result;
}

void PlatformEventManager::clearHistory() {
    std::lock_guard<std::mutex> lock(historyMutex_);
    eventHistory_.clear();

    SCXML::Common::Logger::info("PlatformEventManager", "Event history cleared");
}

void PlatformEventManager::setLoggingEnabled(bool enabled) {
    loggingEnabled_ = enabled;

    SCXML::Common::Logger::info("PlatformEventManager",
                                "Event logging " + std::string(enabled ? "enabled" : "disabled"));
}

void PlatformEventManager::addToHistory(const PlatformEvent &event) {
    if (!loggingEnabled_) {
        return;
    }

    std::lock_guard<std::mutex> lock(historyMutex_);

    // Maintain maximum history size
    if (eventHistory_.size() >= maxHistorySize_) {
        // Remove oldest events (from the beginning)
        size_t removeCount = eventHistory_.size() - maxHistorySize_ + 1;
        eventHistory_.erase(eventHistory_.begin(), eventHistory_.begin() + removeCount);
    }

    eventHistory_.push_back(event);
}

PlatformEventType PlatformEventManager::getErrorEventType(const std::string &errorType) {
    if (errorType == "execution") {
        return PlatformEventType::ERROR_EXECUTION;
    } else if (errorType == "communication") {
        return PlatformEventType::ERROR_COMMUNICATION;
    } else if (errorType == "platform") {
        return PlatformEventType::ERROR_PLATFORM;
    } else {
        return PlatformEventType::ERROR_EXECUTION;  // Default to execution error
    }
}

std::string PlatformEventManager::generateEventName(PlatformEventType type, const std::string &target) {
    switch (type) {
    case PlatformEventType::ERROR_EXECUTION:
        return "error.execution";
    case PlatformEventType::ERROR_COMMUNICATION:
        return "error.communication";
    case PlatformEventType::ERROR_PLATFORM:
        return "error.platform";
    case PlatformEventType::DONE_STATE:
        return target.empty() ? "done.state" : "done.state." + target;
    case PlatformEventType::DONE_INVOKE:
        return target.empty() ? "done.invoke" : "done.invoke." + target;
    case PlatformEventType::DONE_DATA:
        return "done.data";
    default:
        return "unknown.event";
    }
}

// ErrorEventFactory Implementation
PlatformEvent ErrorEventFactory::createExecutionError(const std::string &message, const std::string &location) {
    PlatformEvent event(PlatformEventType::ERROR_EXECUTION, "error.execution");
    event.errorMessage = message;
    event.sourceLocation = location;
    event.eventData["error"] = message;
    if (!location.empty()) {
        event.eventData["location"] = location;
    }
    return event;
}

PlatformEvent ErrorEventFactory::createCommunicationError(const std::string &message, const std::string &target) {
    PlatformEvent event(PlatformEventType::ERROR_COMMUNICATION, "error.communication");
    event.errorMessage = message;
    event.targetId = target;
    event.eventData["error"] = message;
    if (!target.empty()) {
        event.eventData["target"] = target;
    }
    return event;
}

PlatformEvent ErrorEventFactory::createPlatformError(const std::string &message, const std::string &errorCode) {
    PlatformEvent event(PlatformEventType::ERROR_PLATFORM, "error.platform");
    event.errorMessage = message;
    event.errorCode = errorCode;
    event.eventData["error"] = message;
    if (!errorCode.empty()) {
        event.eventData["code"] = errorCode;
    }
    return event;
}

PlatformEvent ErrorEventFactory::createDoneStateEvent(const std::string &stateId, const nlohmann::json &data) {
    std::string eventName = stateId.empty() ? "done.state" : "done.state." + stateId;
    PlatformEvent event(PlatformEventType::DONE_STATE, eventName, stateId);
    event.eventData = data;
    return event;
}

PlatformEvent ErrorEventFactory::createDoneInvokeEvent(const std::string &invokeId, const nlohmann::json &data) {
    std::string eventName = invokeId.empty() ? "done.invoke" : "done.invoke." + invokeId;
    PlatformEvent event(PlatformEventType::DONE_INVOKE, eventName, invokeId);
    event.eventData = data;
    return event;
}

// PlatformEventIntegrator Implementation
SCXML::Common::Result<void> PlatformEventIntegrator::installStandardHandlers(SCXML::Runtime::RuntimeContext &context,
                                                                             IPlatformEventManager &eventManager) {
    try {
        // Install event queue handler for all platform events
        auto queueHandler = createEventQueueHandler(context);

        auto result1 = eventManager.registerHandler(PlatformEventType::ERROR_EXECUTION, queueHandler);
        auto result2 = eventManager.registerHandler(PlatformEventType::ERROR_COMMUNICATION, queueHandler);
        auto result3 = eventManager.registerHandler(PlatformEventType::ERROR_PLATFORM, queueHandler);
        auto result4 = eventManager.registerHandler(PlatformEventType::DONE_STATE, queueHandler);
        auto result5 = eventManager.registerHandler(PlatformEventType::DONE_INVOKE, queueHandler);

        // Check all results
        if (!result1.isSuccess()) {
            return result1;
        }
        if (!result2.isSuccess()) {
            return result2;
        }
        if (!result3.isSuccess()) {
            return result3;
        }
        if (!result4.isSuccess()) {
            return result4;
        }
        if (!result5.isSuccess()) {
            return result5;
        }

        // Install logging handler
        auto loggingHandler = createLoggingHandler("SCXML-Platform");

        eventManager.registerHandler(PlatformEventType::ERROR_EXECUTION, loggingHandler);
        eventManager.registerHandler(PlatformEventType::ERROR_COMMUNICATION, loggingHandler);
        eventManager.registerHandler(PlatformEventType::ERROR_PLATFORM, loggingHandler);
        eventManager.registerHandler(PlatformEventType::DONE_STATE, loggingHandler);
        eventManager.registerHandler(PlatformEventType::DONE_INVOKE, loggingHandler);

        SCXML::Common::Logger::info("PlatformEventIntegrator", "Standard handlers installed successfully");
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to install standard handlers: " + std::string(e.what()));
    }
}

PlatformEventHandler PlatformEventIntegrator::createEventQueueHandler(SCXML::Runtime::RuntimeContext &context) {
    return [&context](const PlatformEvent &event) -> SCXML::Common::Result<void> {
        try {
            // Convert platform event to SCXML internal event
            std::string eventData = event.eventData.empty() ? "{}" : event.eventData.dump();

            // Queue the event in the runtime context
            context.getEventQueue().enqueueInternalEvent(event.eventName, eventData);

            return SCXML::Common::Result<void>::success();

        } catch (const std::exception &e) {
            return SCXML::Common::Result<void>::error("Failed to queue event: " + std::string(e.what()));
        }
    };
}

PlatformEventHandler PlatformEventIntegrator::createLoggingHandler(const std::string &logPrefix) {
    return [logPrefix](const PlatformEvent &event) -> SCXML::Common::Result<void> {
        try {
            std::ostringstream oss;
            oss << "Event: " << event.eventName;

            if (!event.targetId.empty()) {
                oss << " (target: " << event.targetId << ")";
            }

            if (!event.errorMessage.empty()) {
                oss << " - " << event.errorMessage;
            }

            if (!event.errorCode.empty()) {
                oss << " [code: " << event.errorCode << "]";
            }

            if (!event.sourceLocation.empty()) {
                oss << " at " << event.sourceLocation;
            }

            // Log at appropriate level based on event type
            switch (event.type) {
            case PlatformEventType::ERROR_EXECUTION:
            case PlatformEventType::ERROR_COMMUNICATION:
            case PlatformEventType::ERROR_PLATFORM:
                SCXML::Common::Logger::error(logPrefix, oss.str());
                break;
            default:
                SCXML::Common::Logger::info(logPrefix, oss.str());
                break;
            }

            return SCXML::Common::Result<void>::success();

        } catch (const std::exception &e) {
            return SCXML::Common::Result<void>::error("Logging failed: " + std::string(e.what()));
        }
    };
}

}  // namespace Runtime
}  // namespace SCXML