#include "actions/AssignAction.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"
#include <regex>

namespace RSM {

AssignAction::AssignAction(const std::string &location, const std::string &expr, const std::string &id)
    : BaseAction(id), location_(location), expr_(expr) {}

bool AssignAction::execute(IExecutionContext &context) {
    if (!context.isValid()) {
        return false;
    }

    if (isEmptyString(location_)) {
        // Cannot assign to empty location
        return false;
    }

    try {
        return context.getActionExecutor().assignVariable(location_, expr_);
    } catch (const std::exception &) {
        // Exception during execution indicates failure
        return false;
    }
}

std::string AssignAction::getActionType() const {
    return "assign";
}

std::shared_ptr<IActionNode> AssignAction::clone() const {
    auto cloned = std::make_shared<AssignAction>(location_, expr_, getId());
    cloned->setType(type_);
    return cloned;
}

const std::string &AssignAction::getLocation() const {
    return location_;
}

void AssignAction::setLocation(const std::string &location) {
    location_ = location;
}

const std::string &AssignAction::getExpr() const {
    return expr_;
}

void AssignAction::setExpr(const std::string &expr) {
    expr_ = expr;
}

const std::string &AssignAction::getType() const {
    return type_;
}

void AssignAction::setType(const std::string &type) {
    type_ = type;
}

std::vector<std::string> AssignAction::validateSpecific() const {
    std::vector<std::string> errors;

    // Validate location
    if (isEmptyString(location_)) {
        errors.push_back("Assignment location cannot be empty");
    } else if (!isValidLocation(location_)) {
        errors.push_back("Invalid assignment location: " + location_);
    }

    // Validate expression
    if (isEmptyString(expr_)) {
        errors.push_back("Assignment expression cannot be empty");
    }

    // Validate type hint if provided
    if (!type_.empty()) {
        std::vector<std::string> validTypes = {"string", "number", "boolean", "object", "array"};
        if (std::find(validTypes.begin(), validTypes.end(), type_) == validTypes.end()) {
            errors.push_back("Invalid type hint: " + type_);
        }
    }

    return errors;
}

std::string AssignAction::getSpecificDescription() const {
    std::string desc = location_ + " = " + expr_;
    if (!type_.empty()) {
        desc += " (type: " + type_ + ")";
    }
    return desc;
}

bool AssignAction::isValidLocation(const std::string &location) const {
    if (location.empty()) {
        return false;
    }

    // Allow simple variable names and dot notation paths
    // This validates JavaScript-like variable paths
    std::regex locationPattern("^[a-zA-Z_$][a-zA-Z0-9_$]*(\\.[a-zA-Z_$][a-zA-Z0-9_$]*)*$");
    return std::regex_match(location, locationPattern);
}

}  // namespace RSM