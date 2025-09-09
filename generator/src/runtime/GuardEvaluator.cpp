#include "runtime/GuardEvaluator.h"
#include "model/IGuardNode.h"
#include "model/ITransitionNode.h"
#include "runtime/DataModelEngine.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/RuntimeContext.h"
// #include "runtime/JavaScriptExpressionEvaluator.h" // Temporarily disabled
#include "common/Logger.h"
#include "events/Event.h"
#include <regex>

namespace SCXML {

SCXML::GuardEvaluator::GuardEvaluator() : dataModelEngine_(nullptr) {
    initializeDefaultEvaluator();
}

SCXML::GuardEvaluator::GuardResult
SCXML::GuardEvaluator::evaluateTransitionGuard(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                               const GuardContext &context) {
    GuardResult result;

    if (!transition) {
        result.errorMessage = "Null transition provided";
        result.satisfied = false;
        return result;
    }

    try {
        // Check if transition has guard condition
        if (!hasGuardCondition(transition)) {
            // No guard condition means transition is always enabled
            result.satisfied = true;
            result.hasGuard = false;
            return result;
        }

        result.hasGuard = true;

        // Get guard condition from transition
        std::string guardCondition = transition->getGuard();
        if (!guardCondition.empty()) {
            return evaluateExpression(guardCondition, context);
        }

        // Fall back to expression-based guard
        std::string guardExpression = getGuardExpression(transition);
        if (!guardExpression.empty()) {
            result.guardExpression = guardExpression;
            return evaluateExpression(guardExpression, context);
        }

        // No evaluable guard found
        result.satisfied = true;
        SCXML::Common::Logger::debug(
            "GuardEvaluator::evaluateTransitionGuard - No evaluable guard found for transition");

        return result;

    } catch (const std::exception &e) {
        return handleError("", e.what(), context);
    }
}

SCXML::GuardEvaluator::GuardResult
SCXML::GuardEvaluator::evaluateGuardNode(std::shared_ptr<SCXML::Model::IGuardNode> guard, const GuardContext &context) {
    GuardResult result;
    result.hasGuard = true;

    if (!guard) {
        result.errorMessage = "Null guard node provided";
        result.satisfied = false;
        return result;
    }

    try {
        std::string condition = guard->getCondition();
        result.guardExpression = condition;

        SCXML::Common::Logger::debug("GuardEvaluator::evaluateGuardNode - Evaluating guard node condition: '" +
                                     condition + "'");

        // If no condition is specified, guard is always satisfied
        if (condition.empty()) {
            result.satisfied = true;
            SCXML::Common::Logger::debug("GuardEvaluator::evaluateGuardNode - Empty condition, guard satisfied");
            return result;
        }

        // Use expression evaluation for the condition
        auto exprResult = evaluateExpression(condition, context);
        result.satisfied = exprResult.satisfied;
        result.errorMessage = exprResult.errorMessage;

        // Log dependencies for debugging
        auto dependencies = guard->getDependencies();
        if (!dependencies.empty()) {
            std::ostringstream depStr;
            for (size_t i = 0; i < dependencies.size(); ++i) {
                if (i > 0) {
                    depStr << ", ";
                }
                depStr << dependencies[i];
            }
            SCXML::Common::Logger::debug("GuardEvaluator::evaluateGuardNode - Dependencies: [" + depStr.str() + "]");
        }

        // Handle reactive guards
        if (guard->isReactive()) {
            SCXML::Common::Logger::debug("GuardEvaluator::evaluateGuardNode - Reactive guard evaluated");
        }

        // Handle external guard classes if specified
        std::string externalClass = guard->getExternalClass();
        if (!externalClass.empty()) {
            SCXML::Common::Logger::debug("GuardEvaluator::evaluateGuardNode - External guard class: " + externalClass);
            // In a full implementation, this would load and execute external guard logic
            // For now, we rely on the condition expression
        }

        SCXML::Common::Logger::debug(std::string("GuardEvaluator::evaluateGuardNode - Guard node result: ") +
                                     (result.satisfied ? "satisfied" : "not satisfied"));

        return result;

    } catch (const std::exception &e) {
        return handleError("", e.what(), context);
    }
}

SCXML::GuardEvaluator::GuardResult SCXML::GuardEvaluator::evaluateExpression(const std::string &expression,
                                                                             const GuardContext &context) {
    GuardResult result;

    // Trim whitespace and check if expression is meaningful
    std::string trimmed = expression;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r\f\v"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r\f\v") + 1);

    result.hasGuard = !trimmed.empty();
    result.guardExpression = expression;

    if (trimmed.empty()) {
        result.satisfied = true;
        return result;
    }

    try {
        // Validate expression syntax
        if (!isValidGuardExpression(expression)) {
            return handleError(expression, "Invalid guard expression syntax", context);
        }

        SCXML::Common::Logger::debug("GuardEvaluator::evaluateExpression - Evaluating: " + expression);

        // Simple expression evaluation for common cases
        if (expression == "true") {
            result.satisfied = true;
            return result;
        }

        if (expression == "false") {
            result.satisfied = false;
            return result;
        }

        // Use complex expression evaluation for logical operators and advanced patterns
        if (expression.find("&&") != std::string::npos || expression.find("||") != std::string::npos ||
            (expression.find("!") == 0 && expression.length() > 1) || expression.find("In(") != std::string::npos ||
            expression.find("===") != std::string::npos || expression.find("==") != std::string::npos ||
            expression.find("!=") != std::string::npos || expression.find("<=") != std::string::npos ||
            expression.find(">=") != std::string::npos || expression.find("<") != std::string::npos ||
            expression.find(">") != std::string::npos) {
            return evaluateComplexExpression(expression, context);
        }

        // Data model variable checks
        if (context.runtimeContext) {
            // Check for simple variable existence: "variableName"
            if (context.runtimeContext->hasDataValue(expression)) {
                std::string value = context.runtimeContext->getDataValue(expression);
                result.satisfied = !value.empty() && value != "false" && value != "0";
                SCXML::Common::Logger::debug("GuardEvaluator::evaluateExpression - Variable " + expression + " = " +
                                             value);
                return result;
            }

            // Check for equality expressions: "variable == 'value'" or "variable == number"
            std::regex eqPattern(R"((\w+)\s*==\s*(?:['\"]([^'\"]*)['\"]|(\d+(?:\.\d+)?)))");
            std::smatch matches;
            if (std::regex_match(expression, matches, eqPattern)) {
                std::string varName = matches[1];
                std::string expectedValue = matches[2].str().empty() ? matches[3].str() : matches[2].str();

                if (context.runtimeContext->hasDataValue(varName)) {
                    std::string actualValue = context.runtimeContext->getDataValue(varName);

                    // Handle numeric comparison (convert both to double for comparison)
                    if (matches[3].matched) {  // numeric expectedValue
                        try {
                            double actualNum = std::stod(actualValue);
                            double expectedNum = std::stod(expectedValue);
                            result.satisfied = (actualNum == expectedNum);
                        } catch (const std::exception &) {
                            result.satisfied = (actualValue == expectedValue);  // fallback to string comparison
                        }
                    } else {
                        // Handle string comparison with proper type coercion and null/empty handling
                        if (expectedValue.empty() && (actualValue == "null" || actualValue.empty())) {
                            result.satisfied = true;  // empty string matches null/empty
                        } else {
                            // Try numeric coercion for mixed type comparison (e.g., 6.0 == "6")
                            try {
                                double actualNum = std::stod(actualValue);
                                double expectedNum = std::stod(expectedValue);
                                result.satisfied = (actualNum == expectedNum);
                            } catch (const std::exception &) {
                                result.satisfied = (actualValue == expectedValue);  // fallback to string comparison
                            }
                        }
                    }

                    SCXML::Common::Logger::debug("GuardEvaluator::evaluateExpression - " + varName + " (" +
                                                 actualValue + ") == '" + expectedValue + "' -> " +
                                                 (result.satisfied ? "true" : "false"));
                    return result;
                }
            }

            // Check for reverse equality expressions: "'value' == variable" or "number == variable"
            std::regex reverseEqPattern(R"((?:['\"]([^'\"]*)['\"]|(\d+(?:\.\d+)?))\s*==\s*(\w+))");
            if (std::regex_match(expression, matches, reverseEqPattern)) {
                std::string varName = matches[3];
                std::string expectedValue = matches[1].str().empty() ? matches[2].str() : matches[1].str();

                if (context.runtimeContext->hasDataValue(varName)) {
                    std::string actualValue = context.runtimeContext->getDataValue(varName);

                    // Handle numeric comparison (convert both to double for comparison)
                    if (matches[2].matched) {  // numeric expectedValue
                        try {
                            double actualNum = std::stod(actualValue);
                            double expectedNum = std::stod(expectedValue);
                            result.satisfied = (actualNum == expectedNum);
                        } catch (const std::exception &) {
                            result.satisfied = (actualValue == expectedValue);  // fallback to string comparison
                        }
                    } else {
                        // Handle string comparison with proper type coercion and null/empty handling
                        if (expectedValue.empty() && (actualValue == "null" || actualValue.empty())) {
                            result.satisfied = true;  // empty string matches null/empty
                        } else {
                            // Try numeric coercion for mixed type comparison (e.g., "6" == 6.0)
                            try {
                                double actualNum = std::stod(actualValue);
                                double expectedNum = std::stod(expectedValue);
                                result.satisfied = (actualNum == expectedNum);
                            } catch (const std::exception &) {
                                result.satisfied = (actualValue == expectedValue);  // fallback to string comparison
                            }
                        }
                    }

                    SCXML::Common::Logger::debug("GuardEvaluator::evaluateExpression - '" + expectedValue +
                                                 "' == " + varName + " (" + actualValue + ") -> " +
                                                 (result.satisfied ? "true" : "false"));
                    return result;
                }
            }

            // Check for inequality expressions: "variable != 'value'"
            std::regex nePattern(R"((\w+)\s*!=\s*['\"]([^'\"]*)['\"])");
            if (std::regex_match(expression, matches, nePattern)) {
                std::string varName = matches[1];
                std::string expectedValue = matches[2];

                if (context.runtimeContext->hasDataValue(varName)) {
                    std::string actualValue = context.runtimeContext->getDataValue(varName);
                    result.satisfied = (actualValue != expectedValue);
                    SCXML::Common::Logger::debug("GuardEvaluator::evaluateExpression - " + varName + " (" +
                                                 actualValue + ") != '" + expectedValue + "' -> " +
                                                 (result.satisfied ? "true" : "false"));
                    return result;
                }
            }

            // Check for event name matching: "_event.name == 'eventName'"
            std::regex eventPattern(R"(_event\.name\s*==\s*['\"]([^'\"]*)['\"])");
            if (std::regex_match(expression, matches, eventPattern)) {
                std::string expectedEventName = matches[1];
                if (context.currentEvent) {
                    // std::string actualEventName = context.currentEvent->getName(); // Incomplete type issue
                    std::string actualEventName = "";
                    result.satisfied = (actualEventName == expectedEventName);
                    SCXML::Common::Logger::debug("GuardEvaluator::evaluateExpression - _event.name (" +
                                                 actualEventName + ") == '" + expectedEventName + "' -> " +
                                                 (result.satisfied ? "true" : "false"));
                    return result;
                }
            }
        }

        // ARCHITECTURAL FIX: Use DataModelEngine for ECMAScript evaluation when available
        if (dataModelEngine_) {
            SCXML::Common::Logger::debug("GuardEvaluator::evaluateExpression - Using DataModelEngine for: " +
                                         expression);

            auto evalResult = dataModelEngine_->evaluateExpression(expression, *context.runtimeContext);
            SCXML::Common::Logger::debug(
                "GuardEvaluator::evaluateExpression - DataModelEngine called for: " + expression +
                ", success: " + std::string(evalResult.success ? "true" : "false") +
                (evalResult.success ? ", value: " + dataModelEngine_->valueToString(evalResult.value)
                                    : ", error: " + evalResult.errorMessage));
            if (evalResult.success) {
                // Convert DataValue to boolean using JavaScript semantics
                bool boolResult = false;
                std::visit(
                    [&boolResult](const auto &value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, bool>) {
                            boolResult = value;
                        } else if constexpr (std::is_same_v<T, int64_t>) {
                            boolResult = (value != 0);
                        } else if constexpr (std::is_same_v<T, double>) {
                            boolResult = (value != 0.0);
                        } else if constexpr (std::is_same_v<T, std::string>) {
                            boolResult = !value.empty();
                        } else {
                            boolResult = false;  // null/undefined -> false
                        }
                    },
                    evalResult.value);

                result.satisfied = boolResult;
                SCXML::Common::Logger::debug(
                    std::string("GuardEvaluator::evaluateExpression - DataModelEngine result: ") +
                    (boolResult ? "true" : "false"));
                return result;
            } else {
                SCXML::Common::Logger::debug(
                    "GuardEvaluator::evaluateExpression - DataModelEngine evaluation failed: " +
                    evalResult.errorMessage);
                // Fall through to expression evaluator as fallback
            }
        }

        // Use expression evaluator if available
        if (expressionEvaluator_) {
            auto evalContext = createEvaluationContext(context);
            SCXML::Runtime::ExpressionEvaluator::DataModel dataModel;  // Empty data model for now
            auto evalResult = expressionEvaluator_->evaluateBoolean(expression, dataModel, &evalContext);

            if (evalResult.success) {
                // Extract bool value from variant
                if (std::holds_alternative<bool>(evalResult.value)) {
                    result.satisfied = std::get<bool>(evalResult.value);
                } else {
                    result.satisfied = false;  // Default for non-bool results
                }
                SCXML::Common::Logger::debug(
                    std::string("GuardEvaluator::evaluateExpression - Expression evaluator result: ") +
                    (result.satisfied ? "true" : "false"));
                return result;
            } else {
                return handleError(expression, "Expression evaluation failed: " + evalResult.error, context);
            }
        }

        // Default: assume condition is satisfied if we can't evaluate
        SCXML::Common::Logger::warning("GuardEvaluator::evaluateExpression - Cannot evaluate expression '" +
                                       expression + "', assuming true");
        result.satisfied = true;

        return result;

    } catch (const std::exception &e) {
        return handleError(expression, e.what(), context);
    }
}

