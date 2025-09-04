#include "runtime/executors/LogActionExecutor.h"
#include "core/actions/LogActionNode.h"
#include "runtime/DataModelEngine.h"
#include "common/Logger.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace SCXML {
namespace Runtime {

bool LogActionExecutor::execute(const Core::ActionNode& actionNode, RuntimeContext& context) {
    // Cast to specific type
    const auto* logNode = safeCast<Core::LogActionNode>(actionNode);
    if (!logNode) {
        logExecutionError("log", "Invalid action node type for LogActionExecutor", context);
        return false;
    }

    // Validate configuration
    auto errors = validate(actionNode);
    if (!errors.empty()) {
        SCXML::Common::Logger::error("LogActionExecutor::execute - Validation errors:");
        for (const auto& error : errors) {
            SCXML::Common::Logger::error("  " + error);
        }
        logExecutionError("log", "Validation failed", context);
        return false;
    }

    try {
        // Resolve the log message
        std::string message = resolveLogMessage(*logNode, context);

        // Format the message with label
        std::string formattedMessage = formatLogMessage(*logNode, message);

        // Output based on log level
        std::string level = logNode->getLevel();
        std::transform(level.begin(), level.end(), level.begin(), ::tolower);

        if (level == "debug") {
            SCXML::Common::Logger::debug(formattedMessage);
        } else if (level == "warning" || level == "warn") {
            SCXML::Common::Logger::warning(formattedMessage);
        } else if (level == "error") {
            SCXML::Common::Logger::error(formattedMessage);
        } else {
            // Default to info level
            SCXML::Common::Logger::info(formattedMessage);
        }

        SCXML::Common::Logger::debug("LogActionExecutor::execute - Successfully logged message");
        return true;

    } catch (const std::exception& e) {
        logExecutionError("log", "Exception during logging: " + std::string(e.what()), context);
        return false;
    }
}

std::vector<std::string> LogActionExecutor::validate(const Core::ActionNode& actionNode) const {
    std::vector<std::string> errors;

    const auto* logNode = safeCast<Core::LogActionNode>(actionNode);
    if (!logNode) {
        errors.push_back("Invalid action node type for LogActionExecutor");
        return errors;
    }

    // Must have expression to log
    if (logNode->getExpr().empty()) {
        errors.push_back("Log action must have an 'expr' attribute");
    }

    // Validate log level if specified
    const std::string& level = logNode->getLevel();
    if (!level.empty()) {
        std::string lowerLevel = level;
        std::transform(lowerLevel.begin(), lowerLevel.end(), lowerLevel.begin(), ::tolower);

        if (lowerLevel != "debug" && lowerLevel != "info" && lowerLevel != "warning" && 
            lowerLevel != "warn" && lowerLevel != "error") {
            errors.push_back("Invalid log level: " + level + " (valid levels: debug, info, warning, error)");
        }
    }

    return errors;
}

std::string LogActionExecutor::resolveLogMessage(const Core::LogActionNode& logNode, 
                                                RuntimeContext& context) const {
    const std::string& expr = logNode.getExpr();
    if (expr.empty()) {
        return "";
    }

    // Try to evaluate expression using data model engine
    auto dataModel = context.getDataModelEngine();
    if (dataModel) {
        try {
            // Evaluate as ECMAScript expression
            auto result = dataModel->evaluateExpression(expr, context);
            if (result.success) {
                return dataModel->valueToString(result.value);
            } else {
                SCXML::Common::Logger::warning("LogActionExecutor::resolveLogMessage - Expression evaluation failed: " +
                                              result.errorMessage + ", using literal value");
                return expr;  // Fallback to literal value
            }
        } catch (const std::exception& e) {
            SCXML::Common::Logger::warning("LogActionExecutor::resolveLogMessage - Expression evaluation failed: " +
                                          std::string(e.what()) + ", using literal value");
            return expr;  // Fallback to literal value
        }
    } else {
        SCXML::Common::Logger::debug("LogActionExecutor::resolveLogMessage - No data model for expression evaluation, using literal");
        return expr;
    }
}

std::string LogActionExecutor::formatLogMessage(const Core::LogActionNode& logNode, 
                                               const std::string& message) const {
    std::ostringstream formatted;

    // Add label prefix if specified
    const std::string& label = logNode.getLabel();
    if (!label.empty()) {
        formatted << "[" << label << "] ";
    }

    // Add the message
    formatted << message;

    return formatted.str();
}

} // namespace Runtime
} // namespace SCXML