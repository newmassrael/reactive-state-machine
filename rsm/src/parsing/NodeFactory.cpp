#include "factory/NodeFactory.h"
#include "common/Logger.h"

std::shared_ptr<IStateNode> NodeFactory::createStateNode(const std::string &id,
                                                         const Type type) {
  Logger::debug("NodeFactory::createStateNode() - Creating state node: " + id);
  return std::make_shared<StateNode>(id, type);
}

std::shared_ptr<ITransitionNode>
NodeFactory::createTransitionNode(const std::string &event,
                                  const std::string &target) {
  Logger::debug(
      "NodeFactory::createTransitionNode() - Creating transition node: " +
      (event.empty() ? "<no event>" : event) + " -> " + target);
  return std::make_shared<TransitionNode>(event, target);
}

std::shared_ptr<IGuardNode>
NodeFactory::createGuardNode(const std::string &id, const std::string &target) {
  Logger::debug("NodeFactory::createGuardNode() - Creating guard node: " + id +
                " -> " + target);
  return std::make_shared<GuardNode>(id, target);
}

std::shared_ptr<IActionNode>
NodeFactory::createActionNode(const std::string &name) {
  Logger::debug("NodeFactory::createActionNode() - Creating action node: " +
                name);
  return std::make_shared<ActionNode>(name);
}

std::shared_ptr<IDataModelItem>
NodeFactory::createDataModelItem(const std::string &id,
                                 const std::string &expr) {
  Logger::debug(
      "NodeFactory::createDataModelItem() - Creating data model item: " + id);
  return std::make_shared<DataModelItem>(id, expr);
}

std::shared_ptr<IInvokeNode>
NodeFactory::createInvokeNode(const std::string &id) {
  Logger::debug("NodeFactory::createInvokeNode() - Creating invoke node: " +
                id);
  return std::make_shared<InvokeNode>(id);
}
