#include "events/InvokeEventTarget.h"
#include "common/Logger.h"
#include "events/EventRaiserService.h"
#include "runtime/IEventRaiser.h"
#include "scripting/JSEngine.h"
#include <sstream>

namespace RSM {

InvokeEventTarget::InvokeEventTarget(const std::string &invokeId, const std::string &parentSessionId)
    : invokeId_(invokeId), parentSessionId_(parentSessionId) {
    if (invokeId_.empty()) {
        throw std::invalid_argument("InvokeEventTarget: Invoke ID cannot be empty");
    }

    if (parentSessionId_.empty()) {
        throw std::invalid_argument("InvokeEventTarget: Parent session ID cannot be empty");
    }

    LOG_DEBUG("InvokeEventTarget: Created for invoke ID '{}' from parent session '{}'", invokeId_, parentSessionId_);
}

std::future<SendResult> InvokeEventTarget::send(const EventDescriptor &event) {
    LOG_DEBUG("InvokeEventTarget::send() - ENTRY: event='{}', target='{}', invokeId='{}'", event.eventName,
              event.target, invokeId_);

    std::promise<SendResult> resultPromise;
    auto resultFuture = resultPromise.get_future();

    try {
        // Find child session ID using JSEngine invoke mapping
        std::string childSessionId = JSEngine::instance().getInvokeSessionId(parentSessionId_, invokeId_);
        if (childSessionId.empty()) {
            LOG_ERROR("InvokeEventTarget: No child session found for invoke ID '{}' in parent '{}'", invokeId_,
                      parentSessionId_);
            resultPromise.set_value(SendResult::error("No child session found for invoke ID: " + invokeId_,
                                                      SendResult::ErrorType::TARGET_NOT_FOUND));
            return resultFuture;
        }

        LOG_DEBUG("InvokeEventTarget: Found child session '{}' for invoke ID '{}'", childSessionId, invokeId_);

        // Get EventRaiser for child session from centralized service
        auto eventRaiser = EventRaiserService::getInstance().getEventRaiser(childSessionId);
        if (!eventRaiser) {
            LOG_ERROR("InvokeEventTarget: No EventRaiser found for child session '{}'", childSessionId);
            resultPromise.set_value(SendResult::error("No EventRaiser found for child session: " + childSessionId,
                                                      SendResult::ErrorType::TARGET_NOT_FOUND));
            return resultFuture;
        }

        LOG_DEBUG("InvokeEventTarget: Routing event '{}' to child session '{}' via invoke ID '{}'", event.eventName,
                  childSessionId, invokeId_);

        // Prepare event data
        std::string eventName = event.eventName;
        std::string eventData = event.data;

        // Add parameters to event data if present
        if (!event.params.empty()) {
            std::ostringstream dataStream;
            dataStream << eventData;
            for (const auto &param : event.params) {
                dataStream << " " << param.first << "=" << param.second;
            }
            eventData = dataStream.str();
        }

        // Raise event in child session's external queue (W3C SCXML compliance)
        LOG_DEBUG("InvokeEventTarget::send() - Calling eventRaiser->raiseEvent('{}', '{}')", eventName, eventData);
        bool raiseResult = eventRaiser->raiseEvent(eventName, eventData);
        LOG_DEBUG("InvokeEventTarget::send() - eventRaiser->raiseEvent() returned: {}", raiseResult);

        if (!raiseResult) {
            LOG_WARN("InvokeEventTarget: Failed to raise event '{}' in child session '{}'", eventName, childSessionId);
            resultPromise.set_value(
                SendResult::error("Failed to raise event in child session", SendResult::ErrorType::INTERNAL_ERROR));
        } else {
            LOG_DEBUG("InvokeEventTarget: Successfully routed event '{}' to child session '{}'", eventName,
                      childSessionId);
            resultPromise.set_value(SendResult::success(event.sendId));
        }

    } catch (const std::exception &e) {
        LOG_ERROR("InvokeEventTarget: Error sending event to invoke: {}", e.what());
        resultPromise.set_value(SendResult::error("Failed to send event to invoke: " + std::string(e.what()),
                                                  SendResult::ErrorType::INTERNAL_ERROR));
    }

    return resultFuture;
}

std::string InvokeEventTarget::getTargetType() const {
    return "invoke";
}

bool InvokeEventTarget::canHandle(const std::string &targetUri) const {
    // Check if target matches #_<invokeId> pattern
    if (targetUri.length() > 2 && targetUri.substr(0, 2) == "#_") {
        std::string candidateInvokeId = targetUri.substr(2);
        return candidateInvokeId == invokeId_;
    }
    return false;
}

std::vector<std::string> InvokeEventTarget::validate() const {
    std::vector<std::string> errors;

    if (invokeId_.empty()) {
        errors.push_back("Invoke ID cannot be empty");
    }

    if (parentSessionId_.empty()) {
        errors.push_back("Parent session ID cannot be empty");
    }

    // Check if child session exists
    std::string childSessionId = JSEngine::instance().getInvokeSessionId(parentSessionId_, invokeId_);
    if (childSessionId.empty()) {
        errors.push_back("No child session found for invoke ID: " + invokeId_);
    } else {
        // Check if EventRaiser exists for child session
        auto eventRaiser = EventRaiserService::getInstance().getEventRaiser(childSessionId);
        if (!eventRaiser) {
            errors.push_back("No EventRaiser found for child session: " + childSessionId);
        }
    }

    return errors;
}

std::string InvokeEventTarget::getDebugInfo() const {
    std::string childSessionId = JSEngine::instance().getInvokeSessionId(parentSessionId_, invokeId_);
    return "invoke target (invoke: " + invokeId_ + ", parent: " + parentSessionId_ + ", child: " + childSessionId + ")";
}

}  // namespace RSM