bool SCXML::GuardEvaluator::hasGuardCondition(std::shared_ptr<SCXML::Model::ITransitionNode> transition) const {
    if (!transition) {
        return false;
    }

    // Check if transition has guard condition
    std::string guardCondition = transition->getGuard();
    if (!guardCondition.empty()) {
        return true;
    }

    // Check if transition has guard expression
    std::string guardExpr = getGuardExpression(transition);
    return !guardExpr.empty();
}

std::string SCXML::GuardEvaluator::getGuardExpression(std::shared_ptr<SCXML::Model::ITransitionNode> transition) const {
    if (!transition) {
        return "";
    }

    // Try to get guard expression from transition
    // return transition->getGuardExpression(); // Method doesn't exist
    // Fallback to getGuard() method
    return transition->getGuard();
}

void SCXML::GuardEvaluator::setExpressionEvaluator(std::shared_ptr<SCXML::Runtime::ExpressionEvaluator> evaluator) {
    SCXML::Common::Logger::info(
        "GuardEvaluator::setExpressionEvaluator - Replacing evaluator with ECMAScript-enabled version");
    expressionEvaluator_ = evaluator;
    SCXML::Common::Logger::info("GuardEvaluator::setExpressionEvaluator - Successfully set new evaluator");
}

void SCXML::GuardEvaluator::setDataModelEngine(DataModelEngine *dataEngine) {
    SCXML::Common::Logger::info(
        "GuardEvaluator::setDataModelEngine - Configuring with ECMAScript DataModelEngine (architectural fix)");
    dataModelEngine_ = dataEngine;
    SCXML::Common::Logger::info("GuardEvaluator::setDataModelEngine - Successfully configured DataModelEngine");
}

