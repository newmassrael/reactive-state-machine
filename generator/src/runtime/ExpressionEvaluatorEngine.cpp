#include "runtime/ExpressionEvaluatorEngine.h"

namespace SCXML {
namespace Runtime {
#include "common/Logger.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>

EnhancedExpressionEvaluator::EnhancedExpressionEvaluator() {
    Logger::debug("EnhancedExpressionEvaluator::Constructor - Initializing enhanced expression evaluator");
}

SCXML::Common::Result<bool> EnhancedExpressionEvaluator::evaluateBooleanExpression(
    SCXML::Runtime::RuntimeContext &context, const std::string &expression, const ExpressionContext &exprContext) {
    Logger::debug("EnhancedExpressionEvaluator::evaluateBooleanExpression - Evaluating: " + expression);

    try {
        // Preprocess the expression
        std::string processedExpr = preprocessExpression(expression, exprContext);

        // Evaluate using data model engine
        auto *dataModel = context.getDataModelEngine();
        if (!dataModel) {
            return SCXML::Common::Result<bool>(SCXML::Common::ErrorInfo{
                "EnhancedExpressionEvaluator::evaluateBooleanExpression", "No data model engine available",
                "RuntimeContext must have an initialized data model engine"});
        }

        auto result = dataModel->evaluateExpression(processedExpr);
        if (!result.isSuccess()) {
            return SCXML::Common::Result<bool>(
                SCXML::Common::ErrorInfo{"EnhancedExpressionEvaluator::evaluateBooleanExpression",
                                         "Expression evaluation failed: " + expression, result.getErrorMessage()});
        }

        // Convert to boolean
        std::string stringResult = dataModel->convertToString(result.value);
        bool boolResult = convertToBoolean(stringResult);

        Logger::debug("EnhancedExpressionEvaluator::evaluateBooleanExpression - Result: " +
                      std::string(boolResult ? "true" : "false"));

        return SCXML::Common::Result<bool>(boolResult);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<bool>(SCXML::Common::ErrorInfo{
            "EnhancedExpressionEvaluator::evaluateBooleanExpression", "Boolean expression evaluation error", e.what()});
    }
}

SCXML::Common::Result<double> EnhancedExpressionEvaluator::evaluateArithmeticExpression(
    SCXML::Runtime::RuntimeContext &context, const std::string &expression, const ExpressionContext &exprContext) {
    Logger::debug("EnhancedExpressionEvaluator::evaluateArithmeticExpression - Evaluating: " + expression);

    try {
        std::string processedExpr = preprocessExpression(expression, exprContext);

        auto *dataModel = context.getDataModelEngine();
        if (!dataModel) {
            return SCXML::Common::Result<double>(SCXML::Common::ErrorInfo{
                "EnhancedExpressionEvaluator::evaluateArithmeticExpression", "No data model engine available",
                "RuntimeContext must have an initialized data model engine"});
        }

        auto result = dataModel->evaluateExpression(processedExpr);
        if (!result.isSuccess()) {
            return SCXML::Common::Result<double>(SCXML::Common::ErrorInfo{
                "EnhancedExpressionEvaluator::evaluateArithmeticExpression",
                "Arithmetic expression evaluation failed: " + expression, result.getErrorMessage()});
        }

        // Convert to numeric value
        std::string stringResult = dataModel->convertToString(result.value);
        double numericResult;

        try {
            numericResult = std::stod(stringResult);
        } catch (const std::exception &) {
            return SCXML::Common::Result<double>(SCXML::Common::ErrorInfo{
                "EnhancedExpressionEvaluator::evaluateArithmeticExpression",
                "Cannot convert result to number: " + stringResult, "Expression did not evaluate to a numeric value"});
        }

        Logger::debug("EnhancedExpressionEvaluator::evaluateArithmeticExpression - Result: " +
                      std::to_string(numericResult));

        return SCXML::Common::Result<double>(numericResult);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<double>(
            SCXML::Common::ErrorInfo{"EnhancedExpressionEvaluator::evaluateArithmeticExpression",
                                     "Arithmetic expression evaluation error", e.what()});
    }
}

SCXML::Common::Result<std::string> EnhancedExpressionEvaluator::evaluateStringExpression(
    SCXML::Runtime::RuntimeContext &context, const std::string &expression, const ExpressionContext &exprContext) {
    Logger::debug("EnhancedExpressionEvaluator::evaluateStringExpression - Evaluating: " + expression);

    try {
        std::string processedExpr = preprocessExpression(expression, exprContext);

        auto *dataModel = context.getDataModelEngine();
        if (!dataModel) {
            return SCXML::Common::Result<std::string>(SCXML::Common::ErrorInfo{
                "EnhancedExpressionEvaluator::evaluateStringExpression", "No data model engine available",
                "RuntimeContext must have an initialized data model engine"});
        }

        auto result = dataModel->evaluateExpression(processedExpr);
        if (!result.isSuccess()) {
            return SCXML::Common::Result<std::string>(SCXML::Common::ErrorInfo{
                "EnhancedExpressionEvaluator::evaluateStringExpression",
                "String expression evaluation failed: " + expression, result.getErrorMessage()});
        }

        std::string stringResult = dataModel->convertToString(result.value);
        Logger::debug("EnhancedExpressionEvaluator::evaluateStringExpression - Result: " + stringResult);

        return SCXML::Common::Result<std::string>(stringResult);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>(SCXML::Common::ErrorInfo{
            "EnhancedExpressionEvaluator::evaluateStringExpression", "String expression evaluation error", e.what()});
    }
}

SCXML::Common::Result<std::string>
EnhancedExpressionEvaluator::evaluateExpression(SCXML::Runtime::RuntimeContext &context, const std::string &expression,
                                                const ExpressionContext &exprContext) {
    Logger::debug("EnhancedExpressionEvaluator::evaluateExpression - Evaluating: " + expression);

    // Determine expression type and delegate to appropriate evaluator
    ExpressionType type = detectExpressionType(expression);

    switch (type) {
    case ExpressionType::BOOLEAN: {
        auto result = evaluateBooleanExpression(context, expression, exprContext);
        if (result.isSuccess()) {
            return SCXML::Common::Result<std::string>(result.getValue() ? "true" : "false");
        }
        return SCXML::Common::Result<std::string>(result.getErrors()[0]);
    }

    case ExpressionType::ARITHMETIC: {
        auto result = evaluateArithmeticExpression(context, expression, exprContext);
        if (result.isSuccess()) {
            return SCXML::Common::Result<std::string>(std::to_string(result.getValue()));
        }
        return SCXML::Common::Result<std::string>(result.getErrors()[0]);
    }

    case ExpressionType::EVENT_DATA:
        return evaluateEventDataExpression(expression, exprContext);

    case ExpressionType::SYSTEM_VARIABLE:
        return evaluateSystemVariable(expression, exprContext);

    case ExpressionType::STRING:
    case ExpressionType::LOCATION:
    case ExpressionType::FUNCTION_CALL:
    case ExpressionType::COMPLEX:
    default:
        return evaluateStringExpression(context, expression, exprContext);
    }
}

EnhancedExpressionEvaluator::ExpressionType
EnhancedExpressionEvaluator::detectExpressionType(const std::string &expression) {
    // Trim whitespace
    std::string trimmed = expression;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (trimmed.empty()) {
        return ExpressionType::STRING;
    }

    // Check for event data access
    if (trimmed.find("_event") == 0) {
        return ExpressionType::EVENT_DATA;
    }

    // Check for system variables
    if (trimmed.find("_sessionid") == 0 || trimmed.find("_name") == 0 || trimmed.find("_ioprocessors") == 0 ||
        trimmed.find("_x") == 0) {
        return ExpressionType::SYSTEM_VARIABLE;
    }

    // Check for boolean operators/values
    if (trimmed == "true" || trimmed == "false" || trimmed.find("==") != std::string::npos ||
        trimmed.find("!=") != std::string::npos || trimmed.find("&&") != std::string::npos ||
        trimmed.find("||") != std::string::npos || trimmed.find(">=") != std::string::npos ||
        trimmed.find("<=") != std::string::npos || trimmed.find('>') != std::string::npos ||
        trimmed.find('<') != std::string::npos) {
        return ExpressionType::BOOLEAN;
    }

    // Check for arithmetic operators
    if (trimmed.find('+') != std::string::npos || trimmed.find('-') != std::string::npos ||
        trimmed.find('*') != std::string::npos || trimmed.find('/') != std::string::npos ||
        trimmed.find('%') != std::string::npos) {
        return ExpressionType::ARITHMETIC;
    }

    // Check if it's a numeric literal
    try {
        std::stod(trimmed);
        return ExpressionType::ARITHMETIC;
    } catch (const std::exception &) {
        // Not a number
    }

    // Check for function calls
    if (trimmed.find('(') != std::string::npos && trimmed.find(')') != std::string::npos) {
        return ExpressionType::FUNCTION_CALL;
    }

    // Default to string/location
    return ExpressionType::STRING;
}

std::vector<std::string> EnhancedExpressionEvaluator::validateExpression(const std::string &expression) {
    std::vector<std::string> errors;

    if (expression.empty()) {
        errors.push_back("Expression cannot be empty");
        return errors;
    }

    // Check for balanced delimiters
    if (!hasBalancedDelimiters(expression)) {
        errors.push_back("Unbalanced parentheses, brackets, or quotes in expression");
    }

    // Validate function calls
    auto functionErrors = validateFunctionCalls(expression);
    errors.insert(errors.end(), functionErrors.begin(), functionErrors.end());

    // Check for forbidden constructs (for security)
    std::vector<std::string> forbiddenPatterns = {"eval\\(",        "Function\\(",    "setTimeout\\(",
                                                  "setInterval\\(", "XMLHttpRequest", "fetch\\("};

    for (const auto &pattern : forbiddenPatterns) {
        if (std::regex_search(expression, std::regex(pattern))) {
            errors.push_back("Forbidden function or construct: " + pattern);
        }
    }

    return errors;
}

EnhancedExpressionEvaluator::ExpressionContext
EnhancedExpressionEvaluator::createExpressionContext(SCXML::Runtime::RuntimeContext &context) {
    ExpressionContext exprContext;

    // Set session ID
    exprContext.sessionId = context.getSessionId();

    // Set state machine name (if available)
    // This would need to be implemented in RuntimeContext
    exprContext.stateMachineName = "StateMachine";  // placeholder

    // Set available I/O processors
    exprContext.ioProcessors = {"http", "basichttp", "scxml"};

    // Set system variables
    exprContext.systemVars["_sessionid"] = exprContext.sessionId;
    exprContext.systemVars["_name"] = exprContext.stateMachineName;
    exprContext.systemVars["_ioprocessors"] = "['http','basichttp','scxml']";

    Logger::debug("EnhancedExpressionEvaluator::createExpressionContext - Created context for session: " +
                  exprContext.sessionId);

    return exprContext;
}

bool EnhancedExpressionEvaluator::usesEventData(const std::string &expression) {
    return expression.find("_event") != std::string::npos;
}

bool EnhancedExpressionEvaluator::usesSystemVariables(const std::string &expression) {
    return (expression.find("_sessionid") != std::string::npos || expression.find("_name") != std::string::npos ||
            expression.find("_ioprocessors") != std::string::npos || expression.find("_x") != std::string::npos);
}

std::string EnhancedExpressionEvaluator::preprocessExpression(const std::string &expression,
                                                              const ExpressionContext &exprContext) {
    std::string processed = expression;

    // Substitute event data references
    if (usesEventData(processed)) {
        processed = substituteEventData(processed, exprContext);
    }

    // Substitute system variables
    if (usesSystemVariables(processed)) {
        processed = substituteSystemVariables(processed, exprContext);
    }

    Logger::debug("EnhancedExpressionEvaluator::preprocessExpression - Original: " + expression);
    Logger::debug("EnhancedExpressionEvaluator::preprocessExpression - Processed: " + processed);

    return processed;
}

SCXML::Common::Result<std::string>
EnhancedExpressionEvaluator::evaluateEventDataExpression(const std::string &expression,
                                                         const ExpressionContext &exprContext) {
    Logger::debug("EnhancedExpressionEvaluator::evaluateEventDataExpression - Evaluating: " + expression);

    try {
        // Handle _event.name, _event.data.*, etc.
        if (expression == "_event.name") {
            auto it = exprContext.eventData.find("name");
            if (it != exprContext.eventData.end()) {
                return SCXML::Common::Result<std::string>(it->second);
            }
            return SCXML::Common::Result<std::string>("");
        }

        if (expression == "_event.type") {
            auto it = exprContext.eventData.find("type");
            if (it != exprContext.eventData.end()) {
                return SCXML::Common::Result<std::string>(it->second);
            }
            return SCXML::Common::Result<std::string>("platform");
        }

        if (expression.find("_event.data.") == 0) {
            std::string dataKey = expression.substr(12);  // Remove "_event.data."
            auto it = exprContext.eventData.find(dataKey);
            if (it != exprContext.eventData.end()) {
                return SCXML::Common::Result<std::string>(it->second);
            }
            return SCXML::Common::Result<std::string>("");
        }

        return SCXML::Common::Result<std::string>("");

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>(SCXML::Common::ErrorInfo{
            "EnhancedExpressionEvaluator::evaluateEventDataExpression", "Event data evaluation error", e.what()});
    }
}

SCXML::Common::Result<std::string>
EnhancedExpressionEvaluator::evaluateSystemVariable(const std::string &expression,
                                                    const ExpressionContext &exprContext) {
    Logger::debug("EnhancedExpressionEvaluator::evaluateSystemVariable - Evaluating: " + expression);

    auto it = exprContext.systemVars.find(expression);
    if (it != exprContext.systemVars.end()) {
        return SCXML::Common::Result<std::string>(it->second);
    }

    return SCXML::Common::Result<std::string>(
        SCXML::Common::ErrorInfo{"EnhancedExpressionEvaluator::evaluateSystemVariable",
                                 "Unknown system variable: " + expression, "System variable not found in context"});
}

std::string EnhancedExpressionEvaluator::substituteEventData(const std::string &expression,
                                                             const ExpressionContext &exprContext) {
    std::string result = expression;

    // Simple substitution for common event data patterns
    for (const auto &pair : exprContext.eventData) {
        std::string placeholder = "_event.data." + pair.first;
        std::string value = "'" + escapeString(pair.second) + "'";

        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }

    // Handle _event.name
    if (result.find("_event.name") != std::string::npos) {
        auto it = exprContext.eventData.find("name");
        std::string eventName = (it != exprContext.eventData.end()) ? it->second : "";
        std::string replacement = "'" + escapeString(eventName) + "'";

        size_t pos = 0;
        while ((pos = result.find("_event.name", pos)) != std::string::npos) {
            result.replace(pos, 11, replacement);  // "_event.name" is 11 chars
            pos += replacement.length();
        }
    }

    return result;
}

std::string EnhancedExpressionEvaluator::substituteSystemVariables(const std::string &expression,
                                                                   const ExpressionContext &exprContext) {
    std::string result = expression;

    for (const auto &pair : exprContext.systemVars) {
        std::string placeholder = pair.first;
        std::string value = "'" + escapeString(pair.second) + "'";

        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }

    return result;
}

std::vector<std::string> EnhancedExpressionEvaluator::validateFunctionCalls(const std::string &expression) {
    std::vector<std::string> errors;

    // Check for function call patterns
    std::regex functionPattern(R"(\b(\w+)\s*\()");
    std::sregex_iterator iter(expression.begin(), expression.end(), functionPattern);
    std::sregex_iterator end;

    for (; iter != end; ++iter) {
        std::string functionName = iter->str(1);

        // List of allowed functions (this could be configurable)
        std::vector<std::string> allowedFunctions = {
            "parseInt",   "parseFloat", "isNaN",          "isFinite", "Math.abs", "Math.max", "Math.min",
            "Math.round", "Math.floor", "Math.ceil",      "String",   "Number",   "Boolean",  "Array",
            "Object",     "JSON.parse", "JSON.stringify", "Date",     "Date.now"};

        if (std::find(allowedFunctions.begin(), allowedFunctions.end(), functionName) == allowedFunctions.end()) {
            errors.push_back("Function not allowed: " + functionName);
        }
    }

    return errors;
}

bool EnhancedExpressionEvaluator::hasBalancedDelimiters(const std::string &expression) {
    int parentheses = 0;
    int brackets = 0;
    int braces = 0;
    bool inString = false;
    char stringChar = '\0';

    for (size_t i = 0; i < expression.length(); ++i) {
        char c = expression[i];

        if (!inString) {
            switch (c) {
            case '(':
                parentheses++;
                break;
            case ')':
                parentheses--;
                if (parentheses < 0) {
                    return false;
                }
                break;
            case '[':
                brackets++;
                break;
            case ']':
                brackets--;
                if (brackets < 0) {
                    return false;
                }
                break;
            case '{':
                braces++;
                break;
            case '}':
                braces--;
                if (braces < 0) {
                    return false;
                }
                break;
            case '"':
            case '\'':
                inString = true;
                stringChar = c;
                break;
            }
        } else {
            if (c == stringChar && (i == 0 || expression[i - 1] != '\\')) {
                inString = false;
            }
        }
    }

    return parentheses == 0 && brackets == 0 && braces == 0 && !inString;
}

bool EnhancedExpressionEvaluator::convertToBoolean(const std::string &value) {
    if (value.empty() || value == "null" || value == "undefined" || value == "false" || value == "0") {
        return false;
    }

    if (value == "true") {
        return true;
    }

    // Try to parse as number
    try {
        double num = std::stod(value);
        return num != 0.0 && !std::isnan(num);
    } catch (const std::exception &) {
        // Not a number, non-empty string is truthy
        return true;
    }
}

std::string EnhancedExpressionEvaluator::escapeString(const std::string &str) {
    std::string escaped;
    for (char c : str) {
        switch (c) {
        case '\'':
            escaped += "\\'";
            break;
        case '\"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += c;
            break;
        }
    }
    return escaped;
}

}  // namespace Runtime
}  // namespace SCXML