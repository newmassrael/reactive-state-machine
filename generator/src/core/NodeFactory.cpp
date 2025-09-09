#include "core/NodeFactory.h"
#include "common/Logger.h"
#include "core/DataNode.h"
#include "core/actions/AssignActionNode.h"
#include "core/actions/CancelActionNode.h"
#include "core/actions/ForeachActionNode.h"
#include "core/actions/IfActionNode.h"
#include "core/actions/LogActionNode.h"
#include "core/actions/RaiseActionNode.h"
#include "core/actions/ScriptActionNode.h"
#include "core/actions/SendActionNode.h"

namespace SCXML {
namespace Core {

std::shared_ptr<SCXML::Model::IStateNode> NodeFactory::createStateNode(const std::string &id, const Type type) {
    SCXML::Common::Logger::debug("NodeFactory::createStateNode() - Creating state node: " + id);
    return std::make_shared<StateNode>(id, type);
}

std::shared_ptr<SCXML::Model::ITransitionNode> NodeFactory::createTransitionNode(const std::string &event,
                                                                                 const std::string &target) {
    SCXML::Common::Logger::debug("NodeFactory::createTransitionNode() - Creating transition node: " +
                                 (event.empty() ? "<no event>" : event) + " -> " + target);
    return std::make_shared<TransitionNode>(event, target);
}

std::shared_ptr<SCXML::Model::IGuardNode> NodeFactory::createGuardNode(const std::string &id,
                                                                       const std::string &target) {
    SCXML::Common::Logger::debug("NodeFactory::createGuardNode() - Creating guard node: " + id + " -> " + target);
    return std::make_shared<GuardNode>(id, target);
}

std::shared_ptr<SCXML::Model::IActionNode> NodeFactory::createActionNode(const std::string &name) {
    SCXML::Common::Logger::debug("NodeFactory::createActionNode() - Creating action node: " + name);

    // Create specific ActionNode types based on the action name
    if (name == "assign") {
        auto assignNode = std::make_shared<AssignActionNode>(name);

        return assignNode;
    } else if (name == "log") {
        auto logNode = std::make_shared<LogActionNode>(name);

        return logNode;
    } else if (name == "script") {
        auto scriptNode = std::make_shared<ScriptActionNode>(name);

        return scriptNode;
    } else if (name == "raise") {
        auto raiseNode = std::make_shared<SCXML::Core::RaiseActionNode>(name);
        return raiseNode;
    } else if (name == "if") {
        auto ifNode = std::make_shared<SCXML::Core::IfActionNode>(name);
        return ifNode;
    } else if (name == "send") {
        auto sendNode = std::make_shared<SCXML::Core::SendActionNode>(name);
        return sendNode;
    } else if (name == "cancel") {
        auto cancelNode = std::make_shared<SCXML::Core::CancelActionNode>(name);
        return cancelNode;
    } else if (name == "foreach") {
        auto foreachNode = std::make_shared<SCXML::Core::ForeachActionNode>(name);
        return foreachNode;
    }
    // TODO: Add other ActionNode types as needed
    else {
        SCXML::Common::Logger::error(
            "NodeFactory::createActionNode - Unknown action type, creating basic ActionNode: " + name);
        auto basicNode = std::make_shared<ActionNode>(name);
        return basicNode;
    }
}

std::shared_ptr<IDataModelItem> NodeFactory::createDataModelItem(const std::string &id, const std::string &expr) {
    SCXML::Common::Logger::debug("NodeFactory::createDataModelItem() - Creating data model item: " + id);
    auto dataNode = std::make_shared<DataNode>(id);
    if (!expr.empty()) {
        dataNode->setExpr(expr);
    }
    return dataNode;
}

std::shared_ptr<IInvokeNode> NodeFactory::createInvokeNode(const std::string &id) {
    SCXML::Common::Logger::debug("NodeFactory::createInvokeNode() - Creating invoke node: " + id);
    return std::make_shared<InvokeNode>(id);
}

}  // namespace Core
}  // namespace SCXML