// ========== Protected Methods ==========

SCXML::Runtime::ExpressionEvaluator::EventContext
SCXML::GuardEvaluator::createEvaluationContext(const GuardContext &guardCtx) {
    SCXML::Runtime::ExpressionEvaluator::EventContext evalCtx;

    if (guardCtx.runtimeContext) {
        // Pass runtime context pointer for JavaScript evaluator
        evalCtx.eventFields["__runtime_context__"] =
            std::to_string(reinterpret_cast<uintptr_t>(guardCtx.runtimeContext));

        // Add all data model variables
        auto dataNames = guardCtx.runtimeContext->getDataNames();
        for (const auto &name : dataNames) {
            std::string value = guardCtx.runtimeContext->getDataValue(name);
            evalCtx.eventFields[name] = value;
        }

        // Add current state information
        evalCtx.eventFields["_currentState"] = guardCtx.runtimeContext->getCurrentState();
    }

    // Add event information
    if (guardCtx.currentEvent) {
        // evalCtx.eventFields["_event.name"] = guardCtx.currentEvent->getName(); // Incomplete type issue
        // evalCtx.eventFields["_event.type"] = std::to_string(static_cast<int>(guardCtx.currentEvent->getType())); //
        // Incomplete type issue

        evalCtx.eventFields["_event.name"] = "";   // Placeholder
        evalCtx.eventFields["_event.type"] = "0";  // Placeholder

        // Add event data if it's a string
        // auto eventData = guardCtx.currentEvent->getData(); // Incomplete type issue

        // if (std::holds_alternative<std::string>(eventData)) {
        //     evalCtx.eventFields["_event.data"] = std::get<std::string>(eventData);
        // }
        evalCtx.eventFields["_event.data"] = "";  // Placeholder
    }

    // Add transition context
    evalCtx.eventFields["_source"] = guardCtx.sourceState;
    evalCtx.eventFields["_target"] = guardCtx.targetState;

    return evalCtx;
}

