#include "runtime/executors/SendActionExecutor.h"
#include "common/Logger.h"
#include "core/actions/SendActionNode.h"
#include "runtime/RuntimeContext.h"

namespace SCXML {
namespace Runtime {

bool SendActionExecutor::execute(const Core::ActionNode &actionNode, RuntimeContext &context) {
    const auto *sendNode = safeCast<Core::SendActionNode>(actionNode);
    if (!sendNode) {
        SCXML::Common::Logger::error("SendActionExecutor::execute - Invalid action node type");
        return false;
    }

    try {
        SCXML::Common::Logger::debug("SendActionExecutor::execute - Processing send action: " + sendNode->getId());

        // Get event name
        std::string eventName = sendNode->getEvent();
        if (eventName.empty()) {
            SCXML::Common::Logger::error("SendActionExecutor::execute - No event specified for send action");
            return false;
        }

        // Create event
        auto event = context.createEvent(eventName);
        if (!event) {
            SCXML::Common::Logger::error("SendActionExecutor::execute - Failed to create event: " + eventName);
            return false;
        }

        // Set event data if provided
        std::string eventData = sendNode->getData();
        if (!eventData.empty()) {
            // TODO: Set event data payload (would need Event interface extension)
            SCXML::Common::Logger::debug("SendActionExecutor::execute - Event data: " + eventData);
        }

        // Get delay and target
        std::string delayStr = sendNode->getDelay();
        std::string target = sendNode->getTarget();
        std::string sendId = sendNode->getSendId();

        // Parse delay
        uint64_t delayMs = sendNode->parseDelay(delayStr);

        // Handle immediate vs delayed delivery
        if (delayMs == 0) {
            // Immediate delivery
            SCXML::Common::Logger::debug("SendActionExecutor::execute - Sending immediate event: " + eventName);

            if (target.empty() || target == "#_internal") {
                // Internal event
                context.raiseEvent(event);
            } else {
                // External event
                context.sendEvent(event, target);
            }
        } else {
            // Delayed delivery - use EventScheduler for proper delay handling
            SCXML::Common::Logger::debug("SendActionExecutor::execute - Scheduling delayed event: " + eventName +
                                         " (delay: " + std::to_string(delayMs) + "ms)");

            // Use EventScheduler for non-blocking delay implementation
            context.scheduleEvent(event, delayMs, target, sendId);
        }

        SCXML::Common::Logger::info("SendActionExecutor::execute - Send action completed: " + eventName);
        return true;

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("SendActionExecutor::execute - Exception: " + std::string(e.what()));
        return false;
    }
}

}  // namespace Runtime
}  // namespace SCXML