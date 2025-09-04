#include "core/actions/SendActionNode.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <regex>

namespace SCXML {
namespace Core {

SendActionNode::SendActionNode(const std::string &id)
    : ActionNode(id), target_(), event_(), data_(), delay_(), sendId_(), type_() {}

void SendActionNode::setTarget(const std::string &target) {
    target_ = target;
}

void SendActionNode::setEvent(const std::string &event) {
    event_ = event;
}

void SendActionNode::setData(const std::string &data) {
    data_ = data;
}

void SendActionNode::setDelay(const std::string &delay) {
    delay_ = delay;
}

void SendActionNode::setSendId(const std::string &sendId) {
    sendId_ = sendId;
}

void SendActionNode::setType(const std::string &type) {
    type_ = type;
}

bool SendActionNode::execute(::SCXML::Runtime::RuntimeContext &context) {
    if (event_.empty()) {
        // No event specified - this is an error
        SCXML::Common::Logger::error("Event name is required for send action");
        return false;
    }

    try {
        // Create event from action parameters
        auto eventPtr = createEvent(context);
        if (!eventPtr) {
            SCXML::Common::Logger::error("Failed to create event for send action");
            return false;
        }

        // Handle delay if specified
        uint64_t delayMs = parseDelay(delay_);

        // Set sendId on event for tracking
        if (!sendId_.empty()) {
            // Set sendId on event for tracking (method might not exist yet)
        }

        // Determine target - empty or "#_internal" means internal
        std::string resolvedTarget = target_;
        if (resolvedTarget.empty() || resolvedTarget == "#_internal") {
            // Internal event - send to our own event queue
            if (delayMs > 0) {
                // Schedule delayed internal event
                context.sendEvent(eventPtr, "", delayMs);
            } else {
                // Immediate internal event
                context.raiseEvent(eventPtr);
            }
        } else {
            // External event - send to specified target
            context.sendEvent(eventPtr, resolvedTarget, delayMs);
            // sendEvent is void, assume success unless exception thrown
        }

        return true;
    } catch (const std::exception &e) {
        // Log error and fail gracefully
        SCXML::Common::Logger::error("Exception in send action: " + std::string(e.what()));
        return false;
    }
}

std::shared_ptr<SCXML::Model::IActionNode> SendActionNode::clone() const {
    auto cloned = std::make_shared<SendActionNode>(getId());
    cloned->setTarget(target_);
    cloned->setEvent(event_);
    cloned->setData(data_);
    cloned->setDelay(delay_);
    cloned->setSendId(sendId_);
    cloned->setType(type_);

    // Clone child actions (temporarily disabled - IActionNode missing clone method)
    // for (const auto& child : getChildActions()) {
    //     if (child) {
    //         cloned->addChildAction(child->clone());
    //     }
    // }

    return cloned;
}

SCXML::Events::EventPtr SendActionNode::createEvent(::SCXML::Runtime::RuntimeContext & /* context */) {
    using namespace SCXML::Events;

    // Determine event type
    Event::Type eventType = Event::Type::EXTERNAL;
    if (!type_.empty()) {
        std::string lowerType = type_;
        std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(), ::tolower);

        if (lowerType == "platform") {
            eventType = Event::Type::PLATFORM;
        } else if (lowerType == "internal") {
            eventType = Event::Type::INTERNAL;
        } else if (lowerType == "external") {
            eventType = Event::Type::EXTERNAL;
        }
    }

    // If target is internal, force internal type
    if (target_.empty() || target_ == "#_internal") {
        eventType = Event::Type::INTERNAL;
    }

    // Create event with SCXML-compliant parameters
    // Using the constructor that takes name, data, and type
    return std::make_shared<Event>(event_, Event::EventData(data_), eventType);
}

uint64_t SendActionNode::parseDelay(const std::string &delayStr) const {
    if (delayStr.empty()) {
        return 0;
    }

    // Parse delay specification (e.g., "5s", "100ms", "2min", "1.5s")
    std::regex delayRegex(R"((\d+(?:\.\d+)?)\s*([a-zA-Z]*))");
    std::smatch match;

    if (!std::regex_match(delayStr, match, delayRegex)) {
        // Invalid format, treat as immediate
        return 0;
    }

    double value = std::stod(match[1].str());
    std::string unit = match[2].str();

    // Convert to lowercase for comparison
    std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

    // Convert to milliseconds
    if (unit.empty() || unit == "ms" || unit == "milliseconds") {
        return static_cast<uint64_t>(value);
    } else if (unit == "s" || unit == "sec" || unit == "seconds") {
        return static_cast<uint64_t>(value * 1000);
    } else if (unit == "min" || unit == "minutes") {
        return static_cast<uint64_t>(value * 60 * 1000);
    } else if (unit == "h" || unit == "hr" || unit == "hours") {
        return static_cast<uint64_t>(value * 60 * 60 * 1000);
    } else {
        // Unknown unit, treat as milliseconds
        return static_cast<uint64_t>(value);
    }
}

}  // namespace Core
}  // namespace SCXML