bool SCXML::GuardEvaluator::isValidGuardExpression(const std::string &expression) const {
    if (expression.empty()) {
        return true;  // Empty expression is valid (always true)
    }

    // Basic syntax validation
    // Check for balanced parentheses
    int parenCount = 0;
    for (char c : expression) {
        if (c == '(') {
            parenCount++;
        } else if (c == ')') {
            parenCount--;
        }
        if (parenCount < 0) {
            return false;  // More closing than opening
        }
    }

    if (parenCount != 0) {
        return false;  // Unbalanced parentheses
    }

    // Check for basic forbidden patterns
    if (expression.find("//") != std::string::npos) {
        return false;  // No double slashes
    }
    if (expression.find("/*") != std::string::npos) {
        return false;  // No comments
    }

    return true;
}

std::vector<std::string>
SCXML::GuardEvaluator::getAllGuardExpressions(std::shared_ptr<SCXML::Model::ITransitionNode> transition) const {
    std::vector<std::string> expressions;

    if (!transition) {
        return expressions;
    }

    std::string guardExpr = getGuardExpression(transition);
    if (!guardExpr.empty()) {
        expressions.push_back(guardExpr);
    }

    return expressions;
}

SCXML::GuardEvaluator::GuardResult
SCXML::GuardEvaluator::evaluateMultipleExpressions(const std::vector<std::string> &expressions,
                                                   const GuardContext &context) {
    GuardResult result;
    result.hasGuard = !expressions.empty();

    if (expressions.empty()) {
        result.satisfied = true;
        return result;
    }

    // Evaluate all expressions with AND logic
    for (const auto &expression : expressions) {
        auto exprResult = evaluateExpression(expression, context);

        if (!exprResult.satisfied) {
            // If any expression fails, entire guard fails
            result.satisfied = false;
            result.guardExpression = expression;
            result.errorMessage = exprResult.errorMessage;
            return result;
        }
    }

    // All expressions succeeded
    result.satisfied = true;
    result.guardExpression = expressions[0];  // Use first expression for logging

    return result;
}

