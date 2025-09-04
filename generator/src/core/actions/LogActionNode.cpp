#include "core/actions/LogActionNode.h"
#include "common/Logger.h"
#include "core/actions/LogActionNode.h"
#include "runtime/DataModelEngine.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <sstream>

namespace SCXML {
namespace Core {

LogActionNode::LogActionNode(const std::string &id) : ActionNode(id), level_("info") {  // Default to info level
    SCXML::Common::Logger::debug("LogActionNode::Constructor - Creating log action: " + id);
}

void LogActionNode::setExpr(const std::string &expr) {
    expr_ = expr;
    SCXML::Common::Logger::debug("LogActionNode::setExpr - Set expression: " + expr);
}

void LogActionNode::setLabel(const std::string &label) {
    label_ = label;
    SCXML::Common::Logger::debug("LogActionNode::setLabel - Set label: " + label);
}

void LogActionNode::setLevel(const std::string &level) {
    level_ = level;
    SCXML::Common::Logger::debug("LogActionNode::setLevel - Set level: " + level);
}

// execute() method removed - now handled by LogActionExecutor

std::shared_ptr<SCXML::Model::IActionNode> LogActionNode::clone() const {
    auto clone = std::make_shared<LogActionNode>(getId());
    clone->setExpr(expr_);
    clone->setLabel(label_);
    clone->setLevel(level_);
    return clone;
}

// validate() method removed - now handled by LogActionExecutor

// resolveLogMessage() method removed - now handled by LogActionExecutor

// formatLogMessage() method removed - now handled by LogActionExecutor

}  // namespace Core
}  // namespace SCXML
