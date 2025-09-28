#include "events/ParentEventTarget.h"
#include "common/Logger.h"
#include "events/EventRaiserService.h"
#include "runtime/IEventRaiser.h"
#include "scripting/JSEngine.h"
#include <sstream>

namespace RSM {

ParentEventTarget::ParentEventTarget(const std::string &childSessionId, std::shared_ptr<IEventRaiser> eventRaiser)
    : childSessionId_(childSessionId), eventRaiser_(std::move(eventRaiser)) {
    if (childSessionId_.empty()) {
        throw std::invalid_argument("ParentEventTarget requires a valid child session ID");
    }

    if (!eventRaiser_) {
        throw std::invalid_argument("ParentEventTarget requires a valid event raiser");
    }

    LOG_DEBUG("ParentEventTarget: Created for child session: {}", childSessionId_);
}

std::future<SendResult> ParentEventTarget::send(const EventDescriptor &event) {
    LOG_DEBUG("ParentEventTarget::send() - ENTRY: event='{}', target='{}', sessionId='{}'", event.eventName,
              event.target, event.sessionId);

    std::promise<SendResult> resultPromise;
    auto resultFuture = resultPromise.get_future();

    try {
        // Use session ID from event descriptor as child session ID
        std::string actualChildSessionId = event.sessionId.empty() ? childSessionId_ : event.sessionId;
        LOG_DEBUG("ParentEventTarget::send() - Child session: '{}' (from event: '{}', from constructor: '{}')",
                  actualChildSessionId, event.sessionId, childSessionId_);

        // Find parent session ID
        std::string parentSessionId = findParentSessionId(actualChildSessionId);
        if (parentSessionId.empty()) {
            LOG_ERROR("ParentEventTarget: No parent session found for child: {}", actualChildSessionId);
            resultPromise.set_value(SendResult::error("No parent session found for child: " + actualChildSessionId,
                                                      SendResult::ErrorType::TARGET_NOT_FOUND));
            return resultFuture;
        }

        LOG_DEBUG("ParentEventTarget: Routing event '{}' from child '{}' to parent '{}'", event.eventName,
                  actualChildSessionId, parentSessionId);

        // Get parent session's EventRaiser from centralized service
        auto parentEventRaiser = EventRaiserService::getInstance().getEventRaiser(parentSessionId);
        if (!parentEventRaiser) {
            LOG_ERROR("ParentEventTarget: No EventRaiser found for parent session: {}", parentSessionId);
            resultPromise.set_value(SendResult::error("No EventRaiser found for parent session: " + parentSessionId,
                                                      SendResult::ErrorType::TARGET_NOT_FOUND));
            return resultFuture;
        }

        // Create event with parent session as target
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

        // Raise event in parent session using parent's EventRaiser
        // W3C SCXML: Events from child to parent are delivered as external events
        LOG_DEBUG("ParentEventTarget::send() - Calling parent EventRaiser->raiseEvent('{}', '{}')", eventName,
                  eventData);
        bool raiseResult = parentEventRaiser->raiseEvent(eventName, eventData);
        LOG_DEBUG("ParentEventTarget::send() - parent EventRaiser->raiseEvent() returned: {}", raiseResult);

        LOG_DEBUG("ParentEventTarget: Successfully routed event '{}' to parent session '{}'", eventName,
                  parentSessionId);

        resultPromise.set_value(SendResult::success(event.sendId));

    } catch (const std::exception &e) {
        LOG_ERROR("ParentEventTarget: Error sending event to parent: {}", e.what());
        resultPromise.set_value(SendResult::error("Failed to send event to parent: " + std::string(e.what()),
                                                  SendResult::ErrorType::INTERNAL_ERROR));
    }

    return resultFuture;
}

std::vector<std::string> ParentEventTarget::validate() const {
    std::vector<std::string> errors;

    if (childSessionId_.empty()) {
        errors.push_back("Child session ID cannot be empty");
    }

    if (!eventRaiser_) {
        errors.push_back("Event raiser cannot be null");
    }

    // Check if parent session exists
    std::string parentSessionId = findParentSessionId(childSessionId_);
    if (parentSessionId.empty()) {
        errors.push_back("No parent session found for child: " + childSessionId_);
    }

    return errors;
}

std::string ParentEventTarget::getTargetType() const {
    return "parent";
}

bool ParentEventTarget::canHandle(const std::string &targetUri) const {
    return targetUri == "#_parent";
}

std::string ParentEventTarget::getDebugInfo() const {
    std::string parentSessionId = findParentSessionId(childSessionId_);
    return "parent target (child: " + childSessionId_ + ", parent: " + parentSessionId + ")";
}

std::string ParentEventTarget::findParentSessionId(const std::string &childSessionId) const {
    // Access JSEngine to find parent session relationship
    JSEngine &jsEngine = JSEngine::instance();

    // Get parent session ID from JSEngine
    std::string parentSessionId = jsEngine.getParentSessionId(childSessionId);

    if (parentSessionId.empty()) {
        LOG_DEBUG("ParentEventTarget: No parent session found for child: {}", childSessionId);
    } else {
        LOG_DEBUG("ParentEventTarget: Found parent session '{}' for child '{}'", parentSessionId, childSessionId);
    }

    return parentSessionId;
}

}  // namespace RSM