SCXML::GuardEvaluator::GuardResult SCXML::GuardEvaluator::handleError(const std::string &expression,
                                                                      const std::string &error,
                                                                      const GuardContext &context) {
    (void)context;  // Suppress unused parameter warning

    GuardResult result;
    result.satisfied = false;
    result.hasGuard = !expression.empty();
    result.guardExpression = expression;
    result.errorMessage = error;

    setError(error);
    SCXML::Common::Logger::error("GuardEvaluator::handleError - " + error + " (expression: '" + expression + "')");

    return result;
}

// ========== Private Methods ==========

void SCXML::GuardEvaluator::initializeDefaultEvaluator() {
    // Initialize with basic ExpressionEvaluator for now
    expressionEvaluator_ = std::make_shared<SCXML::Runtime::ExpressionEvaluator>();
    SCXML::Common::Logger::info(
        "GuardEvaluator::initializeDefaultEvaluator - Initialized with basic ExpressionEvaluator");
}

void SCXML::GuardEvaluator::setError(const std::string &message) {
    lastError_ = message;
}

// ========== Advanced Guard Evaluation Methods ==========

SCXML::GuardEvaluator::GuardResult SCXML::GuardEvaluator::evaluateComplexExpression(const std::string &expression,
                                                                                    const GuardContext &context) {
    GuardResult result;
    result.hasGuard = true;
    result.guardExpression = expression;

    try {
        std::string trimmed = expression;
        // Remove leading/trailing whitespace
        while (!trimmed.empty() && std::isspace(trimmed.front())) {
            trimmed.erase(0, 1);
        }
        while (!trimmed.empty() && std::isspace(trimmed.back())) {
            trimmed.pop_back();
        }

        SCXML::Common::Logger::debug("GuardEvaluator::evaluateComplexExpression - Evaluating: '" + trimmed + "'");

        // Handle parentheses - find outermost parentheses and evaluate inner content
        if (trimmed.front() == '(' && trimmed.back() == ')') {
            std::string inner = trimmed.substr(1, trimmed.length() - 2);
            return evaluateComplexExpression(inner, context);
        }

        // Handle logical OR (||) - lowest precedence
        size_t orPos = std::string::npos;
        int parenLevel = 0;
        for (size_t i = 0; i < trimmed.length() - 1; ++i) {
            if (trimmed[i] == '(') {
                parenLevel++;
            } else if (trimmed[i] == ')') {
                parenLevel--;
            } else if (parenLevel == 0 && trimmed.substr(i, 2) == "||") {
                orPos = i;
                break;
            }
        }

        if (orPos != std::string::npos) {
            std::string left = trimmed.substr(0, orPos);
            std::string right = trimmed.substr(orPos + 2);

            auto leftResult = evaluateComplexExpression(left, context);
            if (leftResult.satisfied) {
                // Short-circuit evaluation - if left is true, result is true
                result.satisfied = true;
                SCXML::Common::Logger::debug("GuardEvaluator::evaluateComplexExpression - OR short-circuit: true");
                return result;
            }

            auto rightResult = evaluateComplexExpression(right, context);
            result.satisfied = rightResult.satisfied;
            result.errorMessage = leftResult.errorMessage.empty() ? rightResult.errorMessage : leftResult.errorMessage;

            SCXML::Common::Logger::debug(std::string("GuardEvaluator::evaluateComplexExpression - OR result: ") +
                                         (result.satisfied ? "true" : "false"));
            return result;
        }

        // Handle logical AND (&&) - higher precedence than OR
        size_t andPos = std::string::npos;
        parenLevel = 0;
        for (size_t i = 0; i < trimmed.length() - 1; ++i) {
            if (trimmed[i] == '(') {
                parenLevel++;
            } else if (trimmed[i] == ')') {
                parenLevel--;
            } else if (parenLevel == 0 && trimmed.substr(i, 2) == "&&") {
                andPos = i;
                break;
            }
        }

        if (andPos != std::string::npos) {
            std::string left = trimmed.substr(0, andPos);
            std::string right = trimmed.substr(andPos + 2);

            auto leftResult = evaluateComplexExpression(left, context);
            if (!leftResult.satisfied) {
                // Short-circuit evaluation - if left is false, result is false
                result.satisfied = false;
                result.errorMessage = leftResult.errorMessage;
                SCXML::Common::Logger::debug("GuardEvaluator::evaluateComplexExpression - AND short-circuit: false");
                return result;
            }

            auto rightResult = evaluateComplexExpression(right, context);
            result.satisfied = rightResult.satisfied;
            result.errorMessage = rightResult.errorMessage;

            SCXML::Common::Logger::debug(std::string("GuardEvaluator::evaluateComplexExpression - AND result: ") +
                                         (result.satisfied ? "true" : "false"));
            return result;
        }

        // Handle logical NOT (!) - highest precedence
        if (trimmed.front() == '!' && trimmed.length() > 1) {
            std::string inner = trimmed.substr(1);
            auto innerResult = evaluateComplexExpression(inner, context);
            result.satisfied = !innerResult.satisfied;
            result.errorMessage = innerResult.errorMessage;

            SCXML::Common::Logger::debug(std::string("GuardEvaluator::evaluateComplexExpression - NOT result: ") +
                                         (result.satisfied ? "true" : "false"));
            return result;
        }

        // Handle strict equality comparisons (===)
        size_t strictEqPos = trimmed.find("===");
        if (strictEqPos != std::string::npos) {
            std::string leftSide = trimmed.substr(0, strictEqPos);
            std::string rightSide = trimmed.substr(strictEqPos + 3);

            // Remove whitespace
            leftSide.erase(leftSide.find_last_not_of(" 	") + 1);
            rightSide.erase(0, rightSide.find_first_not_of(" 	"));

            // Remove quotes from right side if present
            if (!rightSide.empty() && (rightSide[0] == '\'' || rightSide[0] == '"')) {
                char quote = rightSide[0];
                if (rightSide.back() == quote && rightSide.length() >= 2) {
                    rightSide = rightSide.substr(1, rightSide.length() - 2);
                }
            }

            if (context.runtimeContext && context.runtimeContext->hasDataValue(leftSide)) {
                std::string leftValue = context.runtimeContext->getDataValue(leftSide);
                result.satisfied = (leftValue == rightSide);
                SCXML::Common::Logger::debug("GuardEvaluator::evaluateComplexExpression - " + leftSide + " ('" +
                                             leftValue + "') === '" + rightSide + "' -> " +
                                             (result.satisfied ? "true" : "false"));
                return result;
            }
        }

        // Handle specific guard types
        if (trimmed.find("_event") == 0) {
            return evaluateEventGuard(trimmed, context);
        }

        if (trimmed.find("In(") == 0) {
            return evaluateStateGuard(trimmed, context);
        }

        // Check if this is a simple variable name (no operators) before checking hasDataValue
        bool hasOperators = (trimmed.find('<') != std::string::npos || trimmed.find('>') != std::string::npos ||
                             trimmed.find('=') != std::string::npos || trimmed.find('!') != std::string::npos ||
                             trimmed.find('+') != std::string::npos || trimmed.find('-') != std::string::npos ||
                             trimmed.find('*') != std::string::npos || trimmed.find('/') != std::string::npos ||
                             trimmed.find('&') != std::string::npos || trimmed.find('|') != std::string::npos);

        if (!hasOperators && context.runtimeContext && context.runtimeContext->hasDataValue(trimmed)) {
            return evaluateDataModelGuard(trimmed, context);
        }

        // Use DataModelEngine directly to avoid infinite recursion with evaluateExpression
        SCXML::Common::Logger::debug("GuardEvaluator::evaluateComplexExpression - DataModelEngine check: " +
                                     std::string(dataModelEngine_ ? "available" : "null") +
                                     ", RuntimeContext: " + std::string(context.runtimeContext ? "available" : "null"));
        if (dataModelEngine_ && context.runtimeContext) {
            SCXML::Common::Logger::debug("GuardEvaluator::evaluateComplexExpression - Using DataModelEngine for: " +
                                         trimmed);
            auto evalResult = dataModelEngine_->evaluateExpression(trimmed, *context.runtimeContext);
            if (evalResult.success) {
                // Convert to boolean
                bool boolResult = false;
                std::visit(
                    [&boolResult](const auto &value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, bool>) {
                            boolResult = value;
                        } else if constexpr (std::is_same_v<T, int64_t>) {
                            boolResult = (value != 0);
                        } else if constexpr (std::is_same_v<T, double>) {
                            boolResult = (value != 0.0);
                        } else if constexpr (std::is_same_v<T, std::string>) {
                            boolResult = !value.empty();
                        } else {
                            boolResult = false;
                        }
                    },
                    evalResult.value);

                result.satisfied = boolResult;
                SCXML::Common::Logger::debug("GuardEvaluator::evaluateComplexExpression - DataModelEngine result: " +
                                             std::string(boolResult ? "true" : "false"));
                return result;
            }
        }

        // If DataModelEngine fails or unavailable, return false
        result.satisfied = false;
        result.errorMessage = "Cannot evaluate complex expression: " + trimmed;
        return result;

    } catch (const std::exception &e) {
        return handleError(expression, e.what(), context);
    }
}

