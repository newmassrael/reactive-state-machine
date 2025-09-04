#include "runtime/JavaScriptExpressionEvaluator.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "runtime/DataModelEngine.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

using namespace SCXML;

// ========== Static Regular Expressions ==========

const std::regex JavaScriptExpressionEvaluator::numberRegex_(R"(^-?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$)");
const std::regex JavaScriptExpressionEvaluator::stringRegex_(R"(^(['"])((?:\\.|(?!\1).)*)\1$)");
const std::regex
    JavaScriptExpressionEvaluator::identifierRegex_(R"(^[a-zA-Z_$][a-zA-Z0-9_$]*(?:\.[a-zA-Z_$][a-zA-Z0-9_$]*)*$)");
const std::regex JavaScriptExpressionEvaluator::functionCallRegex_(R"(^([a-zA-Z_$][a-zA-Z0-9_$]*)\s*\(\s*(.*?)\s*\)$)");

// ========== Constructor and Initialization ==========

JavaScriptExpressionEvaluator::JavaScriptExpressionEvaluator() {
    initializeBuiltinFunctions();
    SCXML::Common::Logger::debug("JavaScriptExpressionEvaluator initialized with built-in functions");
}

void JavaScriptExpressionEvaluator::initializeBuiltinFunctions() {
    // Math functions
    builtinFunctions_["abs"] = [](const std::vector<JSValue> &args, const JSEvaluationContext &) -> JSValue {
        if (args.empty()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return std::abs(jsValueToNumber(args[0]));
    };

    builtinFunctions_["max"] = [](const std::vector<JSValue> &args, const JSEvaluationContext &) -> JSValue {
        if (args.empty()) {
            return -std::numeric_limits<double>::infinity();
        }
        double maxVal = jsValueToNumber(args[0]);
        for (size_t i = 1; i < args.size(); ++i) {
            maxVal = std::max(maxVal, jsValueToNumber(args[i]));
        }
        return maxVal;
    };

    builtinFunctions_["min"] = [](const std::vector<JSValue> &args, const JSEvaluationContext &) -> JSValue {
        if (args.empty()) {
            return std::numeric_limits<double>::infinity();
        }
        double minVal = jsValueToNumber(args[0]);
        for (size_t i = 1; i < args.size(); ++i) {
            minVal = std::min(minVal, jsValueToNumber(args[i]));
        }
        return minVal;
    };

    // String functions
    builtinFunctions_["length"] = [](const std::vector<JSValue> &args, const JSEvaluationContext &) -> JSValue {
        if (args.empty()) {
            return 0.0;
        }
        return static_cast<double>(jsValueToString(args[0]).length());
    };

    builtinFunctions_["substring"] = [](const std::vector<JSValue> &args, const JSEvaluationContext &) -> JSValue {
        if (args.size() < 2) {
            return std::string("");
        }
        std::string str = jsValueToString(args[0]);
        int start = static_cast<int>(jsValueToNumber(args[1]));
        int end = args.size() > 2 ? static_cast<int>(jsValueToNumber(args[2])) : static_cast<int>(str.length());

        start = std::max(0, std::min(start, static_cast<int>(str.length())));
        end = std::max(start, std::min(end, static_cast<int>(str.length())));

        return str.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
    };

    // Type checking functions
    builtinFunctions_["typeof"] = [](const std::vector<JSValue> &args, const JSEvaluationContext &) -> JSValue {
        if (args.empty()) {
            return std::string("undefined");
        }

        return std::visit(
            [](const auto &value) -> std::string {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, bool>) {
                    return "boolean";
                } else if constexpr (std::is_same_v<T, double>) {
                    return "number";
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return "string";
                } else {
                    return "undefined";
                }
            },
            args[0]);
    };

    // SCXML-specific functions
    builtinFunctions_["In"] = [](const std::vector<JSValue> &args, const JSEvaluationContext &context) -> JSValue {
        if (args.empty() || !context.runtimeContext) {
            return false;
        }
        std::string stateId = jsValueToString(args[0]);
        return context.runtimeContext->isStateActive(stateId);
    };

    SCXML::Common::Logger::debug("Initialized " + std::to_string(builtinFunctions_.size()) + " built-in functions");
}

