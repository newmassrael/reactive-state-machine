#pragma once

#include "scripting/JSEngine.h"
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace RSM {

/**
 * @brief Single Source of Truth for donedata evaluation (W3C SCXML 5.5, 5.7)
 *
 * Shared by Interpreter engine and Static Code Generator to ensure Zero Duplication.
 *
 * W3C SCXML 5.5: "In cases where the SCXML Processor generates a 'done' event upon
 * entry into the final state, it MUST evaluate the donedata elements param or content
 * children and place the resulting data in the _event.data field."
 *
 * W3C SCXML 5.7: "If the processor cannot create an ECMAScript object for some reason,
 * the processor must place the error error.execution in the internal event queue."
 */
class DoneDataHelper {
public:
    /**
     * @brief Evaluate donedata content expression to _event.data value
     *
     * W3C SCXML 5.5: <content> sets the entire _event.data value
     *
     * @param jsEngine JSEngine instance for expression evaluation
     * @param sessionId Session ID for JSEngine context
     * @param contentExpr Content expression to evaluate
     * @param outEventData Output value (may be string, number, object)
     * @param onError Callback for error.execution events (optional)
     * @return true if evaluation succeeded or no content, false on critical error
     *
     * Usage:
     * ```cpp
     * // Interpreter engine (StateMachine.cpp)
     * std::string eventData;
     * if (!DoneDataHelper::evaluateContent(jsEngine, sessionId, content, eventData,
     *         [this](const std::string& msg) { eventRaiser_->raiseEvent("error.execution", msg); })) {
     *     return false;
     * }
     *
     * // Static Code Generator (generated code)
     * std::string eventData;
     * DoneDataHelper::evaluateContent(jsEngine, sessionId, "1+1", eventData,
     *     [&engine](const std::string& msg) { engine.raise(Event::Error_execution); });
     * ```
     */
    static bool evaluateContent(JSEngine &jsEngine, const std::string &sessionId, const std::string &contentExpr,
                                std::string &outEventData, std::function<void(const std::string &)> onError = nullptr) {
        if (contentExpr.empty()) {
            outEventData = "";
            return true;
        }

        // W3C SCXML 5.5: Evaluate content as expression
        auto future = jsEngine.evaluateExpression(sessionId, contentExpr);
        auto result = future.get();

        if (JSEngine::isSuccess(result)) {
            const auto &value = result.getInternalValue();
            outEventData = convertScriptValueToJson(value, false);

            // For objects/arrays (null case), use original content as fallback
            if (outEventData == "null" && !std::holds_alternative<ScriptNull>(value)) {
                outEventData = contentExpr;
            }
            return true;
        }

        // W3C SCXML 5.10: Raise error.execution event for expression evaluation failure
        if (onError) {
            onError(result.getErrorMessage());
        }

        // W3C SCXML 5.5: Return empty data (not literal content) when evaluation fails
        outEventData = "";
        return true;
    }

    /**
     * @brief Evaluate donedata params to JSON object
     *
     * W3C SCXML 5.5: <param> elements create object with name:value pairs
     * W3C SCXML 5.7: Empty param location raises error.execution
     *
     * @param jsEngine JSEngine instance for expression evaluation
     * @param sessionId Session ID for JSEngine context
     * @param params Vector of (name, expr) pairs
     * @param outEventData Output JSON string (e.g., {"Var1":1})
     * @param onError Callback for error.execution events (optional)
     * @return false if structural error prevents done.state, true otherwise
     *
     * Error Handling:
     * - Structural error (empty location): returns false to prevent done.state event
     * - Runtime error (invalid expr): ignores failed param, continues with others
     *
     * Usage:
     * ```cpp
     * // Interpreter engine
     * std::vector<std::pair<std::string, std::string>> params = {{"x", "1"}, {"y", "Var1"}};
     * std::string eventData;
     * if (!DoneDataHelper::evaluateParams(jsEngine, sessionId, params, eventData,
     *         [this](const std::string& msg) { eventRaiser_->raiseEvent("error.execution", msg); })) {
     *     return false;  // Structural error, skip done.state
     * }
     *
     * // Static Code Generator
     * std::vector<std::pair<std::string, std::string>> params = {{"Var1", "1"}};
     * std::string eventData;
     * DoneDataHelper::evaluateParams(jsEngine, sessionId, params, eventData,
     *     [&engine](const std::string& msg) { engine.raise(Event::Error_execution); });
     * ```
     */
    static bool evaluateParams(JSEngine &jsEngine, const std::string &sessionId,
                               const std::vector<std::pair<std::string, std::string>> &params,
                               std::string &outEventData, std::function<void(const std::string &)> onError = nullptr) {
        if (params.empty()) {
            outEventData = "";
            return true;
        }

        // W3C SCXML 5.5: <param> elements create an object with name:value pairs
        std::ostringstream jsonBuilder;
        jsonBuilder << "{";

        bool first = true;
        for (const auto &param : params) {
            const std::string &paramName = param.first;
            const std::string &paramExpr = param.second;

            // W3C SCXML 5.7: Empty location is invalid (structural error)
            // Must raise error.execution and prevent done.state event generation
            if (paramExpr.empty()) {
                if (onError) {
                    onError("Empty param location or expression: " + paramName);
                }
                // W3C SCXML 5.7: Return false to skip done.state event generation
                return false;
            }

            // Evaluate param expression
            auto future = jsEngine.evaluateExpression(sessionId, paramExpr);
            auto result = future.get();

            if (JSEngine::isSuccess(result)) {
                // W3C SCXML 5.7: Successfully evaluated param
                if (!first) {
                    jsonBuilder << ",";
                }
                first = false;

                const auto &value = result.getInternalValue();
                jsonBuilder << "\"" << escapeJsonString(paramName) << "\":" << convertScriptValueToJson(value, true);
            } else {
                // W3C SCXML 5.7: Invalid location or expression (runtime error)
                // Must raise error.execution and ignore this param, but continue with others
                if (onError) {
                    onError("Invalid param location or expression: " + paramName + " = " + paramExpr);
                }
                // Continue to next param without adding this one
            }
        }

        jsonBuilder << "}";
        outEventData = jsonBuilder.str();
        return true;
    }

    /**
     * @brief Escape special characters for JSON string
     *
     * @param str Input string
     * @return Escaped JSON string (without surrounding quotes)
     */
    static std::string escapeJsonString(const std::string &str) {
        std::ostringstream escaped;
        for (char c : str) {
            switch (c) {
            case '"':
                escaped << "\\\"";
                break;
            case '\\':
                escaped << "\\\\";
                break;
            case '\n':
                escaped << "\\n";
                break;
            case '\r':
                escaped << "\\r";
                break;
            case '\t':
                escaped << "\\t";
                break;
            case '\b':
                escaped << "\\b";
                break;
            case '\f':
                escaped << "\\f";
                break;
            default:
                escaped << c;
                break;
            }
        }
        return escaped.str();
    }

    /**
     * @brief Convert ScriptValue to JSON representation
     *
     * @param value ScriptValue variant
     * @param quoteStrings If true, wrap strings in quotes
     * @return JSON string representation
     */
    static std::string convertScriptValueToJson(const ScriptValue &value, bool quoteStrings) {
        if (std::holds_alternative<std::string>(value)) {
            const std::string &str = std::get<std::string>(value);
            if (quoteStrings) {
                return "\"" + escapeJsonString(str) + "\"";
            }
            return str;
        } else if (std::holds_alternative<double>(value)) {
            return std::to_string(std::get<double>(value));
        } else if (std::holds_alternative<int64_t>(value)) {
            return std::to_string(std::get<int64_t>(value));
        } else if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? "true" : "false";
        }
        return "null";
    }
};

}  // namespace RSM