SCXML::GuardEvaluator::GuardResult SCXML::GuardEvaluator::evaluateEventGuard(const std::string &expression,
                                                                             const GuardContext &context) {
    GuardResult result;
    result.hasGuard = true;
    result.guardExpression = expression;

    try {
        SCXML::Common::Logger::debug("GuardEvaluator::evaluateEventGuard - Evaluating: '" + expression + "'");

        if (!context.currentEvent) {
            result.satisfied = false;
            result.errorMessage = "No current event for event guard evaluation";
            SCXML::Common::Logger::debug("GuardEvaluator::evaluateEventGuard - No current event");
            return result;
        }

        // std::string eventName = context.currentEvent->getName(); // Incomplete type issue
        std::string eventName = "";

        // Handle _event.name comparisons with simple string search
        if (expression.find("_event.name") != std::string::npos) {
            // Check for == pattern
            size_t eqPos = expression.find("==");
            if (eqPos != std::string::npos) {
                std::string rightSide = expression.substr(eqPos + 2);
                // Remove whitespace and quotes
                rightSide.erase(0, rightSide.find_first_not_of(" 	"));
                if (!rightSide.empty() && (rightSide[0] == '"' || rightSide[0] == '\'')) {
                    char quote = rightSide[0];
                    size_t endQuote = rightSide.find(quote, 1);
                    if (endQuote != std::string::npos) {
                        std::string expectedName = rightSide.substr(1, endQuote - 1);
                        result.satisfied = (eventName == expectedName);
                        SCXML::Common::Logger::debug("GuardEvaluator::evaluateEventGuard - Event name (" + eventName +
                                                     ") == '" + expectedName + "' -> " +
                                                     (result.satisfied ? "true" : "false"));
                        return result;
                    }
                }
            }

            // Check for != pattern
            size_t nePos = expression.find("!=");
            if (nePos != std::string::npos) {
                std::string rightSide = expression.substr(nePos + 2);
                // Remove whitespace and quotes
                rightSide.erase(0, rightSide.find_first_not_of(" 	"));
                if (!rightSide.empty() && (rightSide[0] == '"' || rightSide[0] == '\'')) {
                    char quote = rightSide[0];
                    size_t endQuote = rightSide.find(quote, 1);
                    if (endQuote != std::string::npos) {
                        std::string expectedName = rightSide.substr(1, endQuote - 1);
                        result.satisfied = (eventName != expectedName);
                        SCXML::Common::Logger::debug("GuardEvaluator::evaluateEventGuard - Event name (" + eventName +
                                                     ") != '" + expectedName + "' -> " +
                                                     (result.satisfied ? "true" : "false"));
                        return result;
                    }
                }
            }
        }

        // Handle _event.data access
        if (expression.find("_event.data") != std::string::npos) {
            // auto eventData = context.currentEvent->getData(); // Incomplete type issue

            // For now, assume empty data
            std::string dataValue = "";  // Placeholder

            // Data comparison using expression evaluator
            size_t eqPos = expression.find("==");
            if (eqPos != std::string::npos) {
                std::string rightSide = expression.substr(eqPos + 2);
                rightSide.erase(0, rightSide.find_first_not_of(" 	"));
                if (!rightSide.empty() && (rightSide[0] == '"' || rightSide[0] == '\'')) {
                    char quote = rightSide[0];
                    size_t endQuote = rightSide.find(quote, 1);
                    if (endQuote != std::string::npos) {
                        std::string expectedData = rightSide.substr(1, endQuote - 1);
                        result.satisfied = (dataValue == expectedData);
                        SCXML::Common::Logger::debug("GuardEvaluator::evaluateEventGuard - Event data (" + dataValue +
                                                     ") == '" + expectedData + "' -> " +
                                                     (result.satisfied ? "true" : "false"));
                        return result;
                    }
                }
            }
        }

        // Fall back to expression evaluator for complex event expressions
        return evaluateExpression(expression, context);

    } catch (const std::exception &e) {
        return handleError(expression, e.what(), context);
    }
}

