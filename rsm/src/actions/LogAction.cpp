#include "actions/LogAction.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"

namespace RSM {

LogAction::LogAction(const std::string &expr, const std::string &id) : BaseAction(id), expr_(expr) {}

bool LogAction::execute(IExecutionContext &context) {
    if (!context.isValid()) {
        return false;
    }

    try {
        return context.getActionExecutor().executeLogAction(*this);
    } catch (const std::exception &) {
        return false;
    }
}

std::string LogAction::getActionType() const {
    return "log";
}

std::shared_ptr<IActionNode> LogAction::clone() const {
    auto cloned = std::make_shared<LogAction>(getId());
    cloned->setExpr(expr_);
    cloned->setLabel(label_);
    cloned->setLevel(level_);
    return cloned;
}

const std::string &LogAction::getExpr() const {
    return expr_;
}

void LogAction::setExpr(const std::string &expr) {
    expr_ = expr;
}

const std::string &LogAction::getLabel() const {
    return label_;
}

void LogAction::setLabel(const std::string &label) {
    label_ = label;
}

const std::string &LogAction::getLevel() const {
    return level_;
}

void LogAction::setLevel(const std::string &level) {
    level_ = level;
}

std::vector<std::string> LogAction::validateSpecific() const {
    std::vector<std::string> errors;

    // Log actions are generally permissive
    // Empty expr is allowed (will log empty string)
    // Empty label is allowed (no prefix)
    // Invalid level will default to "info"

    return errors;
}

std::string LogAction::getSpecificDescription() const {
    std::string desc = "log";
    if (!expr_.empty()) {
        desc += " expr=\"" + expr_ + "\"";
    }
    if (!label_.empty()) {
        desc += " label=\"" + label_ + "\"";
    }
    if (!level_.empty()) {
        desc += " level=\"" + level_ + "\"";
    }
    return desc;
}

}  // namespace RSM