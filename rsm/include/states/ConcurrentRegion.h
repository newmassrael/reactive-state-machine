#pragma once

#include "ConcurrentStateTypes.h"
#include "IConcurrentRegion.h"
#include "actions/IActionNode.h"
#include "events/EventDescriptor.h"
#include "model/IStateNode.h"
#include "runtime/IExecutionContext.h"
#include "states/IStateExitHandler.h"
#include <cassert>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

/**
 * @brief Concrete implementation of IConcurrentRegion for SCXML compliance
 *
 * SCXML W3C specification section 3.4 requirements:
 * - Regions operate independently within parallel states
 * - Each region maintains its own active configuration
 * - Regions must reach final states independently
 * - Event processing is independent per region
 *
 * SOLID principles:
 * - Single Responsibility: Manages one concurrent region's lifecycle
 * - Open/Closed: Extensible through composition, not modification
 * - Liskov Substitution: Full IConcurrentRegion interface compliance
 * - Interface Segregation: Implements only required concurrent region behavior
 * - Dependency Inversion: Depends on IStateNode abstraction
 */
class ConcurrentRegion : public IConcurrentRegion {
public:
    /**
     * @brief Constructor for SCXML-compliant concurrent region
     * @param id Unique identifier for this region
     * @param rootState Root state node for this region (required by SCXML)
     * @param executionContext Execution context for action execution (optional)
     */
    explicit ConcurrentRegion(const std::string &id, std::shared_ptr<IStateNode> rootState = nullptr,
                              std::shared_ptr<IExecutionContext> executionContext = nullptr);

    /**
     * @brief Destructor ensuring proper cleanup
     */
    virtual ~ConcurrentRegion();

    // IConcurrentRegion interface implementation

    /**
     * @brief Get unique region identifier
     * @return Region ID string (SCXML requirement)
     */
    const std::string &getId() const override;

    /**
     * @brief Activate region according to SCXML semantics
     * @return Operation result with SCXML compliance validation
     */
    ConcurrentOperationResult activate() override;

    /**
     * @brief Deactivate region with proper SCXML cleanup
     * @param executionContext Execution context for proper exit action execution
     * @return Operation result indicating success/failure
     */
    ConcurrentOperationResult deactivate(std::shared_ptr<IExecutionContext> executionContext = nullptr) override;

    /**
     * @brief Check if region is currently active
     * @return true if region is active (SCXML state)
     */
    bool isActive() const override;

    /**
     * @brief Check if region has reached final state
     * @return true if in final state (SCXML completion criteria)
     */
    bool isInFinalState() const override;

    /**
     * @brief Get current region status
     * @return Current status according to SCXML lifecycle
     */
    ConcurrentRegionStatus getStatus() const override;

    /**
     * @brief Get comprehensive region information
     * @return Region info structure with SCXML-compliant data
     */
    ConcurrentRegionInfo getInfo() const override;

    /**
     * @brief Process event in this region
     * @param event Event to process according to SCXML semantics
     * @return Operation result with any state transitions
     */
    ConcurrentOperationResult processEvent(const EventDescriptor &event) override;

    /**
     * @brief Get root state node for this region
     * @return Shared pointer to root state (SCXML requirement)
     */
    std::shared_ptr<IStateNode> getRootState() const override;

    /**
     * @brief Set root state node for this region
     * @param rootState Root state node (SCXML requirement - cannot be null)
     */
    void setRootState(std::shared_ptr<IStateNode> rootState) override;

    /**
     * @brief Get currently active states in this region
     * @return Vector of active state IDs (SCXML configuration)
     */
    std::vector<std::string> getActiveStates() const override;

    /**
     * @brief Reset region to initial state
     * @return Operation result indicating reset success
     */
    ConcurrentOperationResult reset() override;

    /**
     * @brief Validate region configuration against SCXML specification
     * @return Vector of validation errors (empty if valid)
     */
    std::vector<std::string> validate() const override;

    // Additional methods for advanced functionality

    /**
     * @brief Get current state of the region
     * @return Current state ID (empty if inactive)
     */
    const std::string &getCurrentState() const override;

    /**
     * @brief Directly set current state (for W3C SCXML 3.3 deep initial targets)
     *
     * Used when deep initial targets bypass normal region initialization.
     * Updates region's currentState to match the actual active configuration.
     *
     * @param stateId The state ID to set as current
     */
    void setCurrentState(const std::string &stateId) override;

    /**
     * @brief Check if region is in error state
     * @return true if region has encountered an error
     */
    bool isInErrorState() const;

    /**
     * @brief Set error state with message
     * @param errorMessage Description of the error
     */
    void setErrorState(const std::string &errorMessage);

    /**
     * @brief Clear error state and reset to inactive
     */
    void clearErrorState();

    /**
     * @brief Set ExecutionContext for action execution
     *
     * Dependency Injection - allows runtime injection of ExecutionContext
     * from StateMachine for proper JavaScript action execution
     *
     * @param executionContext Execution context from StateMachine
     */
    void setExecutionContext(std::shared_ptr<IExecutionContext> executionContext) override;