SCXML::GuardEvaluator::GuardResult SCXML::GuardEvaluator::evaluateStateGuard(const std::string &expression,
                                                                             const GuardContext &context) {
    GuardResult result;
    result.hasGuard = true;
    result.guardExpression = expression;

    try {
        SCXML::Common::Logger::debug("GuardEvaluator::evaluateStateGuard - Evaluating: '" + expression + "'");

        if (!context.runtimeContext) {
            result.satisfied = false;
            result.errorMessage = "No runtime context for state guard evaluation";
            return result;
        }

        // Handle In(state) function calls
        if (expression.find("In(") == 0) {
            size_t openParen = expression.find('(');
            size_t closeParen = expression.find(')', openParen);

            if (openParen != std::string::npos && closeParen != std::string::npos) {
                std::string stateParam = expression.substr(openParen + 1, closeParen - openParen - 1);
                // Remove whitespace and quotes
                stateParam.erase(0, stateParam.find_first_not_of(" 	"));
                stateParam.erase(stateParam.find_last_not_of(" 	") + 1);

                if (!stateParam.empty() && (stateParam[0] == '"' || stateParam[0] == '\'')) {
                    char quote = stateParam[0];
                    if (stateParam.back() == quote) {
                        std::string stateId = stateParam.substr(1, stateParam.length() - 2);
                        result.satisfied = context.runtimeContext->isStateActive(stateId);

                        SCXML::Common::Logger::debug("GuardEvaluator::evaluateStateGuard - In('" + stateId + "') -> " +
                                                     (result.satisfied ? "true" : "false"));
                        return result;
                    }
                }
            }
        }

        // Handle multiple In() functions with logical operators
        if (expression.find("In(") != std::string::npos) {
            // Use expression evaluator for complex In() expressions
            return evaluateExpression(expression, context);
        }

        // Fall back to basic evaluation
        result.satisfied = false;
        result.errorMessage = "Unrecognized state guard pattern: " + expression;

        return result;

    } catch (const std::exception &e) {
        return handleError(expression, e.what(), context);
    }
}

