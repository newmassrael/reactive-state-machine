#pragma once

#include "SCXMLTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <future>

namespace SCXML {

/**
 * @brief Main SCXML Engine interface
 * 
 * Thread-safe SCXML state machine engine with session-based JavaScript execution.
 * Supports multiple isolated sessions, each with its own variable space and event context.
 */
class SCXML_API SCXMLEngine {
public:
    virtual ~SCXMLEngine() = default;

    // === Engine Lifecycle ===

    /**
     * @brief Initialize the SCXML engine
     * @return true if initialization successful
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown the SCXML engine and cleanup all sessions
     */
    virtual void shutdown() = 0;

    /**
     * @brief Get engine name and version information
     */
    virtual std::string getEngineInfo() const = 0;

    // === Session Management ===

    /**
     * @brief Create a new SCXML session with isolated context
     * @param sessionId Unique identifier for the session
     * @param parentSessionId Optional parent session for hierarchical contexts
     * @return true if session created successfully
     */
    virtual bool createSession(const std::string& sessionId,
                              const std::string& parentSessionId = "") = 0;

    /**
     * @brief Destroy a SCXML session and cleanup its context
     * @param sessionId Session to destroy
     * @return true if session destroyed successfully
     */
    virtual bool destroySession(const std::string& sessionId) = 0;

    /**
     * @brief Check if a session exists
     * @param sessionId Session to check
     * @return true if session exists
     */
    virtual bool hasSession(const std::string& sessionId) const = 0;

    /**
     * @brief Get list of all active sessions
     * @return Vector of session information
     */
    virtual std::vector<SessionInfo> getActiveSessions() const = 0;

    // === JavaScript Execution ===

    /**
     * @brief Execute JavaScript script in the specified session (async)
     * @param sessionId Target session
     * @param script JavaScript code to execute
     * @return Future with execution result
     */
    virtual std::future<ExecutionResult> executeScript(const std::string& sessionId,
                                                       const std::string& script) = 0;

    /**
     * @brief Evaluate JavaScript expression in the specified session (async)
     * @param sessionId Target session
     * @param expression JavaScript expression to evaluate
     * @return Future with evaluation result
     */
    virtual std::future<ExecutionResult> evaluateExpression(const std::string& sessionId,
                                                            const std::string& expression) = 0;

    // === Variable Management ===

    /**
     * @brief Set a variable in the specified session (async)
     * @param sessionId Target session
     * @param name Variable name
     * @param value Variable value
     * @return Future indicating success/failure
     */
    virtual std::future<ExecutionResult> setVariable(const std::string& sessionId,
                                                     const std::string& name,
                                                     const ScriptValue& value) = 0;

    /**
     * @brief Get a variable from the specified session (async)
     * @param sessionId Target session
     * @param name Variable name
     * @return Future with variable value or error
     */
    virtual std::future<ExecutionResult> getVariable(const std::string& sessionId,
                                                     const std::string& name) = 0;

    // === SCXML Event System ===

    /**
     * @brief Set the current event for a session (_event variable) (async)
     * @param sessionId Target session
     * @param event Current event to set
     * @return Future indicating success/failure
     */
    virtual std::future<ExecutionResult> setCurrentEvent(const std::string& sessionId,
                                                         std::shared_ptr<Event> event) = 0;

    /**
     * @brief Setup SCXML system variables for a session (async)
     * @param sessionId Target session
     * @param sessionName Human-readable session name
     * @param ioProcessors List of available I/O processors
     * @return Future indicating success/failure
     */
    virtual std::future<ExecutionResult> setupSystemVariables(const std::string& sessionId,
                                                              const std::string& sessionName,
                                                              const std::vector<std::string>& ioProcessors) = 0;

    // === Engine Information ===

    /**
     * @brief Get current memory usage in bytes
     */
    virtual size_t getMemoryUsage() const = 0;

    /**
     * @brief Trigger JavaScript garbage collection
     */
    virtual void collectGarbage() = 0;
};

/**
 * @brief Factory function to create SCXML engine instance
 * @return Unique pointer to SCXML engine
 */
SCXML_API std::unique_ptr<SCXMLEngine> createSCXMLEngine();

/**
 * @brief Get SCXML library version
 * @return Version string in format "major.minor.patch"
 */
SCXML_API std::string getSCXMLVersion();

}  // namespace SCXML