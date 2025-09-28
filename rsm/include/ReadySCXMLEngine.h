#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

/**
 * @brief Production-ready, high-level SCXML engine interface
 *
 * This is the primary interface users should interact with.
 * All complexity (sessions, threading, initialization) is hidden internally.
 * Ready-to-use with zero configuration required.
 *
 * Example usage:
 * ```cpp
 * auto engine = ReadySCXMLEngine::fromFile("workflow.scxml");
 * engine->start();
 * engine->sendEvent("user_action");
 * if (engine->isInState("completed")) {
 *     // Handle completion
 * }
 * ```
 */
class ReadySCXMLEngine {
public:
    virtual ~ReadySCXMLEngine() = default;

    // === Factory Methods (Hide Complex Construction) ===

    /**
     * @brief Create engine from SCXML file
     * @param scxmlFile Path to SCXML file
     * @return Engine instance or nullptr on error
     */
    static std::unique_ptr<ReadySCXMLEngine> fromFile(const std::string &scxmlFile);

    /**
     * @brief Create engine from SCXML string content
     * @param scxmlContent SCXML document as string
     * @return Engine instance or nullptr on error
     */
    static std::unique_ptr<ReadySCXMLEngine> fromString(const std::string &scxmlContent);

    // === Core State Machine Operations ===

    /**
     * @brief Start the state machine
     * @return true if started successfully
     */
    virtual bool start() = 0;

    /**
     * @brief Stop the state machine
     */
    virtual void stop() = 0;

    /**
     * @brief Send an event to the state machine
     * @param eventName Name of the event
     * @param eventData Optional event data (JSON string)
     * @return true if event was processed
     */
    virtual bool sendEvent(const std::string &eventName, const std::string &eventData = "") = 0;

    // === State Query Operations ===

    /**
     * @brief Check if state machine is running
     * @return true if running
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief Get current active state
     * @return Current state ID, empty if not started
     */
    virtual std::string getCurrentState() const = 0;

    /**
     * @brief Check if a specific state is currently active
     * @param stateId State ID to check
     * @return true if state is active
     */
    virtual bool isInState(const std::string &stateId) const = 0;

    /**
     * @brief Get all currently active states (for hierarchical/parallel states)
     * @return Vector of active state IDs
     */
    virtual std::vector<std::string> getActiveStates() const = 0;

    // === Simple Variable Access ===

    /**
     * @brief Set a variable in the state machine's data model
     * @param name Variable name
     * @param value Variable value (will be converted to appropriate type)
     * @return true if variable was set successfully
     */
    virtual bool setVariable(const std::string &name, const std::string &value) = 0;

    /**
     * @brief Get a variable from the state machine's data model
     * @param name Variable name
     * @return Variable value as string, empty if not found
     */
    virtual std::string getVariable(const std::string &name) const = 0;

    // === Error Information ===

    /**
     * @brief Get last error message
     * @return Error message, empty if no error
     */
    virtual std::string getLastError() const = 0;

    // === Statistics (Optional) ===

    /**
     * @brief Get basic statistics
     */
    struct Statistics {
        int totalEvents = 0;
        int totalTransitions = 0;
        std::string currentState;
        bool isRunning = false;
    };

    virtual Statistics getStatistics() const = 0;
};

}  // namespace RSM