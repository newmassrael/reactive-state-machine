#pragma once
#include "common/Logger.h"
#include "scripting/JSEngine.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace RSM::ForeachHelper {

/**
 * @brief Sets a loop variable with proper W3C SCXML 4.6 compliance
 *
 * Handles variable declaration and type preservation for foreach loop variables.
 * If the variable doesn't exist, it will be declared with 'var' keyword.
 * This is the Single Source of Truth for foreach variable setting logic,
 * shared between Interpreter and AOT engines.
 *
 * @param jsEngine Reference to JSEngine instance
 * @param sessionId JSEngine session ID
 * @param varName Variable name to set
 * @param value JavaScript literal value (e.g., "1", "undefined", "null")
 * @return true if successful, false otherwise
 */
inline bool setLoopVariable(JSEngine &jsEngine, const std::string &sessionId, const std::string &varName,
                            const std::string &value) {
    try {
        // W3C SCXML 4.6: Check if variable already exists
        // W3C SCXML 5.3: Variables with undefined values should be considered as "existing"
        std::string checkExpression = "'" + varName + "' in this";
        auto checkResult = jsEngine.evaluateExpression(sessionId, checkExpression).get();

        bool variableExists = false;
        if (JSEngine::isSuccess(checkResult) && std::holds_alternative<bool>(checkResult.getInternalValue())) {
            variableExists = std::get<bool>(checkResult.getInternalValue());
        }

        std::string script;
        if (!variableExists) {
            // Declare and assign new variable - W3C compliance
            script = "var " + varName + " = " + value + ";";
            LOG_DEBUG("W3C FOREACH: Creating NEW variable '{}' = {}", varName, value);
        } else {
            // Assign value to existing variable
            script = varName + " = " + value + ";";
            LOG_DEBUG("W3C FOREACH: Updating EXISTING variable '{}' = {}", varName, value);
        }

        auto setResult = jsEngine.executeScript(sessionId, script).get();

        if (!JSEngine::isSuccess(setResult)) {
            // Fallback: Treat as string literal
            std::string stringLiteral = "\"" + value + "\"";
            std::string fallbackScript;
            if (!variableExists) {
                fallbackScript = "var " + varName + " = " + stringLiteral + ";";
            } else {
                fallbackScript = varName + " = " + stringLiteral + ";";
            }

            auto fallbackResult = jsEngine.executeScript(sessionId, fallbackScript).get();
            if (!JSEngine::isSuccess(fallbackResult)) {
                LOG_ERROR("Failed to set foreach variable {} = {}", varName, value);
                return false;
            }
        }

        LOG_DEBUG("Set foreach variable: {} = {}", varName, value);
        return true;

    } catch (const std::exception &e) {
        LOG_ERROR("Exception setting foreach variable {}: {}", varName, e.what());
        return false;
    }
}

/**
 * @brief Evaluates a foreach array expression using JSEngine
 *
 * @param jsEngine Reference to JSEngine instance
 * @param sessionId JSEngine session ID
 * @param arrayExpr Array expression to evaluate (e.g., "Var3", "[1,2,3]")
 * @return std::vector<std::string> Array values as strings
 * @throws std::runtime_error if evaluation fails
 */
inline std::vector<std::string> evaluateForeachArray(JSEngine &jsEngine, const std::string &sessionId,
                                                     const std::string &arrayExpr) {
    auto arrayResult = jsEngine.evaluateExpression(sessionId, arrayExpr).get();

    if (!JSEngine::isSuccess(arrayResult)) {
        LOG_ERROR("Failed to evaluate array expression: {}", arrayExpr);
        throw std::runtime_error("Foreach array evaluation failed");
    }

    return JSEngine::resultToStringArray(arrayResult, sessionId, arrayExpr);
}

/**
 * @brief Sets foreach iteration variables (item and optional index)
 *
 * Uses the shared setLoopVariable logic to ensure consistency between
 * Interpreter and AOT engines (ARCHITECTURE.md: Logic Commonization).
 *
 * @param jsEngine Reference to JSEngine instance
 * @param sessionId JSEngine session ID
 * @param itemVar Item variable name (e.g., "Var1")
 * @param itemValue Current iteration value (JavaScript literal)
 * @param indexVar Index variable name (empty string if not used)
 * @param indexValue Current iteration index
 * @throws std::runtime_error if variable setting fails
 */
inline void setForeachIterationVariables(JSEngine &jsEngine, const std::string &sessionId, const std::string &itemVar,
                                         const std::string &itemValue, const std::string &indexVar, size_t indexValue) {
    // Set item variable using shared logic
    if (!setLoopVariable(jsEngine, sessionId, itemVar, itemValue)) {
        LOG_ERROR("Failed to set foreach item variable: {}", itemVar);
        throw std::runtime_error("Foreach setVariable failed for item");
    }

    // Set index variable (if provided)
    if (!indexVar.empty()) {
        if (!setLoopVariable(jsEngine, sessionId, indexVar, std::to_string(indexValue))) {
            LOG_ERROR("Failed to set foreach index variable: {}", indexVar);
            throw std::runtime_error("Foreach setVariable failed for index");
        }
    }
}

