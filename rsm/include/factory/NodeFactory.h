#pragma once

#include "DataModelItem.h"
#include "GuardNode.h"
#include "InvokeNode.h"
#include "StateNode.h"
#include "TransitionNode.h"
#include "actions/AssignAction.h"
#include "actions/IfAction.h"
#include "actions/LogAction.h"
#include "actions/RaiseAction.h"
#include "actions/ScriptAction.h"
#include "types.h"

namespace RSM {

// Forward declarations
class IStateNode;
class ITransitionNode;
class IGuardNode;
class IActionNode;
class IDataModelItem;
class IInvokeNode;

class NodeFactory {
public:
    virtual ~NodeFactory() = default;
    virtual std::shared_ptr<IStateNode> createStateNode(const std::string &id, const Type type);
    virtual std::shared_ptr<ITransitionNode> createTransitionNode(const std::string &event, const std::string &target);
    virtual std::shared_ptr<IGuardNode> createGuardNode(const std::string &id, const std::string &target);
    virtual std::shared_ptr<RSM::IActionNode> createActionNode(const std::string &name);
    virtual std::shared_ptr<IDataModelItem> createDataModelItem(const std::string &id, const std::string &expr = "");
    virtual std::shared_ptr<IInvokeNode> createInvokeNode(const std::string &id);
};

}  // namespace RSM