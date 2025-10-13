#pragma once
#include "common/Logger.h"
#include "scripting/JSEngine.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace RSM::ForeachHelper {

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
 * @param jsEngine Reference to JSEngine instance
 * @param sessionId JSEngine session ID
 * @param itemVar Item variable name (e.g., "Var1")
 * @param itemValue Current iteration value
 * @param indexVar Index variable name (empty string if not used)
 * @param indexValue Current iteration index
 * @throws std::runtime_error if variable setting fails
 */
inline void setForeachIterationVariables(JSEngine &jsEngine, const std::string &sessionId, const std::string &itemVar,
                                         const std::string &itemValue, const std::string &indexVar, size_t indexValue) {
    // Set item variable
    auto setResult = jsEngine.setVariable(sessionId, itemVar, ScriptValue(itemValue)).get();
    if (!JSEngine::isSuccess(setResult)) {
        LOG_ERROR("Failed to set foreach item variable: {}", itemVar);
        throw std::runtime_error("Foreach setVariable failed");
    }

    // Set index variable (if provided)
    if (!indexVar.empty()) {
        auto indexResult =
            jsEngine.setVariable(sessionId, indexVar, ScriptValue(static_cast<int64_t>(indexValue))).get();
        if (!JSEngine::isSuccess(indexResult)) {
            LOG_ERROR("Failed to set foreach index variable: {}", indexVar);
            throw std::runtime_error("Foreach setVariable failed");
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

}  // namespace RSM::ForeachHelper
