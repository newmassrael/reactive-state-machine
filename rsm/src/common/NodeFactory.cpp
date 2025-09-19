#include "factory/NodeFactory.h"
#include "common/Logger.h"

std::shared_ptr<RSM::IStateNode> RSM::NodeFactory::createStateNode(const std::string &id, const Type type) {
    Logger::debug("RSM::NodeFactory::createStateNode() - Creating state node: " + id);
    return std::make_shared<RSM::StateNode>(id, type);
}

std::shared_ptr<RSM::ITransitionNode> RSM::NodeFactory::createTransitionNode(const std::string &event,
                                                                             const std::string &target) {
    Logger::debug("RSM::NodeFactory::createTransitionNode() - Creating transition node: " +
                  (event.empty() ? "<no event>" : event) + " -> " + target);
    return std::make_shared<RSM::TransitionNode>(event, target);
}

std::shared_ptr<RSM::IGuardNode> RSM::NodeFactory::createGuardNode(const std::string &id, const std::string &target) {
    Logger::debug("RSM::NodeFactory::createGuardNode() - Creating guard node: " + id + " -> " + target);
    return std::make_shared<RSM::GuardNode>(id, target);
}

std::shared_ptr<RSM::IActionNode> RSM::NodeFactory::createActionNode(const std::string &name) {
    Logger::debug("RSM::NodeFactory::createActionNode() - Creating action node: " + name);
    return std::make_shared<RSM::ActionNode>(name);
}

std::shared_ptr<RSM::IDataModelItem> RSM::NodeFactory::createDataModelItem(const std::string &id,
                                                                           const std::string &expr) {
    Logger::debug("RSM::NodeFactory::createDataModelItem() - Creating data model item: " + id);
    return std::make_shared<RSM::DataModelItem>(id, expr);
}

std::shared_ptr<RSM::IInvokeNode> RSM::NodeFactory::createInvokeNode(const std::string &id) {
    Logger::debug("RSM::NodeFactory::createInvokeNode() - Creating invoke node: " + id);
    return std::make_shared<RSM::InvokeNode>(id);
}
