#pragma once

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace RSM {

/**
 * @brief Helper functions for W3C SCXML namelist evaluation
 *
 * Single Source of Truth for namelist processing shared between:
 * - Interpreter engine (ActionExecutorImpl::executeSendAction)
 * - AOT engine (StaticCodeGenerator - generated send code)
 *
 * W3C SCXML References:
 * - C.1: Namelist attribute for event data population
 * - 6.2: Send element namelist evaluation
 * - Test 354: Namelist, param, and content event data
 *
 * ARCHITECTURE.md Compliance:
 * - Zero Duplication Principle (lines 493, 560-564)
 * - Single Source of Truth for W3C SCXML C.1 semantics
 * - Helper Function Pattern (lines 605-690)
 */
class NamelistHelper {
public:
    /**
     * @brief Evaluate namelist variables and populate params map
     *
     * Single Source of Truth for namelist evaluation (W3C SCXML C.1).
     * Parses space-separated variable names, evaluates each via JSEngine,
     * and stores results in params map for event data construction.
     *
     * W3C SCXML 6.2: If evaluation of namelist variables produces an error,
     * the Processor MUST discard the message (raise error.execution).
     *
     * @tparam JSEngineType JSEngine type (RSM::JSEngine)
     * @tparam ErrorHandler Callable(const std::string& errorMsg) for error.execution
     *
     * @param jsEngine JSEngine instance for variable evaluation
     * @param sessionId Session identifier for JSEngine context
     * @param namelist Space-separated variable names (e.g., "var1 var2 var3")
     * @param params Output map for storing name-value pairs
     * @param errorHandler Callback to raise error.execution on failures
     *
     * @return true if successful, false if any variable evaluation failed
     *
     * @example Interpreter usage:
     * bool success = NamelistHelper::evaluateNamelist(
     *     jsEngine, sessionId, "var1 var2",
     *     evaluatedParams,
     *     [this, &sendId](const std::string& msg) {
     *         eventRaiser_->raiseEvent("error.execution", msg, sendId, false);
     *     }
     * );
     *
     * @example AOT usage (in send.jinja2):
     * bool success = ::RSM::NamelistHelper::evaluateNamelist(
     *     jsEngine, sessionId_.value(), "{{ action.namelist }}",
     *     params,
     *     [&engine](const std::string& msg) {
     *         LOG_ERROR("Failed to evaluate namelist: {}", msg);
     *         engine.raise(typename Engine::EventWithMetadata(Event::Error_execution));
     *     }
     * );
     */
    template <typename JSEngineType, typename ErrorHandler>
    static bool evaluateNamelist(JSEngineType &jsEngine, const std::string &sessionId, const std::string &namelist,
                                 std::map<std::string, std::vector<std::string>> &params, ErrorHandler errorHandler) {
        if (namelist.empty()) {
            return true;  // No namelist to evaluate
        }

        // W3C SCXML C.1: Parse space/tab/newline-separated variable names
        std::istringstream namelistStream(namelist);
        std::string varName;

        while (namelistStream >> varName) {
            // Evaluate variable in JSEngine context
            auto varResult = jsEngine.getVariable(sessionId, varName).get();

            if (!JSEngineType::isSuccess(varResult)) {
                // W3C SCXML 6.2: Evaluation error → raise error.execution
                std::string errorMsg = "Failed to evaluate namelist variable '" + varName + "'";
                errorHandler(errorMsg);
                return false;  // Stop processing on first error
            }

            // Store variable value in params map
            std::string varValue = JSEngineType::resultToString(varResult);
            params[varName].push_back(varValue);
        }

        return true;  // All variables evaluated successfully
    }
};

}  // namespace RSM
