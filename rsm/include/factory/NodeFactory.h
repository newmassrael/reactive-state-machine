#pragma once

#include "INodeFactory.h"
#include "StateNode.h"
#include "TransitionNode.h"
#include "GuardNode.h"
#include "ActionNode.h"
#include "DataModelItem.h"
#include "InvokeNode.h"


namespace RSM {

class NodeFactory : public INodeFactory
{
public:
    std::shared_ptr<IStateNode> createStateNode(const std::string &id, const Type type) override;
    std::shared_ptr<ITransitionNode> createTransitionNode(const std::string &event, const std::string &target) override;
    std::shared_ptr<IGuardNode> createGuardNode(const std::string &id, const std::string &target) override;
    std::shared_ptr<IActionNode> createActionNode(const std::string &name) override;
    std::shared_ptr<IDataModelItem> createDataModelItem(const std::string &id, const std::string &expr = "") override;
    std::shared_ptr<IInvokeNode> createInvokeNode(const std::string &id) override;
};


}  // namespace RSM