// ========== ExpressionEvaluator Interface Implementation ==========

JavaScriptExpressionEvaluator::BooleanResult
JavaScriptExpressionEvaluator::evaluateBoolean(const std::string &expression, const EventContext &context) {
    BooleanResult result;

    try {
        JSEvaluationContext jsContext(context);
        if (context.eventFields.find("__runtime_context__") != context.eventFields.end()) {
            // Extract runtime context if passed through variables
            jsContext.runtimeContext = reinterpret_cast<SCXML::Runtime::RuntimeContext *>(
                std::stoull(context.eventFields.at("__runtime_context__")));
        }

        JSValue jsResult = evaluateJSExpression(expression, jsContext);
        result.success = true;
        result.value = jsValueToBoolean(jsResult);

        SCXML::Common::Logger::debug("JavaScriptExpressionEvaluator::evaluateBoolean - Expression: '" + expression + "' -> " +
                      (result.value ? "true" : "false"));

    } catch (const std::exception &e) {
        result.success = false;
        result.error = "JavaScript evaluation error: " + std::string(e.what());
        SCXML::Common::Logger::error("JavaScriptExpressionEvaluator::evaluateBoolean - " + result.error);
    }

    return result;
}

JavaScriptExpressionEvaluator::StringResult JavaScriptExpressionEvaluator::evaluateString(const std::string &expression,
                                                                                          const EventContext &context) {
    StringResult result;

    try {
        JSEvaluationContext jsContext(context);
        if (context.eventFields.find("__runtime_context__") != context.eventFields.end()) {
            jsContext.runtimeContext = reinterpret_cast<SCXML::Runtime::RuntimeContext *>(
                std::stoull(context.eventFields.at("__runtime_context__")));
        }

        JSValue jsResult = evaluateJSExpression(expression, jsContext);
        result.success = true;
        result.value = jsValueToString(jsResult);

    } catch (const std::exception &e) {
        result.success = false;
        result.error = "JavaScript evaluation error: " + std::string(e.what());
    }

    return result;
}

SCXML::Runtime::ExpressionEvaluator::EvaluationResult
JavaScriptExpressionEvaluator::evaluateNumber(const std::string &expression, const DataModel & /* dataModel */,
                                              const EventContext *context) {
    SCXML::Runtime::ExpressionEvaluator::EvaluationResult result;

    try {
        JSEvaluationContext jsContext;
        if (context) {
            jsContext = JSEvaluationContext(*context);
            if (context->eventFields.find("__runtime_context__") != context->eventFields.end()) {
                jsContext.runtimeContext = reinterpret_cast<SCXML::Runtime::RuntimeContext *>(
                    std::stoull(context->eventFields.at("__runtime_context__")));
            }
        }

        JSValue jsResult = evaluateJSExpression(expression, jsContext);
        result.success = true;
        result.value = jsValueToNumber(jsResult);

    } catch (const std::exception &e) {
        result.success = false;
        result.error = "JavaScript evaluation error: " + std::string(e.what());
    }

    return result;
}

bool JavaScriptExpressionEvaluator::isValidExpression(const std::string &expression) const {
    try {
        // Basic syntax validation - could be enhanced with a full parser
        std::string trimmed = trimExpression(expression);

        // Check for balanced parentheses
        int parenthesesCount = 0;
        for (char c : trimmed) {
            if (c == '(') {
                parenthesesCount++;
            } else if (c == ')') {
                parenthesesCount--;
            }
            if (parenthesesCount < 0) {
                return false;
            }
        }

        return parenthesesCount == 0 && !trimmed.empty();

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("JavaScriptExpressionEvaluator::isValidExpression - Exception validating expression: " +
                      std::string(e.what()));
        return false;
    } catch (...) {
        SCXML::Common::Logger::error("JavaScriptExpressionEvaluator::isValidExpression - Unknown exception validating expression");
        return false;
    }
}