    /**
     * @brief Set callback for invoke deferring (W3C SCXML 6.4 compliance)
     *
     * This callback allows the region to delegate invoke execution timing
     * to the StateMachine via StateHierarchyManager, ensuring proper SCXML semantics.
     *
     * @param callback Function to call with stateId and invoke nodes for deferring
     */
    void setInvokeCallback(
        std::function<void(const std::string &, const std::vector<std::shared_ptr<IInvokeNode>> &)> callback) override;

    /**
     * @brief Set callback for condition evaluation (W3C SCXML transition guard compliance)
     *
     * This callback allows the region to delegate condition evaluation
     * to the StateMachine's JavaScript engine, ensuring proper SCXML semantics.
     *
     * @param evaluator Function to call with condition string, returns evaluation result
     */
    void setConditionEvaluator(std::function<bool(const std::string &)> evaluator) override;

    /**
     * @brief Set callback for done.state event generation (W3C SCXML 3.4 compliance)
     *
     * Lifecycle:
     * 1. StateMachine calls this during setupParallelStateCallbacks() initialization
     * 2. Callback stored in doneStateCallback_ member, valid throughout region lifetime
     * 3. ConcurrentRegion invokes callback in processEvent() when determineIfInFinalState() returns true
     * 4. Callback generates done.state.{regionId} event via StateMachine::generateDoneStateEvent()
     * 5. Event queued asynchronously, allowing external transitions to handle parallel completion
     *
     * Thread Safety: Callback invoked synchronously in event processing thread
     * Memory Safety: Lambda captures [this] pointer to StateMachine, valid while SM exists
     *
     * @param callback Function to call with region ID when region reaches final state
     */
    void setDoneStateCallback(std::function<void(const std::string &)> callback) override;

    /**
     * @brief Set desired initial child state from parent's initial attribute (W3C SCXML 3.3)
     *
     * When a parent compound state specifies deep initial targets (e.g., initial="s11p112 s11p122"),
     * this method sets the target state for this region, overriding the region's default initial state.
     *
     * This implements W3C SCXML Appendix D algorithm:
     * "if not statesToEnter.some(lambda s: isDescendant(s, child)):
     *     addDescendantStatesToEnter(child, ...)"
     *
     * @param childStateId The desired initial child state ID for this region
     */
    void setDesiredInitialChild(const std::string &childStateId) override;

private:
    // Core state
    std::string id_;
    ConcurrentRegionStatus status_;
    std::shared_ptr<IStateNode> rootState_;
    std::shared_ptr<IExecutionContext> executionContext_;
    std::string currentState_;
    std::string errorMessage_;

    // SCXML state tracking
    std::vector<std::string> activeStates_;
    bool isInFinalState_;

    // Depends on IStateExitHandler abstraction, not concrete implementation
    std::shared_ptr<IStateExitHandler> exitHandler_;

    // W3C SCXML 6.4: Invoke defer callback for proper timing (dependency inversion)
    std::function<void(const std::string &, const std::vector<std::shared_ptr<IInvokeNode>> &)> invokeCallback_;

    // W3C SCXML: Condition evaluation callback for transition guard evaluation (dependency inversion)
    std::function<bool(const std::string &)> conditionEvaluator_;

    // W3C SCXML 3.4: Done state callback for done.state.{id} event generation (dependency inversion)
    std::function<void(const std::string &)> doneStateCallback_;

    // W3C SCXML 3.3: Desired initial child from parent state's initial attribute
    // Used when parent compound state specifies deep initial targets (e.g., initial="s11p112 s11p122")
    // This overrides the region's own default initial state
    std::string desiredInitialChild_;

    // Private methods for internal state management

    /**
     * @brief Validate root state node against SCXML requirements
     * @return true if root state is valid
     */
    bool validateRootState() const;

    /**
     * @brief Update current state information
     */
    void updateCurrentState();

    /**
     * @brief Determine if current configuration represents a final state
     * @return true if configuration is final
     */
    bool determineIfInFinalState() const;

    /**
     * @brief Enter initial state according to SCXML semantics
     * @return Operation result for initial state entry
     */
    ConcurrentOperationResult enterInitialState();

    /**
     * @brief Exit all active states during deactivation
     * @param executionContext Execution context for proper exit action execution
     * @return Operation result for state exit
     */
    ConcurrentOperationResult exitAllStates(std::shared_ptr<IExecutionContext> executionContext);

    /**
     * @brief Recursively check if target state is a descendant of root state
     * @param root Root state to search from
     * @param targetId Target state ID to find
     * @return true if target is descendant of root (including root itself)
     */
    bool isDescendantOf(const std::shared_ptr<IStateNode> &root, const std::string &targetId) const;

    /**
     * @brief Execute an action node with consistent logging and error handling
     * @param actionNode Action node to execute
     * @param context Context description for logging
     * @return true if action executed successfully
     */
    bool executeActionNode(const std::shared_ptr<IActionNode> &actionNode, const std::string &context);

    /**
     * @brief Execute multiple action nodes with consistent error handling
     * @param actionNodes Vector of action nodes to execute
     * @param context Context description for logging (e.g., "transition event='event1'")
     * @details DRY principle: Centralizes action execution logic to avoid code duplication
     */
    void executeActionNodes(const std::vector<std::shared_ptr<IActionNode>> &actionNodes, const std::string &context);
};

}  // namespace RSM
