#pragma once

#include "SCXMLTypes.h"
#include "scripting/JSResult.h"
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

/**
 * @brief Pure JavaScript execution engine interface
 *
 * SOLID Architecture: Single Responsibility for JavaScript execution only.
 * Separated from session management and other concerns for better testability.
 */
class IJSExecutionEngine {
public:
    virtual ~IJSExecutionEngine() = default;

    // === Core JavaScript Execution ===

    /**
     * @brief Execute JavaScript script in the specified session
     * @param sessionId Target session context
     * @param script JavaScript code to execute
     * @return Future with execution result
     */
    virtual std::future<JSResult> executeScript(const std::string &sessionId, const std::string &script) = 0;

    /**
     * @brief Evaluate JavaScript expression in the specified session
     * @param sessionId Target session context
     * @param expression JavaScript expression to evaluate
     * @return Future with evaluation result
     */
    virtual std::future<JSResult> evaluateExpression(const std::string &sessionId, const std::string &expression) = 0;

    /**
     * @brief Validate JavaScript expression syntax without executing
     * @param sessionId Target session context for validation context
     * @param expression JavaScript expression to validate
     * @return Future with validation result (true if syntax is valid)
     */
    virtual std::future<JSResult> validateExpression(const std::string &sessionId, const std::string &expression) = 0;

    // === Variable Management ===

    /**
     * @brief Set a variable in the specified session
     * @param sessionId Target session context
     * @param name Variable name
     * @param value Variable value
     * @return Future indicating success/failure
     */
    virtual std::future<JSResult> setVariable(const std::string &sessionId, const std::string &name,
                                              const ScriptValue &value) = 0;

    /**
     * @brief Get a variable from the specified session
     * @param sessionId Target session context
     * @param name Variable name
     * @return Future with variable value or error
     */
    virtual std::future<JSResult> getVariable(const std::string &sessionId, const std::string &name) = 0;

    // === SCXML-specific Features ===

    /**
     * @brief Set the current event for a session (_event variable)
     * @param sessionId Target session context
     * @param event Current event to set
     * @return Future indicating success/failure
     */
    virtual std::future<JSResult> setCurrentEvent(const std::string &sessionId,
                                                  const std::shared_ptr<Event> &event) = 0;

    /**
     * @brief Setup SCXML system variables for a session
     * @param sessionId Target session context
     * @param sessionName Human-readable session name
     * @param ioProcessors List of available I/O processors
     * @return Future indicating success/failure
     */
    virtual std::future<JSResult> setupSystemVariables(const std::string &sessionId, const std::string &sessionName,
                                                       const std::vector<std::string> &ioProcessors) = 0;

    // === Global Function Management ===

    /**
     * @brief Register a native function accessible from JavaScript
     * @param functionName Name of the function in JavaScript
     * @param callback Native function implementation
     * @return true if registration successful
     */
    virtual bool registerGlobalFunction(const std::string &functionName,
                                        std::function<ScriptValue(const std::vector<ScriptValue> &)> callback) = 0;

    // === Engine Information ===

    /**
     * @brief Get engine name and version information
     * @return Engine information string
     */
    virtual std::string getEngineInfo() const = 0;

    /**
     * @brief Get current memory usage in bytes
     * @return Memory usage in bytes
     */
    virtual size_t getMemoryUsage() const = 0;

    /**
     * @brief Trigger garbage collection
     */
    virtual void collectGarbage() = 0;

    // === Session Context Management ===

    /**
     * @brief Initialize JavaScript context for a session
     * @param sessionId Session identifier
     * @param parentSessionId Optional parent session for hierarchical contexts
     * @return true if context created successfully
     */
    virtual bool initializeSessionContext(const std::string &sessionId, const std::string &parentSessionId = "") = 0;

    /**
     * @brief Cleanup JavaScript context for a session
     * @param sessionId Session identifier
     * @return true if context cleaned up successfully
     */
    virtual bool cleanupSessionContext(const std::string &sessionId) = 0;

    /**
     * @brief Check if JavaScript context exists for session
     * @param sessionId Session identifier
     * @return true if context exists
     */
    virtual bool hasSessionContext(const std::string &sessionId) const = 0;

    /**
     * @brief Check if a variable was pre-initialized (set before datamodel initialization)
     * @param sessionId Session identifier
     * @param variableName Variable name to check
     * @return true if variable was pre-initialized (e.g., by invoke data)
     */
    virtual bool isVariablePreInitialized(const std::string &sessionId, const std::string &variableName) const = 0;

    // === Engine Lifecycle ===

    /**
     * @brief Initialize the JavaScript engine
     * @return true if initialization successful
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown the JavaScript engine and cleanup all contexts
     */
    virtual void shutdown() = 0;

    /**
     * @brief Check if engine is properly initialized
     * @return true if ready for operations
     */
    virtual bool isInitialized() const = 0;
};

}  // namespace RSM