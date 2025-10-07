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
#include <set>
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
class StateMachine : public std::enable_shared_from_this<StateMachine> {
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
     * @brief W3C SCXML 3.13: Transition information for microstep execution
     *
     * Holds all information needed to execute a transition as part of a microstep.
     * Multiple transitions execute atomically: exit all → execute all → enter all.
     */
    struct TransitionInfo {
        IStateNode *sourceState;                      // Source state node
        std::shared_ptr<ITransitionNode> transition;  // Transition node
        std::string targetState;                      // Target state ID
        std::vector<std::string> exitSet;             // States to exit (in order)

        TransitionInfo(IStateNode *src, std::shared_ptr<ITransitionNode> trans, const std::string &target,
                       const std::vector<std::string> &exits)
            : sourceState(src), transition(trans), targetState(target), exitSet(exits) {}
    };

    /**
     * @brief W3C SCXML 3.13: Exit set computation result
     *
     * Returns both the exit set and the LCA to avoid duplicate computation.
     */
    struct ExitSetResult {
        std::vector<std::string> states;  // States to exit (in order)
        std::string lca;                  // Least Common Compound Ancestor

        ExitSetResult() = default;

        ExitSetResult(const std::vector<std::string> &s, const std::string &l) : states(s), lca(l) {}
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
     * @param sendId Send ID from failed send element (for error events)
     * @param invokeId Invoke ID from invoked child process (test 338)
     * @return Transition result
     */
    TransitionResult processEvent(const std::string &eventName, const std::string &eventData,
                                  const std::string &originSessionId, const std::string &sendId = "",
                                  const std::string &invokeId = "", const std::string &originType = "");

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
     * @brief Get SCXML model
     * @return SCXML model pointer
     */
    std::shared_ptr<SCXMLModel> getModel() const;

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
    /**
     * @brief RAII guard for preventing invalid reentrant state entry calls
     *
     * Automatically manages isEnteringState_ flag with exception safety.
     * Throws std::runtime_error if reentrant call detected.
     */
    class EnterStateGuard {
    public:
        EnterStateGuard(bool &enteringFlag, bool &processingEventFlag)
            : enteringFlag_(enteringFlag), processingEventFlag_(processingEventFlag), shouldManage_(true),
              isInvalid_(false) {
            // Invalid reentrant call if already entering and not processing event
            if (enteringFlag_ && !processingEventFlag_) {
                // Don't manage flag, mark as invalid, but don't throw
                // This matches original behavior: return true silently
                shouldManage_ = false;
                isInvalid_ = true;
                return;
            }

            // Legitimate reentrant call during event processing - allow but don't re-set flag
            if (enteringFlag_ && processingEventFlag_) {
                shouldManage_ = false;  // Don't manage flag, it's already true
            } else {
                enteringFlag_ = true;  // First entry, set flag
            }
        }

        ~EnterStateGuard() {
            if (shouldManage_) {
                enteringFlag_ = false;
            }
        }

        bool isInvalidCall() const {
            return isInvalid_;
        }

        // Manually release the guard before destructor
        // Used before checkEventlessTransitions() to allow legitimate recursive calls
        void release() {
            if (shouldManage_) {
                enteringFlag_ = false;
                shouldManage_ = false;
            }
        }

        // Prevent copying
        EnterStateGuard(const EnterStateGuard &) = delete;
        EnterStateGuard &operator=(const EnterStateGuard &) = delete;

    private:
        bool &enteringFlag_;
        bool &processingEventFlag_;
        bool shouldManage_;
        bool isInvalid_;
    };

    // Core state - now delegated to StateHierarchyManager
    // Removed: std::string currentState_ (use hierarchyManager_->getCurrentState())
    // Removed: std::vector<std::string> activeStates_ (use hierarchyManager_->getActiveStates())
    bool isRunning_ = false;
    bool isEnteringState_ = false;                 // Guard against reentrant enterState calls
    bool isProcessingEvent_ = false;               // Track event processing context
    bool isEnteringInitialConfiguration_ = false;  // W3C SCXML 3.3: Track initial configuration entry
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

    // W3C SCXML: Thread safety for StateHierarchyManager access from JSEngine worker thread
    mutable std::mutex hierarchyManagerMutex_;  // Protects hierarchyManager_ read access

    // Statistics
    mutable Statistics stats_;

    // Helper methods

    // W3C SCXML 5.3: Data model initialization with binding mode support
    struct DataItemInfo {
        std::string stateId;  // Empty for top-level datamodel, state ID for state-level data
        std::shared_ptr<IDataModelItem> dataItem;
    };

    /**
     * @brief Collect all data items from document (top-level + all states)
     * @return Vector of all data items with their containing state IDs
     */
    std::vector<DataItemInfo> collectAllDataItems() const;

    /**
     * @brief Initialize a single data item (create variable and optionally assign value)
     * @param item Data item to initialize
     * @param assignValue Whether to assign the initial value (false for late binding variable creation)
     */
    void initializeDataItem(const std::shared_ptr<IDataModelItem> &item, bool assignValue);

    // W3C SCXML 5.3: Track which states have initialized their data (for late binding)
    // Thread-safety: Not required - enterState() follows W3C SCXML run-to-completion
    // semantics and is protected by isEnteringState_ guard. All event processing
    // happens sequentially on the same thread via processQueuedEvents().
    std::set<std::string> initializedStates_;

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

    /**
     * @brief Execute a single transition directly without re-evaluating its condition
     *
     * This method is used when a transition's condition has already been evaluated
     * to avoid side effects from re-evaluation (e.g., W3C test 444: ++var1).
     *
     * @param sourceState The state containing the transition
     * @param transition The transition to execute
     * @return true if the transition was executed successfully, false otherwise
     */
    bool executeTransitionDirect(IStateNode *sourceState, std::shared_ptr<ITransitionNode> transition);

    /**
     * @brief W3C SCXML 3.13: Execute transitions as a microstep
     *
     * Executes multiple transitions atomically with proper phasing:
     * 1. Exit all source states (executing onexit actions)
     * 2. Execute all transition actions in document order
     * 3. Enter all target states (executing onentry actions)
     *
     * @param transitions Vector of transitions to execute
     * @return true if all transitions executed successfully
     */
    bool executeTransitionMicrostep(const std::vector<TransitionInfo> &transitions);

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

    // W3C SCXML transition domain and exit set computation
    std::string findLCA(const std::string &sourceStateId, const std::string &targetStateId) const;
    ExitSetResult computeExitSet(const std::string &sourceStateId, const std::string &targetStateId) const;
    int getStateDocumentPosition(const std::string &stateId) const;
    std::vector<std::string> getProperAncestors(const std::string &stateId) const;
    bool isDescendant(const std::string &stateId, const std::string &ancestorId) const;

    // W3C SCXML onentry action execution
    void executeOnEntryActions(const std::string &stateId);

    // Deferred invoke execution for W3C SCXML compliance
    void deferInvokeExecution(const std::string &stateId, const std::vector<std::shared_ptr<IInvokeNode>> &invokes);
    void executePendingInvokes();

    // Helper method to reduce code duplication between isInFinalState() and isInitialStateFinal()
    bool isStateInFinalState(const std::string &stateId) const;

    // W3C SCXML 3.7 & 5.5: Compound state done.state event generation
    void handleCompoundStateFinalChild(const std::string &finalStateId);
    bool evaluateDoneData(const std::string &finalStateId, std::string &outEventData);

    // Helper methods for donedata evaluation
    static std::string escapeJsonString(const std::string &str);
    static std::string convertScriptValueToJson(const ScriptValue &value, bool quoteStrings);
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
