#include "core/actions/RaiseActionNode.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "runtime/RuntimeContext.h"

namespace SCXML {
namespace Core {

RaiseActionNode::RaiseActionNode(const std::string &id) : ActionNode(id), event_(), data_() {}

void RaiseActionNode::setEvent(const std::string &event) {
    event_ = event;
}

void RaiseActionNode::setData(const std::string &data) {
    data_ = data;
}

bool RaiseActionNode::execute(::SCXML::Runtime::RuntimeContext &context) {
    if (event_.empty()) {
        // No event specified - this is an error
        SCXML::Common::Logger::error("Event name is required for raise action");
        return false;
    }

    try {
        // Create internal event from action parameters
        auto eventPtr = createEvent(context);
        if (!eventPtr) {
            SCXML::Common::Logger::error("Failed to create event for raise action");

            // W3C SCXML spec: raise failure should generate error event
            SCXML::Events::Event errorEvent("error.execution", SCXML::Events::Event::Type::PLATFORM);
            // errorEvent already created with correct type
            errorEvent.setData("Raise action failed to create event");

            // Send error event to internal queue
            context.raiseEvent(std::make_shared<SCXML::Events::Event>(errorEvent));
            return false;
        }

        // Raise the event immediately in the context
        // Internal events have higher priority and are processed immediately
        context.raiseEvent(eventPtr);
        return true;
    } catch (const std::exception &e) {
        // Log error and fail gracefully
        SCXML::Common::Logger::error("Exception in raise action: " + std::string(e.what()));

        // W3C SCXML spec: raise exception should generate error event
        SCXML::Events::Event errorEvent("error.execution", SCXML::Events::Event::Type::PLATFORM);
        errorEvent.setData("Raise action exception: " + std::string(e.what()));

        // Send error event to internal queue (fire-and-forget)
        context.raiseEvent(std::make_shared<SCXML::Events::Event>(errorEvent));
        return false;
    }
}

std::shared_ptr<SCXML::Model::IActionNode> RaiseActionNode::clone() const {
    auto cloned = std::make_shared<RaiseActionNode>(getId());
    cloned->setEvent(event_);
    cloned->setData(data_);

    // Clone child actions (temporarily disabled - IActionNode missing clone method)
    // for (const auto& child : getChildActions()) {
    //     if (child) {
    //         cloned->addChildAction(child->clone());
    //     }
    // }

    return cloned;
}

SCXML::Events::EventPtr RaiseActionNode::createEvent(::SCXML::Runtime::RuntimeContext & /* context */) {
    using namespace SCXML::Events;

    // Raised events are always internal by definition
    // They are processed with high priority within the same state machine instance
    return std::make_shared<Event>(event_, Event::EventData(data_), Event::Type::INTERNAL);
}

}  // namespace Core
}  // namespace SCXML
