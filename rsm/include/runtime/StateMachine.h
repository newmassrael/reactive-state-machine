#pragma once

#include "events/IEventDispatcher.h"
#include "model/IStateNode.h"
#include "model/SCXMLModel.h"
#include "runtime/HistoryManager.h"
#include "runtime/HistoryStateAutoRegistrar.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"
#include "runtime/InvokeExecutor.h"
#include "runtime/StateHierarchyManager.h"
#include "runtime/StateMachineEventRaiser.h"
#include "scripting/JSEngine.h"
#include <functional>
#include <map>
#include <memory>
#include <mutex>
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
     * @brief Default constructor - generates random session ID
     */
    StateMachine();

    /**
     * @brief Constructor with session ID injection
     * @param sessionId Pre-existing session ID to use (for invoke scenarios)
     */
    explicit StateMachine(const std::string &sessionId);

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
     * @brief Process an event with origin tracking for W3C SCXML finalize support
     * @param eventName Name of the event to process
     * @param eventData Optional event data (JSON string)
     * @param originSessionId Session ID that originated this event (for finalize)
     * @return Transition result
     */
    TransitionResult processEvent(const std::string &eventName, const std::string &eventData,
                                  const std::string &originSessionId);

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
     * @brief Check if the state machine is currently in a final state
     * @return true if current state is a final state
     */
    bool isInFinalState() const;

    /**
     * @brief Check if the initial state of the SCXML model is a final state
     * @return true if the initial state is a final state
     */
    bool isInitialStateFinal() const;

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

    /**
     * @brief Set EventDispatcher for delayed events and external targets
     * @param eventDispatcher EventDispatcher instance for event handling
     */
    void setEventDispatcher(std::shared_ptr<IEventDispatcher> eventDispatcher);

    /**
     * @brief W3C SCXML 6.5: Set completion callback for top-level final state notification
     *
     * This callback is invoked when the StateMachine reaches a top-level final state,
     * AFTER all onexit handlers have been executed. Used by invoke mechanism to
     * generate done.invoke events per W3C SCXML specification.
     *
     * @param callback Function to call on completion (nullptr to clear)
     */
    using CompletionCallback = std::function<void()>;
    void setCompletionCallback(CompletionCallback callback);

    /**
     * @brief Set EventRaiser for event processing
     * @param eventRaiser EventRaiser instance for event handling
     */
    void setEventRaiser(std::shared_ptr<IEventRaiser> eventRaiser);

    /**
     * @brief Get EventDispatcher for access by child components
     * @return Current EventDispatcher instance
     */
    std::shared_ptr<IEventDispatcher> getEventDispatcher() const;

private:
    // Core state - now delegated to StateHierarchyManager
    // Removed: std::string currentState_ (use hierarchyManager_->getCurrentState())
    // Removed: std::vector<std::string> activeStates_ (use hierarchyManager_->getActiveStates())
    bool isRunning_ = false;
    bool isEnteringState_ = false;    // Guard against reentrant enterState calls
    bool isProcessingEvent_ = false;  // Track event processing context
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
    std::unique_ptr<HistoryManager> historyManager_;
    std::unique_ptr<HistoryStateAutoRegistrar> historyAutoRegistrar_;

    // W3C SCXML invoke execution (SOLID architecture)
    std::unique_ptr<InvokeExecutor> invokeExecutor_;

    // Event dispatching for delayed events and external targets
    std::shared_ptr<IEventDispatcher> eventDispatcher_;

    // EventRaiser for SCXML compliance mode control
    std::shared_ptr<IEventRaiser> eventRaiser_;

    // W3C SCXML 6.5: Completion callback for invoke done.invoke event
    CompletionCallback completionCallback_;

    // Deferred invoke execution for W3C SCXML compliance
    struct DeferredInvoke {
        std::string stateId;
        std::vector<std::shared_ptr<IInvokeNode>> invokes;
    };

    std::vector<DeferredInvoke> pendingInvokes_;
    std::mutex pendingInvokesMutex_;  // Thread safety for pendingInvokes_

    // Statistics
    mutable Statistics stats_;

    // Helper methods

    bool initializeFromModel();
    void initializeHistoryManager();
    void initializeHistoryAutoRegistrar();

    // Parallel state completion handling
    void handleParallelStateCompletion(const std::string &stateId);
    void setupParallelStateCallbacks();

    bool evaluateCondition(const std::string &condition);
    bool enterState(const std::string &stateId);
    bool exitState(const std::string &stateId);

    /**
     * @brief W3C SCXML compliance: Check for eventless transitions on all active states
     * @return true if an eventless transition was executed, false otherwise
     */
    bool checkEventlessTransitions();

    // New IActionNode-based action execution methods
    bool initializeActionExecutor();
    bool executeActionNodes(const std::vector<std::shared_ptr<RSM::IActionNode>> &actions,
                            bool processEventsAfter = true);
    bool executeEntryActions(const std::string &stateId);
    bool executeExitActions(const std::string &stateId);
    bool ensureJSEnvironment();
    bool setupJSEnvironment();
    void updateStatistics();

    // SCXML W3C compliant state transition processing
    TransitionResult processStateTransitions(IStateNode *stateNode, const std::string &eventName,
                                             const std::string &eventData);

    // W3C SCXML onentry action execution
    void executeOnEntryActions(const std::string &stateId);

    // Deferred invoke execution for W3C SCXML compliance
    void deferInvokeExecution(const std::string &stateId, const std::vector<std::shared_ptr<IInvokeNode>> &invokes);
    void executePendingInvokes();

    // Helper method to reduce code duplication between isInFinalState() and isInitialStateFinal()
    bool isStateInFinalState(const std::string &stateId) const;
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