/**
 * @brief Executes a foreach loop without iteration body (for variable declaration only)
 *
 * Used when foreach is used solely for declaring variables without executing actions
 * (W3C SCXML 4.6 allows empty foreach for variable declaration)
 *
 * @param jsEngine Reference to JSEngine instance
 * @param sessionId JSEngine session ID
 * @param arrayExpr Array expression to evaluate
 * @param itemVar Item variable name
 * @param indexVar Index variable name (empty if not used)
 * @throws std::runtime_error if evaluation or variable setting fails
 */
inline void executeForeachWithoutBody(JSEngine &jsEngine, const std::string &sessionId, const std::string &arrayExpr,
                                      const std::string &itemVar, const std::string &indexVar) {
    auto arrayValues = evaluateForeachArray(jsEngine, sessionId, arrayExpr);

    for (size_t i = 0; i < arrayValues.size(); ++i) {
        setForeachIterationVariables(jsEngine, sessionId, itemVar, arrayValues[i], indexVar, i);
    }
}

/**
 * @brief Executes a foreach loop with custom action body and W3C 4.6 compliant error handling
 *
 * This is the Single Source of Truth for foreach error handling logic,
 * eliminating code duplication between Interpreter and AOT engines.
 *
 * W3C SCXML 4.6 Compliance:
 * "If the evaluation of any child element of foreach causes an error,
 * the processor MUST cease execution of the foreach element and the block that contains it."
 *
 * @tparam BodyFunc Callable type (lambda, function pointer, functor) that takes (size_t iteration)
 *                  and returns bool (true = continue, false = stop loop due to error)
 * @param jsEngine Reference to JSEngine instance
 * @param sessionId JSEngine session ID
 * @param arrayExpr Array expression to evaluate (e.g., "Var3", "[1,2,3]")
 * @param itemVar Item variable name (e.g., "Var2")
 * @param indexVar Index variable name (empty string if not used)
 * @param executeBody Lambda/callable that executes iteration actions.
 *                    Return true to continue, false to stop loop (W3C 4.6 error handling)
 * @return true if all iterations succeeded, false if loop was stopped due to error
 * @throws std::runtime_error if array evaluation or variable setting fails
 *
 * @example Interpreter Engine usage:
 * @code
 * bool success = ForeachHelper::executeForeachWithActions(
 *     jsEngine, sessionId, "myArray", "item", "index",
 *     [&](size_t i) {
 *         // Execute nested actions
 *         for (const auto& action : nestedActions) {
 *             if (!action->execute(context)) {
 *                 return false;  // Stop loop on error (W3C 4.6)
 *             }
 *         }
 *         return true;
 *     }
 * );
 * @endcode
 *
 * @example AOT Engine usage (generated code):
 * @code
 * bool success = ::RSM::ForeachHelper::executeForeachWithActions(
 *     jsEngine, sessionId_.value(), "Var3", "Var2", "",
 *     [&](size_t i) {
 *         // Custom C++ generated code
 *         auto result = jsEngine.evaluateExpression(sessionId_.value(), "Var1 + 1").get();
 *         if (!::RSM::JSEngine::isSuccess(result)) {
 *             LOG_ERROR("Expression evaluation failed");
 *             return false;  // Stop loop on error (W3C 4.6)
 *         }
 *         jsEngine.setVariable(sessionId_.value(), "Var1", result.getInternalValue());
 *         return true;
 *     }
 * );
 * @endcode
 */
template <typename BodyFunc>
inline bool executeForeachWithActions(JSEngine &jsEngine, const std::string &sessionId, const std::string &arrayExpr,
                                      const std::string &itemVar, const std::string &indexVar, BodyFunc &&executeBody) {
    try {
        // Evaluate array expression
        auto arrayValues = evaluateForeachArray(jsEngine, sessionId, arrayExpr);

        // W3C SCXML 4.6: Execute foreach loop with error handling
        for (size_t i = 0; i < arrayValues.size(); ++i) {
            // Set iteration variables (item and optional index)
            setForeachIterationVariables(jsEngine, sessionId, itemVar, arrayValues[i], indexVar, i);

            // Execute body actions for this iteration
            // W3C SCXML 4.6: If body returns false (error), stop loop execution
            if (!executeBody(i)) {
                LOG_DEBUG("Foreach loop stopped at iteration {} due to error (W3C SCXML 4.6)", i);
                return false;  // Single Source of Truth for W3C 4.6 compliance
            }
        }

        return true;  // All iterations succeeded

    } catch (const std::exception &e) {
        LOG_ERROR("Exception in foreach execution: {}", e.what());
        throw;
    }
}

}  // namespace RSM::ForeachHelper
