#include "events/InternalEventTarget.h"
#include "common/Logger.h"
#include "runtime/EventRaiserImpl.h"
#include "runtime/IEventRaiser.h"
#include <future>
#include <sstream>

namespace RSM {

InternalEventTarget::InternalEventTarget(std::shared_ptr<IEventRaiser> eventRaiser, bool isExternal)
    : eventRaiser_(eventRaiser), isExternal_(isExternal) {}

std::future<SendResult> InternalEventTarget::send(const EventDescriptor &event) {
    LOG_DEBUG("InternalEventTarget::send() - ENTRY: event='{}', target='{}'", event.eventName, event.target);

    LOG_DEBUG("InternalEventTarget: Processing event - sessionId='{}', event='{}', isExternal={}", event.sessionId,
              event.eventName, isExternal_);

    std::promise<SendResult> promise;
    auto future = promise.get_future();

    try {
        // Validate event descriptor first
        auto validationErrors = event.validate();
        if (!validationErrors.empty()) {
            std::string errorMsg = "Event validation failed: ";
            for (const auto &error : validationErrors) {
                errorMsg += error + "; ";
            }
            promise.set_value(SendResult::error(errorMsg, SendResult::ErrorType::VALIDATION_ERROR));
            return future;
        }

        // Resolve event name (from expression if provided)
        std::string eventName = resolveEventName(event);
        if (eventName.empty()) {
            promise.set_value(
                SendResult::error("Failed to resolve event name", SendResult::ErrorType::VALIDATION_ERROR));
            return future;
        }

        // Build event data
        std::string eventData = buildEventData(event);

        // SCXML "fire and forget": Queue event and return immediate success
        // EventRaiser uses async processing, so queueing success = operation success
        // W3C SCXML compliance: Use appropriate priority based on target type
        auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
        bool queueSuccess = false;

        if (eventRaiserImpl) {
            // Use priority-aware method for W3C SCXML compliance
            auto priority =
                isExternal_ ? EventRaiserImpl::EventPriority::EXTERNAL : EventRaiserImpl::EventPriority::INTERNAL;
            LOG_DEBUG("InternalEventTarget::send() - Calling raiseEventWithPriority('{}', '{}', {})", eventName,
                      eventData, (isExternal_ ? "EXTERNAL" : "INTERNAL"));
            queueSuccess = eventRaiserImpl->raiseEventWithPriority(eventName, eventData, priority);
        } else {
            // Fallback to standard method for compatibility
            LOG_DEBUG("InternalEventTarget::send() - Calling eventRaiser_->raiseEvent('{}', '{}')", eventName,
                      eventData);
            queueSuccess = eventRaiser_->raiseEvent(eventName, eventData);
        }

        LOG_DEBUG("InternalEventTarget::send() - raiseEvent result: {}", queueSuccess);

        if (queueSuccess) {
            // Generate send ID for tracking (internal events get immediate IDs)
            std::string sendId = "internal_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                  std::chrono::steady_clock::now().time_since_epoch())
                                                                  .count());

            LOG_DEBUG("InternalEventTarget: Successfully sent internal event '{}' with sendId '{}'", eventName, sendId);
            promise.set_value(SendResult::success(sendId));
        } else {
            // Only fails if EventRaiser is not ready (shutdown, etc.)
            LOG_ERROR("InternalEventTarget: Failed to queue internal event '{}' - EventRaiser not ready", eventName);
            promise.set_value(
                SendResult::error("EventRaiser not ready for internal event", SendResult::ErrorType::INTERNAL_ERROR));
        }

    } catch (const std::exception &e) {
        LOG_ERROR("InternalEventTarget: Exception while sending event: {}", e.what());
        promise.set_value(
            SendResult::error("Exception: " + std::string(e.what()), SendResult::ErrorType::INTERNAL_ERROR));
    }

    return future;
}

std::string InternalEventTarget::getTargetType() const {
    return "internal";
}

bool InternalEventTarget::canHandle(const std::string &targetUri) const {
    return targetUri == "#_internal" || targetUri.empty() || targetUri == "_internal";
}

std::vector<std::string> InternalEventTarget::validate() const {
    std::vector<std::string> errors;

    if (!eventRaiser_) {
        errors.push_back("InternalEventTarget requires a valid EventRaiser");
    } else if (!eventRaiser_->isReady()) {
        errors.push_back("EventRaiser is not ready to handle events");
    }

    return errors;
}

std::string InternalEventTarget::getDebugInfo() const {
    std::ostringstream info;
    info << "InternalEventTarget{";
    info << "eventRaiser=" << (eventRaiser_ ? "valid" : "null");
    info << ", ready=" << (eventRaiser_ ? (eventRaiser_->isReady() ? "true" : "false") : "unknown");
    info << "}";
    return info.str();
}

std::string InternalEventTarget::resolveEventName(const EventDescriptor &event) const {
    // If eventExpr is provided, we would need to evaluate it through the ActionExecutor
    // For now, we'll support only literal event names
    if (!event.eventExpr.empty()) {
        LOG_WARN("InternalEventTarget: eventExpr not yet supported, using literal name");
    }

    return event.eventName;
}

std::string InternalEventTarget::buildEventData(const EventDescriptor &event) const {
    if (event.data.empty() && event.params.empty()) {
        return "";
    }

    // SCXML Compliance: "processor MUST reformat this data to match its data model,
    // but MUST NOT otherwise modify it"

    // For simple data without parameters, return data directly (SCXML compliant)
    if (!event.data.empty() && event.params.empty()) {
        return event.data;
    }

    // For complex data with parameters, build structured data
    std::ostringstream dataBuilder;
    dataBuilder << "{";

    bool first = true;

    // Add main data if provided
    if (!event.data.empty()) {
        dataBuilder << "\"data\": \"" << event.data << "\"";
        first = false;
    }

    // Add parameters
    for (const auto &param : event.params) {
        if (!first) {
            dataBuilder << ", ";
        }
        dataBuilder << "\"" << param.first << "\": \"" << param.second << "\"";
        first = false;
    }

    dataBuilder << "}";
    return dataBuilder.str();
}

}  // namespace RSM