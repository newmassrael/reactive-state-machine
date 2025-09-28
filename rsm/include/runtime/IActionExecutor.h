#pragma once

#include <memory>
#include <string>

namespace RSM {

// Forward declarations
class LogAction;
class RaiseAction;
class IfAction;
class ScriptAction;
class AssignAction;
class SendAction;
class CancelAction;
class ForeachAction;
class IEventRaiser;

/**
 * @brief Interface for executing SCXML actions
 *
 * This interface provides the core operations needed to execute
 * SCXML executable content like <script>, <assign>, <log>, etc.
 * It abstracts the underlying JavaScript engine and state management.
 *
 * Uses the Command pattern with typed execution methods for type safety
 * and better error handling.
 */
class IActionExecutor {
public:
    virtual ~IActionExecutor() = default;

    // High-level action execution methods (Command pattern)
    /**
     * @brief Execute a script action
     * @param action ScriptAction to execute
     * @return true if execution was successful
     */
    virtual bool executeScriptAction(const ScriptAction &action) = 0;

    /**
     * @brief Execute an assign action
     * @param action AssignAction to execute
     * @return true if execution was successful
     */
    virtual bool executeAssignAction(const AssignAction &action) = 0;

    /**
     * @brief Execute a log action
     * @param action LogAction to execute
     * @return true if execution was successful
     */
    virtual bool executeLogAction(const LogAction &action) = 0;

    /**
     * @brief Execute a raise action
     * @param action RaiseAction to execute
     * @return true if execution was successful
     */
    virtual bool executeRaiseAction(const RaiseAction &action) = 0;

    /**
     * @brief Execute an if action (conditional execution)
     * @param action IfAction to execute
     * @return true if execution was successful
     */
    virtual bool executeIfAction(const IfAction &action) = 0;

    /**
     * @brief Execute a send action (external event sending)
     * @param action SendAction to execute
     * @return true if execution was successful
     */
    virtual bool executeSendAction(const SendAction &action) = 0;

    /**
     * @brief Execute a cancel action (delayed event cancellation)
     * @param action CancelAction to execute
     * @return true if execution was successful
     */
    virtual bool executeCancelAction(const CancelAction &action) = 0;

    /**
     * @brief Execute a foreach action (iteration over arrays)
     * @param action ForeachAction to execute
     * @return true if execution was successful
     */
    virtual bool executeForeachAction(const ForeachAction &action) = 0;

    // Low-level primitives (for internal use)
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
     * @brief Evaluate a boolean condition
     * @param condition Boolean expression to evaluate
     * @return true if condition evaluates to true
     */
    virtual bool evaluateCondition(const std::string &condition) = 0;

    /**
     * @brief Log a message with specified level
     * @param level Log level ("info", "warn", "error", "debug")
     * @param message Message to log
     */
    virtual void log(const std::string &level, const std::string &message) = 0;

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

    /**
     * @brief Set event raiser for dependency injection
     * @param eventRaiser Event raiser implementation
     */
    virtual void setEventRaiser(std::shared_ptr<IEventRaiser> eventRaiser) = 0;
};

}  // namespace RSM