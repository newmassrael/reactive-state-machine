#include "runtime/executors/AssignActionExecutor.h"
#include "core/actions/AssignActionNode.h"
#include "runtime/DataModelEngine.h"
#include "common/Logger.h"
#include <stdexcept>

namespace SCXML {
namespace Runtime {

bool AssignActionExecutor::execute(const Core::ActionNode& actionNode, RuntimeContext& context) {
    // Cast to specific type
    const auto* assignNode = safeCast<Core::AssignActionNode>(actionNode);
    if (!assignNode) {
        logExecutionError("assign", "Invalid action node type for AssignActionExecutor", context);
        return false;
    }

    // Validate configuration
    auto errors = validate(actionNode);
    if (!errors.empty()) {
        SCXML::Common::Logger::error("AssignActionExecutor::execute - Validation errors:");
        for (const auto& error : errors) {
            SCXML::Common::Logger::error("  " + error);
        }
        logExecutionError("assign", "Validation failed", context);
        return false;
    }

    try {
        // Resolve the value to assign
        std::string value = resolveValue(*assignNode, context);
        
        if (value.empty() && !assignNode->getExpr().empty() && !assignNode->getAttr().empty()) {
            logExecutionError("assign", "Failed to resolve value for assignment", context);
            return false;
        }

        // Get data model engine from context
        auto dataModel = context.getDataModelEngine();
        if (!dataModel) {
            logExecutionError("assign", "No data model engine available", context);
            return false;
        }

        // Perform the assignment
        const std::string& location = assignNode->getLocation();
        auto setResult = dataModel->setValue(location, value);
        bool success = setResult.success;

        if (success) {
            SCXML::Common::Logger::debug("AssignActionExecutor::execute - Successfully assigned '" + 
                                       value + "' to location '" + location + "'");
            return true;
        } else {
            logExecutionError("assign", "Failed to assign to location: " + location, context);
            return false;
        }

    } catch (const std::exception& e) {
        logExecutionError("assign", "Exception during assignment: " + std::string(e.what()), context);
        return false;
    }
}

std::vector<std::string> AssignActionExecutor::validate(const Core::ActionNode& actionNode) const {
    std::vector<std::string> errors;

    const auto* assignNode = safeCast<Core::AssignActionNode>(actionNode);
    if (!assignNode) {
        errors.push_back("Invalid action node type for AssignActionExecutor");
        return errors;
    }

    // Must have location
    if (assignNode->getLocation().empty()) {
        errors.push_back("Assign action must have a 'location' attribute");
    }

    // Must have either expr or attr (but not both)
    const std::string& expr = assignNode->getExpr();
    const std::string& attr = assignNode->getAttr();
    
    if (expr.empty() && attr.empty()) {
        errors.push_back("Assign action must have either 'expr' or 'attr' attribute");
    } else if (!expr.empty() && !attr.empty()) {
        errors.push_back("Assign action cannot have both 'expr' and 'attr' attributes");
    }

    return errors;
}

std::string AssignActionExecutor::resolveValue(const Core::AssignActionNode& assignNode, 
                                             RuntimeContext& context) const {
    const std::string& expr = assignNode.getExpr();
    const std::string& attr = assignNode.getAttr();

    if (!expr.empty()) {
        // Evaluate expression using data model engine
        auto dataModel = context.getDataModelEngine();
        
        if (dataModel) {
            try {
                // Try to evaluate as ECMAScript expression
                auto result = dataModel->evaluateExpression(expr, context);
                if (result.success) {
                    return dataModel->valueToString(result.value);
                } else {
                    throw std::runtime_error("Expression evaluation failed: " + result.errorMessage);
                }
            } catch (const std::exception& e) {
                SCXML::Common::Logger::warning("AssignActionExecutor::resolveValue - Expression evaluation failed: " +
                                              std::string(e.what()) + ", using literal value");
                return expr;  // Fallback to literal value
            }
        } else {
            SCXML::Common::Logger::warning("AssignActionExecutor::resolveValue - No data model for expression evaluation, using literal");
            return expr;
        }
    } else if (!attr.empty()) {
        // Read from context attribute
        // This would typically read from the current event's data or context variables
        SCXML::Common::Logger::warning("AssignActionExecutor::resolveValue - Attribute resolution not fully implemented: " + attr);
        return attr;  // Placeholder implementation
    }

    return "";
}

} // namespace Runtime
} // namespace SCXML