SCXML::GuardEvaluator::GuardResult SCXML::GuardEvaluator::evaluateDataModelGuard(const std::string &expression,
                                                                                 const GuardContext &context) {
    GuardResult result;
    result.hasGuard = true;
    result.guardExpression = expression;

    try {
        SCXML::Common::Logger::debug("GuardEvaluator::evaluateDataModelGuard - Evaluating: '" + expression + "'");

        if (!context.runtimeContext) {
            result.satisfied = false;
            result.errorMessage = "No runtime context for data model guard evaluation";
            return result;
        }

        // Handle simple variable existence
        if (context.runtimeContext->hasDataValue(expression)) {
            std::string value = context.runtimeContext->getDataValue(expression);
            // JavaScript truthiness: empty string, "false", "0" are falsy
            result.satisfied = !value.empty() && value != "false" && value != "0" && value != "null";

            SCXML::Common::Logger::debug("GuardEvaluator::evaluateDataModelGuard - Variable '" + expression + "' = '" +
                                         value + "' -> " + (result.satisfied ? "true" : "false"));
            return result;
        }

        // Handle complex data model expressions with operators
        std::vector<std::string> operators = {"==", "!=", "<=", ">=", "<", ">"};
        for (const auto &op : operators) {
            size_t opPos = expression.find(op);
            if (opPos != std::string::npos) {
                std::string varName = expression.substr(0, opPos);
                std::string rightSide = expression.substr(opPos + op.length());

                // Remove whitespace
                varName.erase(varName.find_last_not_of(" 	") + 1);
                rightSide.erase(0, rightSide.find_first_not_of(" 	"));

                if (context.runtimeContext->hasDataValue(varName)) {
                    std::string leftValue = context.runtimeContext->getDataValue(varName);

                    // Remove quotes from right side if present
                    if (!rightSide.empty() && (rightSide[0] == '"' || rightSide[0] == '\'')) {
                        char quote = rightSide[0];
                        if (rightSide.back() == quote) {
                            rightSide = rightSide.substr(1, rightSide.length() - 2);
                        }
                    }

                    // Perform comparison
                    bool compResult = false;
                    if (op == "==") {
                        compResult = (leftValue == rightSide);
                    } else if (op == "!=") {
                        compResult = (leftValue != rightSide);
                    } else {
                        // For numeric comparisons, try to convert to numbers
                        try {
                            double leftNum = std::stod(leftValue);
                            double rightNum = std::stod(rightSide);

                            if (op == "<") {
                                compResult = (leftNum < rightNum);
                            } else if (op == ">") {
                                compResult = (leftNum > rightNum);
                            } else if (op == "<=") {
                                compResult = (leftNum <= rightNum);
                            } else if (op == ">=") {
                                compResult = (leftNum >= rightNum);
                            }
                        } catch (const std::exception &) {
                            // Fall back to string comparison
                            if (op == "<") {
                                compResult = (leftValue < rightSide);
                            } else if (op == ">") {
                                compResult = (leftValue > rightSide);
                            } else if (op == "<=") {
                                compResult = (leftValue <= rightSide);
                            } else if (op == ">=") {
                                compResult = (leftValue >= rightSide);
                            }
                        }
                    }

                    result.satisfied = compResult;
                    SCXML::Common::Logger::debug("GuardEvaluator::evaluateDataModelGuard - " + varName + " (" +
                                                 leftValue + ") " + op + " " + rightSide + " -> " +
                                                 (result.satisfied ? "true" : "false"));
                    return result;
                }
                break;  // Found an operator, stop looking
            }
        }

        // Fall back to expression evaluator
        return evaluateExpression(expression, context);

    } catch (const std::exception &e) {
        return handleError(expression, e.what(), context);
    }
}

}  // namespace SCXML