// ========== JavaScript Expression Evaluation ==========

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::evaluateJSExpression(const std::string &expression, const JSEvaluationContext &context) {
    std::string trimmed = trimExpression(expression);

    if (trimmed.empty()) {
        throw std::runtime_error("Empty expression");
    }

    SCXML::Common::Logger::debug("JavaScriptExpressionEvaluator::evaluateJSExpression - Evaluating: '" + trimmed + "'");

    // Parse expression using operator precedence
    return evaluateConditional(trimmed, context);
}

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::evaluateConditional(const std::string &expression, const JSEvaluationContext &context) {
    // Handle ternary operator (condition ? trueExpr : falseExpr)
    size_t questionPos = std::string::npos;
    int parenthesesLevel = 0;

    for (size_t i = 0; i < expression.length(); ++i) {
        char c = expression[i];
        if (c == '(') {
            parenthesesLevel++;
        } else if (c == ')') {
            parenthesesLevel--;
        } else if (c == '?' && parenthesesLevel == 0) {
            questionPos = i;
            break;
        }
    }

    if (questionPos != std::string::npos) {
        // Find the colon
        size_t colonPos = std::string::npos;
        parenthesesLevel = 0;

        for (size_t i = questionPos + 1; i < expression.length(); ++i) {
            char c = expression[i];
            if (c == '(') {
                parenthesesLevel++;
            } else if (c == ')') {
                parenthesesLevel--;
            } else if (c == ':' && parenthesesLevel == 0) {
                colonPos = i;
                break;
            }
        }

        if (colonPos != std::string::npos) {
            std::string condition = trimExpression(expression.substr(0, questionPos));
            std::string trueExpr = trimExpression(expression.substr(questionPos + 1, colonPos - questionPos - 1));
            std::string falseExpr = trimExpression(expression.substr(colonPos + 1));

            JSValue condResult = evaluateLogicalOr(condition, context);
            return jsValueToBoolean(condResult) ? evaluateConditional(trueExpr, context)
                                                : evaluateConditional(falseExpr, context);
        }
    }

    return evaluateLogicalOr(expression, context);
}

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::evaluateLogicalOr(const std::string &expression, const JSEvaluationContext &context) {
    std::vector<std::string> parts = splitExpression(expression, {"||"});

    if (parts.size() == 1) {
        return evaluateLogicalAnd(parts[0], context);
    }

    // Evaluate left to right with short-circuit evaluation
    for (size_t i = 0; i < parts.size(); i += 2) {
        JSValue result = evaluateLogicalAnd(parts[i], context);
        if (jsValueToBoolean(result)) {
            return result;  // Short-circuit: first truthy value
        }
    }

    // All were falsy, return the last value
    return evaluateLogicalAnd(parts[parts.size() - 2], context);
}

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::evaluateLogicalAnd(const std::string &expression, const JSEvaluationContext &context) {
    std::vector<std::string> parts = splitExpression(expression, {"&&"});

    if (parts.size() == 1) {
        return evaluateEquality(parts[0], context);
    }

    // Evaluate left to right with short-circuit evaluation
    for (size_t i = 0; i < parts.size(); i += 2) {
        JSValue result = evaluateEquality(parts[i], context);
        if (!jsValueToBoolean(result)) {
            return result;  // Short-circuit: first falsy value
        }
    }

    // All were truthy, return the last value
    return evaluateEquality(parts[parts.size() - 2], context);
}

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::evaluateEquality(const std::string &expression, const JSEvaluationContext &context) {
    std::vector<std::string> parts = splitExpression(expression, {"===", "!==", "==", "!="});

    if (parts.size() == 1) {
        return evaluateRelational(parts[0], context);
    }

    JSValue left = evaluateRelational(parts[0], context);

    for (size_t i = 1; i < parts.size(); i += 2) {
        std::string op = parts[i];
        JSValue right = evaluateRelational(parts[i + 1], context);

        if (op == "===") {
            // Strict equality
            left = JSValue(left.index() == right.index() && jsValueToString(left) == jsValueToString(right));
        } else if (op == "!==") {
            // Strict inequality
            left = JSValue(left.index() != right.index() || jsValueToString(left) != jsValueToString(right));
        } else if (op == "==") {
            // Loose equality with type coercion
            left = JSValue(jsValueToString(left) == jsValueToString(right));
        } else if (op == "!=") {
            // Loose inequality with type coercion
            left = JSValue(jsValueToString(left) != jsValueToString(right));
        }
    }

    return left;
}

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::evaluateRelational(const std::string &expression, const JSEvaluationContext &context) {
    std::vector<std::string> parts = splitExpression(expression, {"<=", ">=", "<", ">"});

    if (parts.size() == 1) {
        return evaluateAdditive(parts[0], context);
    }

    JSValue left = evaluateAdditive(parts[0], context);

    for (size_t i = 1; i < parts.size(); i += 2) {
        std::string op = parts[i];
        JSValue right = evaluateAdditive(parts[i + 1], context);

        double leftNum = jsValueToNumber(left);
        double rightNum = jsValueToNumber(right);

        if (op == "<") {
            left = JSValue(leftNum < rightNum);
        } else if (op == "<=") {
            left = JSValue(leftNum <= rightNum);
        } else if (op == ">") {
            left = JSValue(leftNum > rightNum);
        } else if (op == ">=") {
            left = JSValue(leftNum >= rightNum);
        }
    }

    return left;
}

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::evaluateAdditive(const std::string &expression, const JSEvaluationContext &context) {
    std::vector<std::string> parts = splitExpression(expression, {"+", "-"});

    if (parts.size() == 1) {
        return evaluateMultiplicative(parts[0], context);
    }

    JSValue left = evaluateMultiplicative(parts[0], context);

    for (size_t i = 1; i < parts.size(); i += 2) {
        std::string op = parts[i];
        JSValue right = evaluateMultiplicative(parts[i + 1], context);

        if (op == "+") {
            // JavaScript + operator: if either operand is string, concatenate
            if (std::holds_alternative<std::string>(left) || std::holds_alternative<std::string>(right)) {
                left = JSValue(jsValueToString(left) + jsValueToString(right));
            } else {
                left = JSValue(jsValueToNumber(left) + jsValueToNumber(right));
            }
        } else if (op == "-") {
            left = JSValue(jsValueToNumber(left) - jsValueToNumber(right));
        }
    }

    return left;
}

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::evaluateMultiplicative(const std::string &expression,
                                                      const JSEvaluationContext &context) {
    std::vector<std::string> parts = splitExpression(expression, {"*", "/", "%"});

    if (parts.size() == 1) {
        return evaluateUnary(parts[0], context);
    }

    JSValue left = evaluateUnary(parts[0], context);

    for (size_t i = 1; i < parts.size(); i += 2) {
        std::string op = parts[i];
        JSValue right = evaluateUnary(parts[i + 1], context);

        double leftNum = jsValueToNumber(left);
        double rightNum = jsValueToNumber(right);

        if (op == "*") {
            left = JSValue(leftNum * rightNum);
        } else if (op == "/") {
            left = JSValue(rightNum != 0.0 ? leftNum / rightNum
                                           : (leftNum > 0   ? std::numeric_limits<double>::infinity()
                                              : leftNum < 0 ? -std::numeric_limits<double>::infinity()
                                                            : std::numeric_limits<double>::quiet_NaN()));
        } else if (op == "%") {
            left = JSValue(rightNum != 0.0 ? std::fmod(leftNum, rightNum) : std::numeric_limits<double>::quiet_NaN());
        }
    }

    return left;
}

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::evaluateUnary(const std::string &expression, const JSEvaluationContext &context) {
    std::string trimmed = trimExpression(expression);

    if (trimmed.empty()) {
        throw std::runtime_error("Empty unary expression");
    }

    // Handle unary operators
    if (trimmed[0] == '!') {
        JSValue operand = evaluateUnary(trimExpression(trimmed.substr(1)), context);
        return JSValue(!jsValueToBoolean(operand));
    }

    if (trimmed[0] == '-') {
        JSValue operand = evaluateUnary(trimExpression(trimmed.substr(1)), context);
        return JSValue(-jsValueToNumber(operand));
    }

    if (trimmed[0] == '+') {
        JSValue operand = evaluateUnary(trimExpression(trimmed.substr(1)), context);
        return JSValue(jsValueToNumber(operand));
    }

    return evaluatePrimary(trimmed, context);
}

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::evaluatePrimary(const std::string &expression, const JSEvaluationContext &context) {
    std::string trimmed = trimExpression(expression);

    if (trimmed.empty()) {
        throw std::runtime_error("Empty primary expression");
    }

    // Handle parentheses
    if (trimmed[0] == '(' && trimmed.back() == ')') {
        return evaluateJSExpression(trimmed.substr(1, trimmed.length() - 2), context);
    }

    // Handle literals
    if (trimmed == "true") {
        return JSValue(true);
    }
    if (trimmed == "false") {
        return JSValue(false);
    }
    if (trimmed == "null" || trimmed == "undefined") {
        return JSValue(nullptr);
    }

    // Handle number literals
    if (std::regex_match(trimmed, numberRegex_)) {
        return JSValue(parseNumberLiteral(trimmed));
    }

    // Handle string literals
    std::smatch stringMatch;
    if (std::regex_match(trimmed, stringMatch, stringRegex_)) {
        return JSValue(parseStringLiteral(trimmed));
    }

    // Handle function calls
    std::smatch funcMatch;
    if (std::regex_match(trimmed, funcMatch, functionCallRegex_)) {
        std::string funcName = funcMatch[1].str();
        std::string argsStr = funcMatch[2].str();

        // Parse arguments (simplified - doesn't handle nested function calls perfectly)
        std::vector<JSValue> args;
        if (!argsStr.empty()) {
            std::stringstream ss(argsStr);
            std::string arg;
            while (std::getline(ss, arg, ',')) {
                args.push_back(evaluateJSExpression(trimExpression(arg), context));
            }
        }

        return callBuiltinFunction(funcName, args, context);
    }

    // Handle event data access
    if (isEventDataReference(trimmed)) {
        return accessEventData(trimmed, context);
    }

    // Handle variable access (including data model variables)
    if (std::regex_match(trimmed, identifierRegex_)) {
        return accessDataModelVariable(trimmed, context);
    }

    throw std::runtime_error("Invalid primary expression: " + trimmed);
}

