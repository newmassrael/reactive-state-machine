#include "core/actions/AssignActionNode.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "runtime/DataModelEngine.h"
#include "runtime/RuntimeContext.h"
#include "runtime/ActionExecutor.h"
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
    SCXML::Common::Logger::debug("AssignActionNode::setExpr - Input length: " + std::to_string(expr.length()));
    SCXML::Common::Logger::debug("AssignActionNode::setExpr - Stored expr_ value: '" + expr_ + "'");
    SCXML::Common::Logger::debug("AssignActionNode::setExpr - Stored expr_ length: " + std::to_string(expr_.length()));
    
    // 16진수 덤프로 정확한 내용 확인
    std::string hexDump = "";
    for (size_t i = 0; i < expr_.length(); ++i) {
        char buf[4];
        sprintf(buf, "%02x ", (unsigned char)expr_[i]);
        hexDump += buf;
    }
    SCXML::Common::Logger::debug("AssignActionNode::setExpr - Hex dump: " + hexDump);
}

const std::string &AssignActionNode::getExpr() const {
    SCXML::Common::Logger::debug("AssignActionNode::getExpr - Retrieved expr_ value: '" + expr_ + "'");
    SCXML::Common::Logger::debug("AssignActionNode::getExpr - Retrieved expr_ length: " + std::to_string(expr_.length()));
    
    // 16진수 덤프로 정확한 내용 확인
    std::string hexDump = "";
    for (size_t i = 0; i < expr_.length(); ++i) {
        char buf[4];
        sprintf(buf, "%02x ", (unsigned char)expr_[i]);
        hexDump += buf;
    }
    SCXML::Common::Logger::debug("AssignActionNode::getExpr - Hex dump: " + hexDump);
    
    return expr_;
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

bool AssignActionNode::execute(::SCXML::Runtime::RuntimeContext &context) {
    // Use Executor pattern - create static factory
    ::SCXML::Runtime::DefaultActionExecutorFactory factory;
    auto executor = factory.createExecutor(getActionType());
    
    if (!executor) {
        SCXML::Common::Logger::error("AssignActionNode::execute - No executor available for action type: " + getActionType());
        return false;
    }

    return executor->execute(*this, context);
}

std::vector<std::string> AssignActionNode::validate() const {
    // Use Executor pattern - delegate to AssignActionExecutor
    ::SCXML::Runtime::DefaultActionExecutorFactory factory;
    auto executor = factory.createExecutor(getActionType());
    
    if (!executor) {
        return {"No executor available for action type: " + getActionType()};
    }

    return executor->validate(*this);
}

}  // namespace Core
}  // namespace SCXML
