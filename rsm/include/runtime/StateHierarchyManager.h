#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace RSM {

// Forward declarations
class SCXMLModel;
class IStateNode;
class IExecutionContext;
class IInvokeNode;
class IActionNode;
class ConcurrentStateNode;
class HistoryManager;

/**
 * @brief Hierarchical state management system
 *
 * Handles hierarchical entry/exit logic for SCXML compound states.
 * Operates independently from existing StateMachine to support minimal invasive integration.
 */
class StateHierarchyManager {
public:
    /**
     * @brief Constructor
     * @param model SCXML model (for state information reference)
     */
    explicit StateHierarchyManager(std::shared_ptr<SCXMLModel> model);

    /**
     * @brief Destructor
     */
    ~StateHierarchyManager() = default;

    /**
     * @brief Hierarchical state entry
     *
     * Automatically enters the initial child state if the target state is a compound state.
     * Tracks all activated states internally.
     *
     * @param stateId State ID to enter
     * @return Success status
     */
    bool enterState(const std::string &stateId);

    /**
     * @brief Return current deepest active state
     *
     * Returns the deepest (leaf) active state in the hierarchy.
     * Used for StateMachine::getCurrentState() compatibility.
     *
     * @return Current active state ID
     */
    std::string getCurrentState() const;

    /**
     * @brief Return all active states
     *
     * Returns a list of all currently active states.
     * Sorted in hierarchical order (parent -> child).
     *
     * @return List of active state IDs
     */
    std::vector<std::string> getActiveStates() const;

    /**
     * @brief Check if a specific state is active
     *
     * @param stateId State ID to check
     * @return Whether the state is active
     */
    bool isStateActive(const std::string &stateId) const;

    /**
     * @brief Exit state
     *
     * Deactivates the specified state and its descendant states.
     *
     * @param stateId State ID to exit
     * @param executionContext Execution context for proper exit action execution
     */
    void exitState(const std::string &stateId, std::shared_ptr<IExecutionContext> executionContext = nullptr);

    /**
     * @brief Reset all states
     *
     * Clears the entire active state list.
     */
    void reset();

    /**
     * @brief Check if hierarchical mode is needed
     *
     * Checks whether current active states require hierarchical management.
     *
     * @return Whether hierarchical mode is needed
     */
    bool isHierarchicalModeNeeded() const;

    /**
     * @brief Set callback for onentry action execution
     *
     * This callback is called when states are added to the active configuration
     * to execute their onentry actions per W3C SCXML specification.
     *
     * @param callback Function to call with state ID for onentry execution
     */
    void setOnEntryCallback(std::function<void(const std::string &)> callback);

    /**
     * @brief Set callback for invoke deferring (W3C SCXML 6.4 compliance)
     *
     * This callback is called when a state with invoke elements is entered,
     * allowing the StateMachine to defer invoke execution until after state entry completes.
     * This ensures proper timing with transition actions and W3C SCXML compliance.
     *
     * @param callback Function to call with stateId and invoke nodes for deferring
     */
    void setInvokeDeferCallback(
        std::function<void(const std::string &, const std::vector<std::shared_ptr<IInvokeNode>> &)> callback);

    /**
     * @brief Set condition evaluator callback for transition guard evaluation
     *
     * This callback is used by concurrent regions to evaluate guard conditions
     * on transitions using the StateMachine's JavaScript engine.
     *
     * @param evaluator Function to call with condition string, returns evaluation result
     */
    void setConditionEvaluator(std::function<bool(const std::string &)> evaluator);

    /**
     * @brief Set execution context for concurrent region action execution
     *
     * This context is passed to parallel state regions during state entry
     * to ensure proper action execution in transitions (W3C SCXML 403c compliance).
     *
     * @param context Execution context for JavaScript evaluation and action execution
     */
    void setExecutionContext(std::shared_ptr<IExecutionContext> context);

    /**
     * @brief Set callback for initial transition action execution (W3C SCXML 3.13 compliance)
     *
     * This callback is called when a compound state with an initial transition
     * needs to execute the transition's actions AFTER parent onentry and BEFORE child entry.
     * The callback must handle immediate mode control to ensure proper event queuing.
     *
     * @param callback Function to call with action nodes from initial transition
     */
    void setInitialTransitionCallback(std::function<void(const std::vector<std::shared_ptr<IActionNode>> &)> callback);

    /**
     * @brief Set callback for entering states via StateMachine
     *
     * W3C SCXML 3.10: When entering initial child states, delegate to StateMachine::enterState
     * to ensure history states are properly restored instead of re-executing defaults
     *
     * @param callback Function to call to enter a state (returns success/failure)
     */
    void setEnterStateCallback(std::function<bool(const std::string &)> callback);

    /**
     * @brief Set history manager for direct history restoration
     *
     * W3C SCXML 3.10: Allows StateHierarchyManager to handle history restoration
     * without triggering EnterStateGuard issues from reentrant calls
     *
     * @param historyManager History manager instance
     */
    void setHistoryManager(HistoryManager *historyManager);

