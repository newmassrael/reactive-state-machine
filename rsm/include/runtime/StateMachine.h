#pragma once

#include "model/IStateNode.h"
#include "model/SCXMLModel.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"
#include "runtime/IHistoryManager.h"
#include "runtime/IHistoryStateAutoRegistrar.h"
#include "runtime/StateHierarchyManager.h"
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
     * @brief Get session ID for SCXML data model access
     * @return Current session ID
     */
    const std::string &getSessionId() const;

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

    /**
     * @brief Register a history state for tracking
     * @param historyStateId ID of the history state
     * @param parentStateId ID of the parent compound state
     * @param type History type (SHALLOW or DEEP)
     * @param defaultStateId Default state if no history available
     * @return true if registration succeeded
     */
    bool registerHistoryState(const std::string &historyStateId, const std::string &parentStateId, HistoryType type,
                              const std::string &defaultStateId = "");

    /**
     * @brief Check if a state ID represents a history state
     * @param stateId State ID to check
     * @return true if it's a history state
     */
    bool isHistoryState(const std::string &stateId) const;

    /**
     * @brief Clear all recorded history (for testing/reset purposes)
     */
    void clearAllHistory();

    /**
     * @brief Get history information for debugging
     * @return Vector of all recorded history entries
     */
    std::vector<HistoryEntry> getHistoryEntries() const;

private:
    // Core state - now delegated to StateHierarchyManager
    // Removed: std::string currentState_ (use hierarchyManager_->getCurrentState())
    // Removed: std::vector<std::string> activeStates_ (use hierarchyManager_->getActiveStates())
    bool isRunning_ = false;
    std::string initialState_;

    // SCXML model
    std::shared_ptr<SCXMLModel> model_;

    // JavaScript integration
    std::string sessionId_;
    std::string currentEventData_;
    bool jsEnvironmentReady_ = false;

    // Action execution infrastructure
    std::shared_ptr<IActionExecutor> actionExecutor_;
    std::shared_ptr<IExecutionContext> executionContext_;

    // Hierarchical state management
    std::unique_ptr<StateHierarchyManager> hierarchyManager_;

    // History state management (SOLID architecture)
    std::unique_ptr<IHistoryManager> historyManager_;
    std::unique_ptr<IHistoryStateAutoRegistrar> historyAutoRegistrar_;

    // Statistics
    mutable Statistics stats_;

    // Helper methods
    std::string generateSessionId();
    bool initializeFromModel();
    void initializeHistoryManager();
    void initializeHistoryAutoRegistrar();

    // Parallel state completion handling
    void handleParallelStateCompletion(const std::string &stateId);
    void setupParallelStateCallbacks();

    bool evaluateCondition(const std::string &condition);
    bool enterState(const std::string &stateId);
    bool exitState(const std::string &stateId);

    // New IActionNode-based action execution methods
    bool initializeActionExecutor();
    bool executeActionNodes(const std::vector<std::shared_ptr<RSM::IActionNode>> &actions);
    bool executeEntryActions(const std::string &stateId);
    bool executeExitActions(const std::string &stateId);
    bool ensureJSEnvironment();
    bool setupJSEnvironment();
    void updateStatistics();

    // SCXML W3C compliant state transition processing
    TransitionResult processStateTransitions(IStateNode *stateNode, const std::string &eventName,
                                             const std::string &eventData);
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
