#pragma once
#include "common/Logger.h"
#include "scripting/JSEngine.h"
#include <stdexcept>
#include <string>

namespace RSM::GuardHelper {

/**
 * @brief Evaluates a guard expression using JSEngine
 *
 * @param jsEngine Reference to JSEngine instance
 * @param sessionId JSEngine session ID
 * @param guardExpr Guard expression to evaluate (e.g., "typeof Var4 !== 'undefined'")
 * @return bool True if guard evaluates to true, false otherwise
 * @throws std::runtime_error if evaluation fails
 */
inline bool evaluateGuard(JSEngine &jsEngine, const std::string &sessionId, const std::string &guardExpr) {
    auto guardResult = jsEngine.evaluateExpression(sessionId, guardExpr).get();

    if (!JSEngine::isSuccess(guardResult)) {
        LOG_ERROR("Guard evaluation failed: {}", guardExpr);
        throw std::runtime_error("Guard evaluation failed");
    }

    return JSEngine::resultToBool(guardResult);
}

}  // namespace RSM::GuardHelper
