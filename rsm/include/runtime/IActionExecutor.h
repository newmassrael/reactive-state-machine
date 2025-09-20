#pragma once

#include <memory>
#include <string>

namespace RSM {

/**
 * @brief Interface for executing SCXML actions
 *
 * This interface provides the core operations needed to execute
 * SCXML executable content like <script>, <assign>, <log>, etc.
 * It abstracts the underlying JavaScript engine and state management.
 */
class IActionExecutor {
public:
    virtual ~IActionExecutor() = default;

    /**
     * @brief Execute JavaScript script code
     * @param script JavaScript code to execute
     * @return true if script execution was successful
     */
    virtual bool executeScript(const std::string &script) = 0;

    /**
     * @brief Assign a value to a variable in the data model
     * @param location Variable location (e.g., "myVar", "data.field")
     * @param expr Expression to evaluate and assign
     * @return true if assignment was successful
     */
    virtual bool assignVariable(const std::string &location, const std::string &expr) = 0;

    /**
     * @brief Evaluate a JavaScript expression and return result as string
     * @param expression JavaScript expression to evaluate
     * @return Evaluation result as string, empty if failed
     */
    virtual std::string evaluateExpression(const std::string &expression) = 0;

    /**
     * @brief Log a message with specified level
     * @param level Log level ("info", "warn", "error", "debug")
     * @param message Message to log
     */
    virtual void log(const std::string &level, const std::string &message) = 0;

    /**
     * @brief Raise an internal event
     * @param eventName Name of the event to raise
     * @param eventData Optional event data as JSON string
     * @return true if event was raised successfully
     */
    virtual bool raiseEvent(const std::string &eventName, const std::string &eventData = "") = 0;

    /**
     * @brief Check if a variable exists in the data model
     * @param location Variable location to check
     * @return true if variable exists
     */
    virtual bool hasVariable(const std::string &location) = 0;

    /**
     * @brief Get current session ID
     * @return Session identifier string
     */
    virtual std::string getSessionId() const = 0;
};

}  // namespace RSM