// ========== Data Model and Event Access ==========

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::accessDataModelVariable(const std::string &variableName,
                                                       const JSEvaluationContext &context) {
    // First check JS variables
    auto jsVarIt = globalJSVariables_.find(variableName);
    if (jsVarIt != globalJSVariables_.end()) {
        return jsVarIt->second;
    }

    // Check context JS variables
    auto ctxVarIt = context.jsVariables.find(variableName);
    if (ctxVarIt != context.jsVariables.end()) {
        return ctxVarIt->second;
    }

    // Check legacy variables
    auto legacyVarIt = context.eventFields.find(variableName);
    if (legacyVarIt != context.eventFields.end()) {
        return JSValue(legacyVarIt->second);
    }

    // Try data model engine if available
    if (context.runtimeContext) {
        auto dataEngine = context.runtimeContext->getDataModelEngine();
        if (dataEngine) {
            auto result = dataEngine->getValue(variableName);
            if (result.success) {
                // Convert DataValue to JSValue - create shared_ptr wrapper for compatibility
                // Note: This creates a non-owning shared_ptr, which is safe if dataEngine outlives this call
                std::shared_ptr<SCXML::DataModelEngine> sharedEngine(dataEngine, [](SCXML::DataModelEngine *) {});
                return convertDataValueToJSValue(result.value, sharedEngine);
            }
        }

        // Fallback to RuntimeContext direct data access
        if (context.runtimeContext->hasDataValue(variableName)) {
            std::string value = context.runtimeContext->getDataValue(variableName);
            return JSValue(value);
        }
    }

    SCXML::Common::Logger::debug("JavaScriptExpressionEvaluator::accessDataModelVariable - Variable not found: " + variableName);
    return JSValue(nullptr);  // undefined
}

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::accessEventData(const std::string &eventPath, const JSEvaluationContext &context) {
    if (!context.currentEvent) {
        SCXML::Common::Logger::debug("JavaScriptExpressionEvaluator::accessEventData - No current event for path: " + eventPath);
        return JSValue(nullptr);
    }

    if (eventPath == "_event") {
        return JSValue(std::string("Event"));  // Event object reference
    }

    if (eventPath == "_event.name") {
        return JSValue(context.currentEvent->getName());
    }

    if (eventPath == "_event.type") {
        return JSValue(context.currentEvent->getName());  // In SCXML, type is same as name
    }

    // Handle _event.data access (simplified - would need full event data parsing)
    if (eventPath.find("_event.data") == 0) {
        // For now, return string representation
        // A full implementation would parse the event data structure
        return JSValue(std::string("eventdata"));
    }

    SCXML::Common::Logger::debug("JavaScriptExpressionEvaluator::accessEventData - Unknown event path: " + eventPath);
    return JSValue(nullptr);
}

