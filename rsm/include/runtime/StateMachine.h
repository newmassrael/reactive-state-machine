#pragma once

#include "model/IStateNode.h"
#include "model/SCXMLModel.h"
#include "scripting/JSEngine.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

class StateNode;
class TransitionNode;

/**
 * @brief SCXML-based State Machine Implementation
 *
 * This class provides a complete implementation of SCXML state machine
 * with JavaScript integration for guards, actions, and data model.
 */
class StateMachine {
public:
    /**
     * @brief State transition result structure
     */
    struct TransitionResult {
        bool success = false;
        std::string fromState;
        std::string toState;
        std::string eventName;
        std::string errorMessage;

        TransitionResult() = default;

        TransitionResult(bool s) : success(s) {}

        TransitionResult(bool s, const std::string &from, const std::string &to, const std::string &event)
            : success(s), fromState(from), toState(to), eventName(event) {}
    };

    /**
     * @brief Constructor
     */
    StateMachine();

    /**
     * @brief Destructor
     */
    ~StateMachine();

    /**
     * @brief Load SCXML document from file
     * @param scxmlFile Path to SCXML file
     * @return true if loaded successfully
     */
    bool loadSCXML(const std::string &scxmlFile);

    /**
     * @brief Load SCXML document from string
     * @param scxmlContent SCXML content as string
     * @return true if loaded successfully
     */
    bool loadSCXMLFromString(const std::string &scxmlContent);

    /**
     * @brief Start the state machine
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the state machine
     */
    void stop();

    /**
     * @brief Process an event
     * @param eventName Name of the event to process
     * @param eventData Optional event data (JSON string)
     * @return Transition result
     */
    TransitionResult processEvent(const std::string &eventName, const std::string &eventData = "");

    /**
     * @brief Get current state ID
     * @return Current state ID, empty if not started
     */
    std::string getCurrentState() const;

    /**
     * @brief Get all currently active states (for hierarchical states)
     * @return Vector of active state IDs
     */
    std::vector<std::string> getActiveStates() const;

    /**
     * @brief Check if state machine is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Check if a state is currently active
     * @param stateId State ID to check
     * @return true if state is active
     */
    bool isStateActive(const std::string &stateId) const;

    /**
     * @brief Bind C++ object for JavaScript access
     * @param name Object name in JavaScript
     * @param object Pointer to C++ object
     */
    template <typename T> void bindObject(const std::string &name, T *object);

    /**
     * @brief Get current event data (accessible from guards/actions)
     * @return Current event data as JSON string
     */
    std::string getCurrentEventData() const;

    /**
     * @brief Get state machine statistics
     */
    struct Statistics {
        int totalTransitions = 0;
        int totalEvents = 0;
        int failedTransitions = 0;
        std::string currentState;
        bool isRunning = false;
    };

    Statistics getStatistics() const;

private:
    /**
     * @brief Internal transition data
     */
    struct Transition {
        std::string fromState;
        std::string toState;
        std::string event;
        std::string condition;  // JavaScript expression
        std::string action;     // JavaScript code to execute
        int priority = 0;       // For conflict resolution
    };

    /**
     * @brief Internal state data
     */
    struct State {
        std::string id;
        std::string onEntryAction;  // JavaScript code
        std::string onExitAction;   // JavaScript code
        bool isFinal = false;
        std::string parent;  // For hierarchical states (future)
    };

    // Core state
    std::string currentState_;
    std::vector<std::string> activeStates_;
    bool isRunning_ = false;
    std::string initialState_;

    // SCXML model
    std::shared_ptr<SCXMLModel> model_;

    // Transitions and states
    std::vector<Transition> transitions_;
    std::map<std::string, State> states_;

    // JavaScript integration
    std::string sessionId_;
    std::string currentEventData_;
    bool jsEnvironmentReady_ = false;

    // Statistics
    mutable Statistics stats_;

    // Helper methods
    std::string generateSessionId();
    bool initializeFromModel();
    void extractStatesBasic(std::shared_ptr<IStateNode> stateNode);
    std::vector<Transition> findTransitions(const std::string &fromState, const std::string &event);
    bool evaluateCondition(const std::string &condition);
    bool executeAction(const std::string &action);
    bool enterState(const std::string &stateId);
    bool exitState(const std::string &stateId);
    bool ensureJSEnvironment();
    bool setupJSEnvironment();
    void updateStatistics();
};

// Template implementation
template <typename T> void StateMachine::bindObject(const std::string &name, T *object) {
    (void)name;    // Unused parameter - C++ binding not implemented yet
    (void)object;  // Unused parameter - C++ binding not implemented yet
    // Implementation will use existing JSEngine binding
    // This is a placeholder for the template
    static_assert(std::is_class_v<T>, "Can only bind class objects");
}

}  // namespace RSM
