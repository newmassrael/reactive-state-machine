#include "core/actions/AssignActionNode.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "runtime/DataModelEngine.h"
#include "runtime/RuntimeContext.h"
#include <sstream>

namespace SCXML {
namespace Core {

AssignActionNode::AssignActionNode(const std::string &id) : ActionNode(id) {
    SCXML::Common::Logger::debug("AssignActionNode::Constructor - Creating assign action: " + id);
}

void AssignActionNode::setLocation(const std::string &location) {
    location_ = location;
    SCXML::Common::Logger::debug("AssignActionNode::setLocation - Set location: " + location);
}

void AssignActionNode::setExpr(const std::string &expr) {
    expr_ = expr;
    SCXML::Common::Logger::debug("AssignActionNode::setExpr - Set expression: " + expr);
}

void AssignActionNode::setAttr(const std::string &attr) {
    attr_ = attr;
    SCXML::Common::Logger::debug("AssignActionNode::setAttr - Set attribute: " + attr);
}

void AssignActionNode::setType(const std::string &type) {
    type_ = type;
    SCXML::Common::Logger::debug("AssignActionNode::setType - Set type: " + type);
}

// execute() method removed - now handled by AssignActionExecutor

std::shared_ptr<SCXML::Model::IActionNode> AssignActionNode::clone() const {
    auto clone = std::make_shared<AssignActionNode>(getId());
    clone->setLocation(location_);
    clone->setExpr(expr_);
    clone->setAttr(attr_);
    clone->setType(type_);
    return clone;
}

// validate() method removed - now handled by AssignActionExecutor

// resolveValue() method removed - now handled by AssignActionExecutor

}  // namespace Core
}  // namespace SCXML
