#pragma once

#include "SCXMLTypes.h"
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

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
    virtual bool createSession(const std::string &sessionId, const std::string &parentSessionId = "") = 0;

    /**
     * @brief Destroy a SCXML session and cleanup its context
     * @param sessionId Session to destroy
     * @return true if session destroyed successfully
     */
    virtual bool destroySession(const std::string &sessionId) = 0;

    /**
     * @brief Check if a session exists
     * @param sessionId Session to check
     * @return true if session exists
     */
    virtual bool hasSession(const std::string &sessionId) const = 0;

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
    virtual std::future<ExecutionResult> executeScript(const std::string &sessionId, const std::string &script) = 0;

    /**
     * @brief Evaluate JavaScript expression in the specified session (async)
     * @param sessionId Target session
     * @param expression JavaScript expression to evaluate
     * @return Future with evaluation result
     */
    virtual std::future<ExecutionResult> evaluateExpression(const std::string &sessionId,
                                                            const std::string &expression) = 0;

    // === Variable Management ===

    /**
     * @brief Set a variable in the specified session (async)
     * @param sessionId Target session
     * @param name Variable name
     * @param value Variable value
     * @return Future indicating success/failure
     */
    virtual std::future<ExecutionResult> setVariable(const std::string &sessionId, const std::string &name,
                                                     const ScriptValue &value) = 0;

    /**
     * @brief Get a variable from the specified session (async)
     * @param sessionId Target session
     * @param name Variable name
     * @return Future with variable value or error
     */
    virtual std::future<ExecutionResult> getVariable(const std::string &sessionId, const std::string &name) = 0;

    // === SCXML Event System ===

    /**
     * @brief Set the current event for a session (_event variable) (async)
     * @param sessionId Target session
     * @param event Current event to set
     * @return Future indicating success/failure
     */
    virtual std::future<ExecutionResult> setCurrentEvent(const std::string &sessionId,
                                                         std::shared_ptr<Event> event) = 0;

    /**
     * @brief Setup SCXML system variables for a session (async)
     * @param sessionId Target session
     * @param sessionName Human-readable session name
     * @param ioProcessors List of available I/O processors
     * @return Future indicating success/failure
     */
    virtual std::future<ExecutionResult> setupSystemVariables(const std::string &sessionId,
                                                              const std::string &sessionName,
                                                              const std::vector<std::string> &ioProcessors) = 0;

    // === High-Level SCXML State Machine API (NEW) ===

    /**
     * @brief Load SCXML from string and prepare for execution (synchronous)
     * @param scxmlContent SCXML document as string
     * @param sessionId Optional session ID (auto-generated if empty)
     * @return true if loaded successfully
     */
    virtual bool loadSCXMLFromString(const std::string &scxmlContent, const std::string &sessionId = "") = 0;

    /**
     * @brief Load SCXML from file and prepare for execution (synchronous)
     * @param scxmlFile Path to SCXML file
     * @param sessionId Optional session ID (auto-generated if empty)
     * @return true if loaded successfully
     */
    virtual bool loadSCXMLFromFile(const std::string &scxmlFile, const std::string &sessionId = "") = 0;

    /**
     * @brief Start the state machine (synchronous)
     * @param sessionId Target session (uses default if empty)
     * @return true if started successfully
     */
    virtual bool startStateMachine(const std::string &sessionId = "") = 0;

    /**
     * @brief Stop the state machine (synchronous)
     * @param sessionId Target session (uses default if empty)
     */
    virtual void stopStateMachine(const std::string &sessionId = "") = 0;

    /**
     * @brief Send event to state machine (synchronous)
     * @param eventName Name of the event
     * @param sessionId Target session (uses default if empty)
     * @param eventData Optional event data (JSON string)
     * @return true if event was processed successfully
     */
    virtual bool sendEventSync(const std::string &eventName, const std::string &sessionId = "",
                               const std::string &eventData = "") = 0;

    /**
     * @brief Check if state machine is running (synchronous)
     * @param sessionId Target session (uses default if empty)
     * @return true if running
     */
    virtual bool isStateMachineRunning(const std::string &sessionId = "") const = 0;

    /**
     * @brief Get current active state (synchronous)
     * @param sessionId Target session (uses default if empty)
     * @return Current state ID, empty if not started
     */
    virtual std::string getCurrentStateSync(const std::string &sessionId = "") const = 0;

    /**
     * @brief Check if a specific state is currently active (synchronous)
     * @param stateId State ID to check
     * @param sessionId Target session (uses default if empty)
     * @return true if state is active
     */
    virtual bool isInStateSync(const std::string &stateId, const std::string &sessionId = "") const = 0;

    /**
     * @brief Get all currently active states (synchronous)
     * @param sessionId Target session (uses default if empty)
     * @return Vector of active state IDs
     */
    virtual std::vector<std::string> getActiveStatesSync(const std::string &sessionId = "") const = 0;

    /**
     * @brief Set a variable in the state machine's data model (synchronous)
     * @param name Variable name
     * @param value Variable value (will be converted to appropriate type)
     * @param sessionId Target session (uses default if empty)
     * @return true if variable was set successfully
     */
    virtual bool setVariableSync(const std::string &name, const std::string &value,
                                 const std::string &sessionId = "") = 0;

    /**
     * @brief Get a variable from the state machine's data model (synchronous)
     * @param name Variable name
     * @param sessionId Target session (uses default if empty)
     * @return Variable value as string, empty if not found
     */
    virtual std::string getVariableSync(const std::string &name, const std::string &sessionId = "") const = 0;

    /**
     * @brief Get last error message for the state machine operations
     * @param sessionId Target session (uses default if empty)
     * @return Error message, empty if no error
     */
    virtual std::string getLastStateMachineError(const std::string &sessionId = "") const = 0;

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

}  // namespace RSM