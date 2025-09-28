#pragma once

#include "EventRaiserImpl.h"
#include "IActionExecutor.h"
#include "common/Logger.h"
#include "common/TypeRegistry.h"
#include "scripting/JSEngine.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace RSM {

// Forward declarations
class IEventDispatcher;
class IActionNode;

/**
 * @brief Concrete implementation of IActionExecutor using JSEngine
 *
 * This implementation bridges the action execution interface with
 * the existing JSEngine infrastructure, providing SCXML executable
 * content capabilities while maintaining compatibility with current
 * architecture.
 */
class ActionExecutorImpl : public IActionExecutor {
public:
    /**
     * @brief Construct executor for given session
     * @param sessionId JavaScript session identifier
     * @param eventDispatcher Event dispatcher for delayed event sending (optional)
     */
    explicit ActionExecutorImpl(const std::string &sessionId,
                                std::shared_ptr<IEventDispatcher> eventDispatcher = nullptr);

    /**
     * @brief Destructor - unregister from JSEngine EventDispatcher registry
     */
    virtual ~ActionExecutorImpl();

    // High-level action execution methods (Command pattern)
    bool executeScriptAction(const ScriptAction &action) override;
    bool executeAssignAction(const AssignAction &action) override;
    bool executeLogAction(const LogAction &action) override;
    bool executeRaiseAction(const RaiseAction &action) override;
    bool executeIfAction(const IfAction &action) override;
    bool executeSendAction(const SendAction &action) override;
    bool executeCancelAction(const CancelAction &action) override;
    bool executeForeachAction(const ForeachAction &action) override;

    // Low-level primitives
    bool executeScript(const std::string &script) override;
    bool assignVariable(const std::string &location, const std::string &expr) override;
    std::string evaluateExpression(const std::string &expression) override;
    bool evaluateCondition(const std::string &condition) override;
    void log(const std::string &level, const std::string &message) override;

    bool hasVariable(const std::string &location) override;
    std::string getSessionId() const override;

    /**
     * @brief Set event raiser for dependency injection
     * @param eventRaiser Event raiser implementation
     */
    void setEventRaiser(std::shared_ptr<IEventRaiser> eventRaiser);

    /**
     * @brief Set current event data for _event variable access
     * @param eventName Current event name
     * @param eventData Current event data as JSON string
     */
    void setCurrentEvent(const std::string &eventName, const std::string &eventData);

    /**
     * @brief Clear current event data
     */
    void clearCurrentEvent();

    /**
     * @brief Check if session is ready for execution
     * @return true if session exists and is operational
     */
    bool isSessionReady() const;

    /**
     * @brief Set event dispatcher for delayed event handling
     * @param eventDispatcher Event dispatcher instance
     */
    void setEventDispatcher(std::shared_ptr<IEventDispatcher> eventDispatcher);

    /**
     * @brief DEPRECATED: Check if a string is an expression that needs JavaScript evaluation
     * This method is kept for backward compatibility but is no longer used in the main evaluation path.
     * @param value String to check
     * @return true if value contains expressions that need evaluation
     */
    bool isExpression(const std::string &value) const;

private:
    std::string sessionId_;
    std::string currentEventName_;
    std::string currentEventData_;
    std::shared_ptr<IEventDispatcher> eventDispatcher_;
    std::shared_ptr<IEventRaiser> eventRaiser_;

    // Assignment context tracking to prevent _event updates during assign actions

    // Expression validation cache for performance
    mutable std::unordered_map<std::string, bool> expressionCache_;
    mutable std::mutex expressionCacheMutex_;

    /**
     * @brief Validate variable location syntax
     * @param location Variable location string
     * @return true if location is valid
     */
    bool isValidLocation(const std::string &location) const;

    /**
     * @brief Transform SCXML variable names to valid JavaScript identifiers
     * @param name Original SCXML variable name (may be numeric like "1", "2")
     * @return JavaScript-compatible variable name (e.g., "1" -> "var1")
     */
    std::string transformVariableName(const std::string &name) const;

    /**
     * @brief Interpret a value as a literal following SCXML specification
     * @param value String value to interpret as literal
     * @return Processed literal value (e.g., unquoted strings)
     */
    std::string interpretAsLiteral(const std::string &value) const;

    /**
     * @brief Try to evaluate an expression using JavaScript engine
     * @param expression Expression string to evaluate
     * @param result Output parameter for the evaluation result
     * @return true if JavaScript evaluation succeeded, false if fallback needed
     */
    bool tryJavaScriptEvaluation(const std::string &expression, std::string &result) const;

    /**
     * @brief DEPRECATED: Fast check for obvious literal values
     * @param value String to check
     * @return true if value is obviously a literal (no JS evaluation needed)
     */
    bool isObviousLiteral(const std::string &value) const;

    /**
     * @brief DEPRECATED: Fast check for obvious expressions
     * @param value String to check
     * @return true if value is obviously an expression (needs JS evaluation)
     */
    bool isObviousExpression(const std::string &value) const;

    /**
     * @brief DEPRECATED: Validate expression using JSEngine with caching
     * @param value String to validate
     * @return true if value is a valid JavaScript expression
     */
    bool validateWithJSEngine(const std::string &value) const;

    /**
     * @brief Handle JavaScript execution errors
     * @param operation Operation description for logging
     * @param errorMessage Error message from JSEngine
     */
    void handleJSError(const std::string &operation, const std::string &errorMessage) const;

    /**
     * @brief Ensure current event is set in JavaScript context
     * @return true if _event variable was set successfully
     */
    bool ensureCurrentEventSet();

    /**
     * @brief Generate unique sendid for SCXML send actions
     * @return Unique sendid string following SCXML specification
     */
    std::string generateUniqueSendId() const;

    /**
     * @brief Parse array expression into vector of string values
     * @param arrayExpr Array expression to parse (e.g., "[1,2,3]", "myArray", "data.items")
     * @return Vector of string values from the array
     */
    std::vector<std::string> parseArrayExpression(const std::string &arrayExpr);

    /**
     * @brief Set loop variable in JavaScript context for foreach iteration
     * @param varName Variable name to set
     * @param value Value to assign (as string)
     * @param iteration Current iteration number (for logging)
     * @return true if variable was set successfully
     */
    bool setLoopVariable(const std::string &varName, const std::string &value, size_t iteration);

    /**
     * @brief Execute all actions in a foreach iteration
     * @param actions Vector of actions to execute
     * @param iteration Current iteration number (for logging)
     * @return true if all actions executed successfully
     */
    bool executeIterationActions(const std::vector<std::shared_ptr<IActionNode>> &actions, size_t iteration);
};

}  // namespace RSM