bool JavaScriptExpressionEvaluator::isEventDataReference(const std::string &identifier) {
    return identifier.find("_event") == 0;
}

// ========== Built-in Function Calls ==========

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::callBuiltinFunction(const std::string &functionName, const std::vector<JSValue> &args,
                                                   const JSEvaluationContext &context) {
    auto funcIt = builtinFunctions_.find(functionName);
    if (funcIt != builtinFunctions_.end()) {
        return funcIt->second(args, context);
    }

    throw std::runtime_error("Unknown function: " + functionName);
}

// ========== JavaScript Variables Management ==========

void JavaScriptExpressionEvaluator::setJSVariable(const std::string &name, const JSValue &value) {
    globalJSVariables_[name] = value;
    SCXML::Common::Logger::debug("JavaScriptExpressionEvaluator::setJSVariable - " + name + " = " + jsValueToString(value));
}

JavaScriptExpressionEvaluator::JSValue JavaScriptExpressionEvaluator::getJSVariable(const std::string &name) const {
    auto it = globalJSVariables_.find(name);
    return it != globalJSVariables_.end() ? it->second : JSValue(nullptr);
}

void JavaScriptExpressionEvaluator::clearJSVariables() {
    globalJSVariables_.clear();
    SCXML::Common::Logger::debug("JavaScriptExpressionEvaluator::clearJSVariables - All variables cleared");
}

