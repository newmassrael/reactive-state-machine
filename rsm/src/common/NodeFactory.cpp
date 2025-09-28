#include "factory/NodeFactory.h"
#include "actions/AssignAction.h"
#include "actions/CancelAction.h"
#include "actions/IfAction.h"
#include "actions/LogAction.h"
#include "actions/RaiseAction.h"
#include "actions/ScriptAction.h"
#include "actions/SendAction.h"
#include "common/Logger.h"
#include "states/ConcurrentStateNode.h"

std::shared_ptr<RSM::IStateNode> RSM::NodeFactory::createStateNode(const std::string &id, const Type type) {
    LOG_DEBUG("Creating state node: {}", id);

    // SCXML W3C specification section 3.4: parallel states require ConcurrentStateNode
    if (type == Type::PARALLEL) {
        // Create ConcurrentStateNode with default configuration for SCXML compliance
        ConcurrentStateConfig config;  // Uses SCXML W3C compliant defaults
        LOG_DEBUG("Creating ConcurrentStateNode for parallel state: {}", id);
        return std::make_shared<RSM::ConcurrentStateNode>(id, config);
    }

    return std::make_shared<RSM::StateNode>(id, type);
}

std::shared_ptr<RSM::ITransitionNode> RSM::NodeFactory::createTransitionNode(const std::string &event,
                                                                             const std::string &target) {
    LOG_DEBUG("Creating transition node: {} -> {}", (event.empty() ? "<no event>" : event), target);
    return std::make_shared<RSM::TransitionNode>(event, target);
}

std::shared_ptr<RSM::IGuardNode> RSM::NodeFactory::createGuardNode(const std::string &id, const std::string &target) {
    LOG_DEBUG("Creating guard node: {} -> {}", id, target);
    return std::make_shared<RSM::GuardNode>(id, target);
}

std::shared_ptr<RSM::IActionNode> RSM::NodeFactory::createActionNode(const std::string &name) {
    LOG_DEBUG("Creating action node: {}", name);

    // Create specific action types based on SCXML element names
    if (name == "script") {
        return std::make_shared<RSM::ScriptAction>("");
    } else if (name == "assign") {
        return std::make_shared<RSM::AssignAction>("", "");
    } else if (name == "log") {
        return std::make_shared<RSM::LogAction>("");
    } else if (name == "raise") {
        return std::make_shared<RSM::RaiseAction>("");
    } else if (name == "if") {
        return std::make_shared<RSM::IfAction>("");
    } else if (name == "send") {
        return std::make_shared<RSM::SendAction>("");
    } else if (name == "cancel") {
        return std::make_shared<RSM::CancelAction>("");
    } else {
        // Default to ScriptAction for unknown types
        LOG_WARN("Unknown action type: {}, defaulting to ScriptAction", name);
        return std::make_shared<RSM::ScriptAction>("");
    }
}

std::shared_ptr<RSM::IDataModelItem> RSM::NodeFactory::createDataModelItem(const std::string &id,
                                                                           const std::string &expr) {
    LOG_DEBUG("Creating data model item: {}", id);
    return std::make_shared<RSM::DataModelItem>(id, expr);
}

std::shared_ptr<RSM::IInvokeNode> RSM::NodeFactory::createInvokeNode(const std::string &id) {
    LOG_DEBUG("Creating invoke node: {}", id);
    return std::make_shared<RSM::InvokeNode>(id);
}
