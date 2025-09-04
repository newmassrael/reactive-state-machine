#pragma once

#include "model/INodeFactory.h"
#include "core/ActionNode.h"
#include "core/DataModelItem.h"
#include "core/GuardNode.h"
#include "core/InvokeNode.h"
#include "core/StateNode.h"
#include "core/TransitionNode.h"

using SCXML::Model::IStateNode;
using SCXML::Model::ITransitionNode;
using SCXML::Model::IActionNode;
using SCXML::Model::IGuardNode;
using SCXML::Model::IDataModelItem;
using SCXML::Model::INodeFactory;

namespace SCXML {
namespace Core {

class NodeFactory : public INodeFactory {
public:
    std::shared_ptr<IStateNode> createStateNode(const std::string &id, const Type type) override;
    std::shared_ptr<ITransitionNode> createTransitionNode(const std::string &event, const std::string &target) override;
    std::shared_ptr<IGuardNode> createGuardNode(const std::string &id, const std::string &target) override;
    std::shared_ptr<IActionNode> createActionNode(const std::string &name) override;
    std::shared_ptr<IDataModelItem> createDataModelItem(const std::string &id, const std::string &expr = "") override;
    std::shared_ptr<IInvokeNode> createInvokeNode(const std::string &id) override;
};

} // namespace Core
}  // namespace SCXML
