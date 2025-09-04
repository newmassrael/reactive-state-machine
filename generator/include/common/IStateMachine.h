#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace SCXML {
namespace Common {

/**
 * @brief Unified State Machine Interface
 *
 * This interface provides a common API for both runtime and compile-time
 * generated state machines, ensuring consistent behavior and interoperability.
 */
class IStateMachine {
public:
    /**
     * @brief State machine execution state
     */
    enum class ExecutionState {
        STOPPED,   // State machine is stopped
        STARTING,  // State machine is starting
        RUNNING,   // State machine is executing
        PAUSED,    // State machine is paused
        STOPPING,  // State machine is stopping
        ERROR      // State machine encountered an error
    };

    /**
     * @brief Event priority levels
     */
    enum class EventPriority { LOW = 0, NORMAL = 1, HIGH = 2, CRITICAL = 3 };

public:
    virtual ~IStateMachine() = default;

    // ========== Lifecycle Management ==========

    /**
     * @brief Start the state machine execution
     * @return true if started successfully
     */
    virtual bool start() = 0;

    /**
     * @brief Stop the state machine execution
     * @return true if stopped successfully
     */
    virtual bool stop() = 0;

    /**
     * @brief Pause the state machine execution
     * @return true if paused successfully
     */
    virtual bool pause() = 0;

    /**
     * @brief Resume the state machine execution
     * @return true if resumed successfully
     */
    virtual bool resume() = 0;

    /**
     * @brief Reset the state machine to initial state
     * @return true if reset successfully
     */
    virtual bool reset() = 0;

    // ========== State Information ==========

    /**
     * @brief Get current execution state
     * @return Current execution state
     */
    virtual ExecutionState getExecutionState() const = 0;

    /**
     * @brief Check if state machine is running
     * @return true if running
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief Get current active states
     * @return Vector of currently active state IDs
     */
    virtual std::vector<std::string> getActiveStates() const = 0;

    /**
     * @brief Get current state (primary active state)
     * @return Current state ID
     */
    virtual std::string getCurrentState() const = 0;

    /**
     * @brief Check if specific state is active
     * @param stateId State ID to check
     * @return true if state is active
     */
    virtual bool isStateActive(const std::string &stateId) const = 0;

    // ========== Event Management ==========

    /**
     * @brief Send event to state machine
     * @param eventName Event name
     * @param priority Event priority
     * @return true if event was accepted
     */
    virtual bool sendEvent(const std::string &eventName, EventPriority priority = EventPriority::NORMAL) = 0;

    /**
     * @brief Send event with data
     * @param eventName Event name
     * @param eventData Event data (JSON string)
     * @param priority Event priority
     * @return true if event was accepted
     */
    virtual bool sendEventWithData(const std::string &eventName, const std::string &eventData,
                                   EventPriority priority = EventPriority::NORMAL) = 0;

    /**
     * @brief Get pending event count
     * @return Number of pending events
     */
    virtual size_t getPendingEventCount() const = 0;

    /**
     * @brief Clear all pending events
     */
    virtual void clearPendingEvents() = 0;

    // ========== Data Model Access ==========

    /**
     * @brief Set data model variable
     * @param name Variable name
     * @param value Variable value (JSON string)
     * @return true if set successfully
     */
    virtual bool setDataValue(const std::string &name, const std::string &value) = 0;

    /**
     * @brief Get data model variable
     * @param name Variable name
     * @return Variable value (JSON string) or empty if not found
     */
    virtual std::string getDataValue(const std::string &name) const = 0;

    /**
     * @brief Check if data variable exists
     * @param name Variable name
     * @return true if variable exists
     */
    virtual bool hasDataValue(const std::string &name) const = 0;

    // ========== Configuration ==========

    /**
     * @brief Get state machine name
     * @return State machine name
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Set state machine name
     * @param name New name
     */
    virtual void setName(const std::string &name) = 0;

    /**
     * @brief Enable or disable event tracing
     * @param enable true to enable tracing
     */
    virtual void setEventTracing(bool enable) = 0;

    /**
     * @brief Check if event tracing is enabled
     * @return true if tracing is enabled
     */
    virtual bool isEventTracingEnabled() const = 0;

    // ========== Statistics ==========

    /**
     * @brief Get execution statistics
     * @return Statistics as JSON string
     */
    virtual std::string getStatistics() const = 0;

    /**
     * @brief Reset execution statistics
     */
    virtual void resetStatistics() = 0;

    // ========== Callbacks ==========

    /**
     * @brief State change callback function
     */
    using StateChangeCallback = std::function<void(const std::string &fromState, const std::string &toState)>;

    /**
     * @brief Event callback function
     */
    using EventCallback = std::function<void(const std::string &eventName, const std::string &eventData)>;

    /**
     * @brief Error callback function
     */
    using ErrorCallback = std::function<void(const std::string &errorMessage)>;

    /**
     * @brief Set state change callback
     * @param callback Callback function
     */
    virtual void setStateChangeCallback(StateChangeCallback callback) = 0;

    /**
     * @brief Set event processing callback
     * @param callback Callback function
     */
    virtual void setEventCallback(EventCallback callback) = 0;

    /**
     * @brief Set error callback
     * @param callback Callback function
     */
    virtual void setErrorCallback(ErrorCallback callback) = 0;
};

// ========== Utility Functions ==========

/**
 * @brief Convert execution state to string
 * @param state Execution state
 * @return State name as string
 */
inline std::string executionStateToString(IStateMachine::ExecutionState state) {
    switch (state) {
    case IStateMachine::ExecutionState::STOPPED:
        return "stopped";
    case IStateMachine::ExecutionState::STARTING:
        return "starting";
    case IStateMachine::ExecutionState::RUNNING:
        return "running";
    case IStateMachine::ExecutionState::PAUSED:
        return "paused";
    case IStateMachine::ExecutionState::STOPPING:
        return "stopping";
    case IStateMachine::ExecutionState::ERROR:
        return "error";
    default:
        return "unknown";
    }
}

/**
 * @brief Convert string to execution state
 * @param stateStr State string
 * @return Execution state
 */
inline IStateMachine::ExecutionState stringToExecutionState(const std::string &stateStr) {
    if (stateStr == "stopped") {
        return IStateMachine::ExecutionState::STOPPED;
    }
    if (stateStr == "starting") {
        return IStateMachine::ExecutionState::STARTING;
    }
    if (stateStr == "running") {
        return IStateMachine::ExecutionState::RUNNING;
    }
    if (stateStr == "paused") {
        return IStateMachine::ExecutionState::PAUSED;
    }
    if (stateStr == "stopping") {
        return IStateMachine::ExecutionState::STOPPING;
    }
    if (stateStr == "error") {
        return IStateMachine::ExecutionState::ERROR;
    }
    return IStateMachine::ExecutionState::STOPPED;
}

}  // namespace Common
}  // namespace SCXML

// Compatibility support
using IStateMachine = SCXML::Common::IStateMachine;