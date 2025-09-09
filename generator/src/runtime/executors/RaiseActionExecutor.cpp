#include "runtime/executors/RaiseActionExecutor.h"
#include "common/Logger.h"
#include "core/actions/RaiseActionNode.h"
#include <stdexcept>

namespace SCXML {
namespace Runtime {

bool RaiseActionExecutor::execute(const Core::ActionNode &actionNode, RuntimeContext &context) {
    // Cast to specific type
    const auto *raiseNode = safeCast<Core::RaiseActionNode>(actionNode);
    if (!raiseNode) {
        logExecutionError("raise", "Invalid action node type for RaiseActionExecutor", context);
        return false;
    }

    // Validate configuration
    auto errors = validate(actionNode);
    if (!errors.empty()) {
        SCXML::Common::Logger::error("RaiseActionExecutor::execute - Validation errors:");
        for (const auto &error : errors) {
            SCXML::Common::Logger::error("  " + error);
        }
        logExecutionError("raise", "Validation failed", context);
        return false;
    }

    try {
        // Create internal event from action parameters
        auto eventPtr = createEvent(*raiseNode, context);
        if (!eventPtr) {
            logExecutionError("raise", "Failed to create event for raise action", context);

            // W3C SCXML spec: raise failure should generate error event
            SCXML::Events::Event errorEvent("error.execution", SCXML::Events::Event::Type::PLATFORM);
            errorEvent.setData("Raise action failed to create event");

            // Send error event to internal queue
            context.raiseEvent(std::make_shared<SCXML::Events::Event>(errorEvent));
            return false;
        }

        // Raise the event immediately in the context
        // Internal events have higher priority and are processed immediately
        SCXML::Common::Logger::debug("RaiseActionExecutor::execute - Raising event: " + eventPtr->getName());
        context.raiseEvent(eventPtr);
        return true;

    } catch (const std::exception &e) {
        // Log error and fail gracefully
        logExecutionError("raise", "Exception in raise action: " + std::string(e.what()), context);

        // W3C SCXML spec: raise exception should generate error event
        SCXML::Events::Event errorEvent("error.execution", SCXML::Events::Event::Type::PLATFORM);
        errorEvent.setData("Raise action exception: " + std::string(e.what()));

        // Send error event to internal queue (fire-and-forget)
        context.raiseEvent(std::make_shared<SCXML::Events::Event>(errorEvent));
        return false;
    }
}

std::vector<std::string> RaiseActionExecutor::validate(const Core::ActionNode &actionNode) const {
    std::vector<std::string> errors;

    const auto *raiseNode = safeCast<Core::RaiseActionNode>(actionNode);
    if (!raiseNode) {
        errors.push_back("Invalid action node type for RaiseActionExecutor");
        return errors;
    }

    // Must have event name
    if (raiseNode->getEvent().empty()) {
        errors.push_back("Raise action must have an 'event' attribute");
    }

    return errors;
}

std::shared_ptr<Events::Event> RaiseActionExecutor::createEvent(const Core::RaiseActionNode &raiseNode,
                                                                RuntimeContext &context) const {
    (void)context;  // Suppress unused parameter warning for now

    using namespace SCXML::Events;

    // Raised events are always internal by definition
    // They are processed with high priority within the same state machine instance
    const std::string &eventName = raiseNode.getEvent();
    const std::string &data = raiseNode.getData();

    return std::make_shared<Event>(eventName, Event::EventData(data), Event::Type::INTERNAL);
}

}  // namespace Runtime
}  // namespace SCXML