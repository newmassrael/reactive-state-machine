#include "core/actions/RaiseActionNode.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "runtime/RuntimeContext.h"
#include "runtime/ActionExecutor.h"

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
    // Use Executor pattern - create static factory
    ::SCXML::Runtime::DefaultActionExecutorFactory factory;
    auto executor = factory.createExecutor(getActionType());
    
    if (!executor) {
        SCXML::Common::Logger::error("RaiseActionNode::execute - No executor available for action type: " + getActionType());
        return false;
    }

    return executor->execute(*this, context);
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

std::vector<std::string> RaiseActionNode::validate() const {
    // Use Executor pattern - delegate to RaiseActionExecutor
    ::SCXML::Runtime::DefaultActionExecutorFactory factory;
    auto executor = factory.createExecutor(getActionType());
    
    if (!executor) {
        return {"No executor available for action type: " + getActionType()};
    }

    return executor->validate(*this);
}

SCXML::Events::EventPtr RaiseActionNode::createEvent(::SCXML::Runtime::RuntimeContext & /* context */) {
    using namespace SCXML::Events;

    // Raised events are always internal by definition
    // They are processed with high priority within the same state machine instance
    return std::make_shared<Event>(event_, Event::EventData(data_), Event::Type::INTERNAL);
}

}  // namespace Core
}  // namespace SCXML