// ========== JavaScript Value Conversions ==========

bool JavaScriptExpressionEvaluator::jsValueToBoolean(const JSValue &value) {
    return std::visit(
        [](const auto &v) -> bool {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                return v;
            } else if constexpr (std::is_same_v<T, double>) {
                return v != 0.0 && !std::isnan(v);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return !v.empty();
            } else {
                return false;  // null/undefined
            }
        },
        value);
}

std::string JavaScriptExpressionEvaluator::jsValueToString(const JSValue &value) {
    return std::visit(
        [](const auto &v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                return v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, double>) {
                if (std::isnan(v)) {
                    return "NaN";
                }
                if (std::isinf(v)) {
                    return v > 0 ? "Infinity" : "-Infinity";
                }
                return std::to_string(v);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return v;
            } else {
                return "null";  // null/undefined
            }
        },
        value);
}

double JavaScriptExpressionEvaluator::jsValueToNumber(const JSValue &value) {
    return std::visit(
        [](const auto &v) -> double {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                return v ? 1.0 : 0.0;
            } else if constexpr (std::is_same_v<T, double>) {
                return v;
            } else if constexpr (std::is_same_v<T, std::string>) {
                try {
                    if (v.empty()) {
                        return 0.0;
                    }
                    return std::stod(v);
                } catch (...) {
                    return std::numeric_limits<double>::quiet_NaN();
                }
            } else {
                return 0.0;  // null/undefined -> 0
            }
        },
        value);
}

// ========== Utility Methods ==========

std::string JavaScriptExpressionEvaluator::trimExpression(const std::string &expression) {
    size_t start = expression.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = expression.find_last_not_of(" \t\n\r");
    return expression.substr(start, end - start + 1);
}

