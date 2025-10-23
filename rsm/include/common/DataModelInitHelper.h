#pragma once

#include "scripting/JSEngine.h"
#include <functional>
#include <string>

namespace RSM {

/**
 * @brief Single Source of Truth for datamodel variable initialization (W3C SCXML 5.2, 5.3)
 *
 * ARCHITECTURE.MD: Zero Duplication Principle
 * Shared by Interpreter engine and AOT (Static) Code Generator to eliminate duplication
 * of datamodel initialization error handling logic.
 *
 * W3C SCXML 5.2: "When the document is loaded, the SCXML Processor must evaluate the
 * expressions in the 'expr' or 'src' attributes and assign the resulting value to the
 * data element."
 *
 * W3C SCXML 5.3: "If the value specified (by 'src', 'srcexpr' or children) is not a
 * legal data value, the SCXML Processor MUST raise error.execution and place the
 * error.execution event on the internal event queue."
 */
class DataModelInitHelper {
public:
    /**
     * @brief Initialize a datamodel variable with expression evaluation and error handling
     *
     * W3C SCXML 5.2/5.3: Evaluate expression and set variable, or raise error.execution
     *
     * @param jsEngine JSEngine instance for expression evaluation
     * @param sessionId Session ID for JSEngine context
     * @param varId Variable identifier (name)
     * @param expr Expression to evaluate for initial value
     * @param onError Callback for error.execution events (W3C SCXML 5.3)
     * @return true if initialization succeeded, false if error occurred
     *
     * Usage Example (matches DoneDataHelper pattern):
     * ```cpp
     * // Interpreter engine (StateMachine.cpp)
     * DataModelInitHelper::initializeVariable(
     *     jsEngine, sessionId, "Var1", "undefined.invalidProperty",
     *     [this](const std::string& msg) {
     *         eventRaiser_->raiseEvent("error.execution", msg);
     *     });
     *
     * // AOT engine (generated code from template)
     * DataModelInitHelper::initializeVariable(
     *     jsEngine, sessionId.value(), "Var1", "undefined.invalidProperty",
     *     [&engine](const std::string& msg) {
     *         engine.raise(typename Engine::EventWithMetadata(Event::Error_execution, msg));
     *     });
     * ```
     */
    static bool initializeVariable(JSEngine &jsEngine, const std::string &sessionId, const std::string &varId,
                                   const std::string &expr,
                                   std::function<void(const std::string &)> onError = nullptr) {
        if (expr.empty()) {
            // W3C SCXML 5.2: Empty expression → undefined value
            auto result = jsEngine.evaluateExpression(sessionId, "undefined").get();
            if (JSEngine::isSuccess(result)) {
                jsEngine.setVariable(sessionId, varId, result.getInternalValue());
                return true;
            }
            // Fallback: even undefined evaluation failed
            if (onError) {
                onError("Failed to initialize variable '" + varId + "' with undefined");
            }
            return false;
        }

        // W3C SCXML 5.2: Evaluate expression for initial value
        auto result = jsEngine.evaluateExpression(sessionId, expr).get();

        if (JSEngine::isSuccess(result)) {
            // Success: Set variable value
            jsEngine.setVariable(sessionId, varId, result.getInternalValue());
            return true;
        }

        // W3C SCXML 5.3: Raise error.execution for initialization failure
        if (onError) {
            onError("Failed to evaluate data expression for '" + varId + "'");
        }
        return false;
    }
};

}  // namespace RSM