    /**
     * @brief Enter a state along with all its ancestors up to a parent
     *
     * W3C SCXML 3.3: When initial attribute specifies deep descendants,
     * all ancestor states must be entered from top to bottom.
     * Properly handles parallel states in the ancestor chain.
     *
     * @param targetStateId Target state to enter
     * @param stopAtParent Stop before entering this parent (exclusive)
     * @return Success status
     */
    bool enterStateWithAncestors(const std::string &targetStateId, IStateNode *stopAtParent,
                                 std::vector<std::string> *deferredOnEntryStates = nullptr);

    /**
     * @brief Remove a state from active configuration
     *
     * @param stateId State ID to remove
     */
    void removeStateFromConfiguration(const std::string &stateId);

    /**
     * @brief Add state to active configuration (without onentry callback)
     *
     * W3C SCXML: Used for deferred onentry execution
     * Only adds state to configuration without calling onentry
     *
     * @param stateId State ID to add
     */
    void addStateToConfigurationWithoutOnEntry(const std::string &stateId);

private:
    /**
     * @brief SCXML W3C: Specialized cleanup for parallel states
     *
     * Exits a parallel state and all its descendant regions simultaneously
     * @param parallelStateId The parallel state to exit
     */
    void exitParallelStateAndDescendants(const std::string &parallelStateId);

    /**
     * @brief SCXML W3C: Traditional hierarchical state cleanup
     *
     * Removes a state and all its child states from the active configuration
     * @param stateId The hierarchical state to exit
     */
    void exitHierarchicalState(const std::string &stateId);

    /**
     * @brief Recursively collects all descendant states of a given parent state
     *
     * @param parentId Parent state ID
     * @param collector Vector to collect descendant state IDs
     */
    void collectDescendantStates(const std::string &parentId, std::vector<std::string> &collector);

    /**
     * @brief W3C SCXML 3.3: Update parallel region currentState for deep initial targets
     *
     * When deep initial targets bypass default region initialization, we must synchronize
     * each region's currentState with the actual active configuration. This function
     * finds all active parallel states and updates their regions' currentState to match
     * the deepest active descendant within each region.
     */
    void updateParallelRegionCurrentStates();

    std::shared_ptr<SCXMLModel> model_;
    std::vector<std::string> activeStates_;      // Active state list (hierarchical order)
    std::unordered_set<std::string> activeSet_;  // Set for fast lookup

    // TSAN FIX: Mutex to protect activeStates_ and activeSet_ from concurrent access
    mutable std::mutex configurationMutex_;

    // W3C SCXML onentry callback
    std::function<void(const std::string &)> onEntryCallback_;

    // W3C SCXML 6.4: Invoke defer callback for proper timing
    std::function<void(const std::string &, const std::vector<std::shared_ptr<IInvokeNode>> &)> invokeDeferCallback_;
    std::function<bool(const std::string &)> conditionEvaluator_;

    // Execution context for concurrent region action execution (403c fix)
    std::shared_ptr<IExecutionContext> executionContext_;

    // W3C SCXML 3.13: Initial transition action callback for proper event queuing
    std::function<void(const std::vector<std::shared_ptr<IActionNode>> &)> initialTransitionCallback_;

    // W3C SCXML 3.10: State entry callback for history restoration
    // When entering initial child states, delegate to StateMachine::enterState
    // to ensure history states are properly restored instead of re-executing defaults
    std::function<bool(const std::string &)> enterStateCallback_;

    // W3C SCXML 3.10: History manager for direct history restoration (test 579)
    HistoryManager *historyManager_;

    /**
     * @brief Update execution context for all regions of a parallel state
     *
     * W3C SCXML 403c: DRY principle - centralized region executionContext management
     * This helper eliminates code duplication between enterState() and setExecutionContext()
     *
     * @param parallelState The parallel state whose regions need executionContext update
     */
    void updateRegionExecutionContexts(ConcurrentStateNode *parallelState);

    /**
     * @brief Add state to active configuration
     *
     * @param stateId State ID to add
     */
    void addStateToConfiguration(const std::string &stateId);

    /**
     * @brief Find initial child state of compound state
     *
     * @param stateNode Compound state node
     * @return Initial child state ID (empty string if none)
     */
    std::string findInitialChildState(IStateNode *stateNode) const;

    /**
     * @brief Check if state node is a compound state
     *
     * @param stateNode State node to check
     * @return Whether it is a compound state
     */
    bool isCompoundState(IStateNode *stateNode) const;

    /**
     * @brief Check if a state is a descendant of a given root state
     *
     * @param rootState Root state to check from
     * @param stateId State ID to find
     * @return true if stateId is rootState or a descendant of rootState
     */
    bool isStateDescendantOf(IStateNode *rootState, const std::string &stateId) const;

    /**
     * @brief Synchronize parallel region currentState when StateMachine modifies states directly
     *
     * W3C SCXML 405: When StateMachine processes eventless transitions within parallel regions,
     * the ConcurrentRegion must be notified to update its internal state tracking.
     *
     * @param stateId The state that was just entered
     */
    void synchronizeParallelRegionState(const std::string &stateId);
};

}  // namespace RSM