std::vector<std::string> JavaScriptExpressionEvaluator::splitExpression(const std::string &expression,
                                                                        const std::vector<std::string> &operators) {
    std::vector<std::string> result;
    result.push_back(expression);

    for (const auto &op : operators) {
        std::vector<std::string> newResult;

        for (const auto &part : result) {
            size_t pos = 0;
            size_t lastPos = 0;
            int parenthesesLevel = 0;

            while (pos < part.length()) {
                char c = part[pos];
                if (c == '(') {
                    parenthesesLevel++;
                } else if (c == ')') {
                    parenthesesLevel--;
                } else if (parenthesesLevel == 0 && part.substr(pos, op.length()) == op) {
                    // Found operator at top level
                    newResult.push_back(trimExpression(part.substr(lastPos, pos - lastPos)));
                    newResult.push_back(op);
                    lastPos = pos + op.length();
                    pos = lastPos - 1;  // -1 because pos will be incremented
                }
                pos++;
            }

            if (lastPos < part.length()) {
                newResult.push_back(trimExpression(part.substr(lastPos)));
            }
        }

        if (newResult.size() > 1) {
            result = newResult;
        }
    }

    return result;
}

size_t JavaScriptExpressionEvaluator::findMatchingParenthesis(const std::string &expression, size_t startPos) {
    if (startPos >= expression.length() || expression[startPos] != '(') {
        return std::string::npos;
    }

    int level = 1;
    for (size_t i = startPos + 1; i < expression.length(); ++i) {
        if (expression[i] == '(') {
            level++;
        } else if (expression[i] == ')') {
            level--;
        }

        if (level == 0) {
            return i;
        }
    }

    return std::string::npos;
}

std::string JavaScriptExpressionEvaluator::parseStringLiteral(const std::string &literal) {
    if (literal.length() < 2) {
        return "";
    }

    char quote = literal[0];
    if (quote != '"' && quote != '\'') {
        return literal;
    }

    std::string result;
    for (size_t i = 1; i < literal.length() - 1; ++i) {
        char c = literal[i];
        if (c == '\\' && i + 1 < literal.length() - 1) {
            char next = literal[i + 1];
            switch (next) {
            case 'n':
                result += '\n';
                break;
            case 't':
                result += '\t';
                break;
            case 'r':
                result += '\r';
                break;
            case '\\':
                result += '\\';
                break;
            case '\'':
                result += '\'';
                break;
            case '"':
                result += '"';
                break;
            default:
                result += next;
                break;
            }
            i++;  // Skip the next character
        } else {
            result += c;
        }
    }

    return result;
}

double JavaScriptExpressionEvaluator::parseNumberLiteral(const std::string &literal) {
    try {
        return std::stod(literal);
    } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

JavaScriptExpressionEvaluator::JSValue
JavaScriptExpressionEvaluator::convertDataValueToJSValue(const SCXML::DataModelEngine::DataValue &dataValue,
                                                         std::shared_ptr<SCXML::DataModelEngine> dataEngine) {
    if (!dataEngine) {
        return JSValue(nullptr);
    }

    using DataValue = SCXML::DataModelEngine::DataValue;

    return std::visit(
        [&dataEngine](const auto &value) -> JSValue {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, std::monostate>) {
                return JSValue(nullptr);
            } else if constexpr (std::is_same_v<T, bool>) {
                return JSValue(value);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return JSValue(static_cast<double>(value));
            } else if constexpr (std::is_same_v<T, double>) {
                return JSValue(value);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return JSValue(value);
            } else if constexpr (std::is_same_v<T, std::vector<DataValue>>) {
                // For arrays, convert to string representation for now
                // Could be enhanced to support proper array operations
                if (dataEngine) {
                    return JSValue(dataEngine->valueToString(DataValue(value)));
                }
                return JSValue(std::string("[Array]"));
            } else if constexpr (std::is_same_v<T, std::unordered_map<std::string, DataValue>>) {
                // For objects, convert to string representation for now
                // Could be enhanced to support property access
                if (dataEngine) {
                    return JSValue(dataEngine->valueToString(DataValue(value)));
                }
                return JSValue(std::string("[Object]"));
            } else {
                return JSValue(nullptr);
            }
        },
        dataValue);
}