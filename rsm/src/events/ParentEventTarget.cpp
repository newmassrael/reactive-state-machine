#include "events/ParentEventTarget.h"
#include "common/JsonUtils.h"
#include "common/Logger.h"
#include "events/EventRaiserService.h"
#include "events/IEventDispatcher.h"
#include "runtime/IEventRaiser.h"
#include "scripting/JSEngine.h"
#include <sstream>
#include <thread>

namespace RSM {

ParentEventTarget::ParentEventTarget(const std::string &childSessionId, std::shared_ptr<IEventRaiser> eventRaiser,
                                     std::shared_ptr<IEventScheduler> scheduler)
    : childSessionId_(childSessionId), eventRaiser_(std::move(eventRaiser)), scheduler_(std::move(scheduler)) {
    if (childSessionId_.empty()) {
        throw std::invalid_argument("ParentEventTarget requires a valid child session ID");
    }

    if (!eventRaiser_) {
        throw std::invalid_argument("ParentEventTarget requires a valid event raiser");
    }

    LOG_DEBUG("ParentEventTarget: Created for child session: {}", childSessionId_);
}

std::future<SendResult> ParentEventTarget::send(const EventDescriptor &event) {
    LOG_DEBUG("ParentEventTarget::send() - ENTRY: event='{}', target='{}', sessionId='{}', delay={}ms", event.eventName,
              event.target, event.sessionId, event.delay.count());

    // Check if this is a delayed event and scheduler is available
    if (event.delay.count() > 0 && scheduler_) {
        LOG_DEBUG("ParentEventTarget: Scheduling delayed parent event '{}' for {}ms", event.eventName,
                  event.delay.count());

        // Create a copy of this target for delayed execution
        auto sharedThis = std::make_shared<ParentEventTarget>(childSessionId_, eventRaiser_, scheduler_);

        // Schedule the event for delayed execution
        auto sendIdFuture = scheduler_->scheduleEvent(event, event.delay, sharedThis, event.sendId, event.sessionId);

        // Convert sendId future to SendResult future
        std::promise<SendResult> resultPromise;
        auto resultFuture = resultPromise.get_future();

        // Handle the sendId asynchronously
        std::thread([sendIdFuture = std::move(sendIdFuture), resultPromise = std::move(resultPromise)]() mutable {
            try {
                std::string assignedSendId = sendIdFuture.get();
                resultPromise.set_value(SendResult::success(assignedSendId));
            } catch (const std::exception &e) {
                resultPromise.set_value(
                    SendResult::error("Failed to schedule delayed parent event: " + std::string(e.what()),
                                      SendResult::ErrorType::INTERNAL_ERROR));
            }
        }).detach();

        return resultFuture;
    } else {
        // Execute immediately (no delay or no scheduler available)
        return sendImmediately(event);
    }
}

std::future<SendResult> ParentEventTarget::sendImmediately(const EventDescriptor &event) {
    LOG_DEBUG("ParentEventTarget::sendImmediately() - ENTRY: event='{}', target='{}', sessionId='{}'", event.eventName,
              event.target, event.sessionId);

    std::promise<SendResult> resultPromise;
    auto resultFuture = resultPromise.get_future();

    try {
        // Use session ID from event descriptor as child session ID
        std::string actualChildSessionId = event.sessionId.empty() ? childSessionId_ : event.sessionId;
        LOG_DEBUG(
            "ParentEventTarget::sendImmediately() - Child session: '{}' (from event: '{}', from constructor: '{}')",
            actualChildSessionId, event.sessionId, childSessionId_);

        // Find parent session ID: use _parentSessionId param if provided (for done.invoke), otherwise lookup via
        // JSEngine
        std::string parentSessionId;
        auto it = event.params.find("_parentSessionId");
        if (it != event.params.end() && !it->second.empty()) {
            parentSessionId = it->second;
            LOG_DEBUG("ParentEventTarget: Using parent session from params: '{}'", parentSessionId);
        } else {
            parentSessionId = findParentSessionId(actualChildSessionId);
            LOG_DEBUG("ParentEventTarget: Found parent session via JSEngine: '{}'", parentSessionId);
        }

        if (parentSessionId.empty()) {
            // W3C SCXML: This is normal during cleanup when parent relationship is already removed
            // Child's onexit handlers may try to send events after invoke is cancelled
            LOG_DEBUG("ParentEventTarget: No parent session found for child: {} (likely during cleanup)",
                      actualChildSessionId);
            resultPromise.set_value(SendResult::error("No parent session found for child: " + actualChildSessionId,
                                                      SendResult::ErrorType::TARGET_NOT_FOUND));
            return resultFuture;
        }

        LOG_DEBUG("ParentEventTarget: Routing event '{}' from child '{}' to parent '{}'", event.eventName,
                  actualChildSessionId, parentSessionId);

        // Get parent session's EventRaiser from centralized service
        LOG_DEBUG("ParentEventTarget: Looking up EventRaiser for parent session: {}", parentSessionId);
        auto parentEventRaiser = EventRaiserService::getInstance().getEventRaiser(parentSessionId);
        LOG_DEBUG("ParentEventTarget: EventRaiser lookup result: {}", parentEventRaiser ? "FOUND" : "NOT FOUND");
        if (!parentEventRaiser) {
            LOG_ERROR("ParentEventTarget: No EventRaiser found for parent session: {}", parentSessionId);
            resultPromise.set_value(SendResult::error("No EventRaiser found for parent session: " + parentSessionId,
                                                      SendResult::ErrorType::TARGET_NOT_FOUND));
            return resultFuture;
        }

        // Create event with parent session as target
        std::string eventName = event.eventName;
        std::string eventData = event.data;

        // W3C SCXML: Format params as JSON object to match ECMAScript data model
        // This enables _event.data.paramName access in finalize handlers (Test 233)
        if (!event.params.empty()) {
            json eventDataJson = json::object();

            // Add all params to the JSON object
            for (const auto &param : event.params) {
                eventDataJson[param.first] = param.second;
            }

            eventData = JsonUtils::toCompactString(eventDataJson);
        }

        // Raise event in parent session using parent's EventRaiser with origin tracking
        // W3C SCXML 6.4: Pass child session ID as originSessionId for finalize support
        LOG_DEBUG(
            "ParentEventTarget::sendImmediately() - Calling parent EventRaiser->raiseEvent('{}', '{}', origin: '{}')",
            eventName, eventData, actualChildSessionId);
        bool raiseResult = parentEventRaiser->raiseEvent(eventName, eventData, actualChildSessionId);
        LOG_DEBUG("ParentEventTarget::sendImmediately() - parent EventRaiser->raiseEvent() returned: {}", raiseResult);

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