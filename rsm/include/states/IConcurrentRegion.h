#pragma once

#include "ConcurrentStateTypes.h"
#include <memory>
#include <string>
#include <vector>

namespace RSM {

// Forward declarations
struct EventDescriptor;
class IStateNode;
class IExecutionContext;
class IInvokeNode;

/**
 * @brief Interface for concurrent regions in parallel states
 *
 * A concurrent region represents an independent execution path within
 * a parallel state. Each region maintains its own state configuration
 * and processes events independently.
 *
 * SCXML Compliance:
 * - Each region operates independently
 * - Regions can reach final states individually
 * - All regions must complete for parallel state completion
 */
class IConcurrentRegion {
public:
    virtual ~IConcurrentRegion() = default;

    /**
     * @brief Get the unique identifier for this region
     * @return Region ID string
     */
    virtual const std::string &getId() const = 0;

    /**
     * @brief Activate this region
     * @return Operation result indicating success or failure
     */
    virtual ConcurrentOperationResult activate() = 0;

    /**
     * @brief Deactivate this region
     * @param executionContext Optional execution context for proper exit action execution
     * @return Operation result indicating success or failure
     */
    virtual ConcurrentOperationResult deactivate(std::shared_ptr<IExecutionContext> executionContext = nullptr) = 0;

    /**
     * @brief Check if this region is currently active
     * @return true if region is active
     */
    virtual bool isActive() const = 0;

    /**
     * @brief Check if this region has reached a final state
     * @return true if region is in a final state
     */
    virtual bool isInFinalState() const = 0;

    /**
     * @brief Get current status of this region
     * @return Current region status
     */
    virtual ConcurrentRegionStatus getStatus() const = 0;

    /**
     * @brief Get information about this region
     * @return Region information structure
     */
    virtual ConcurrentRegionInfo getInfo() const = 0;

    /**
     * @brief Process an event in this region
     * @param event Event to process
     * @return Operation result with any generated events
     */
    virtual ConcurrentOperationResult processEvent(const EventDescriptor &event) = 0;

    /**
     * @brief Get the root state node for this region
     * @return Shared pointer to root state node
     */
    virtual std::shared_ptr<IStateNode> getRootState() const = 0;

    /**
     * @brief Set the root state node for this region
     * @param rootState Root state node for this region
     */
    virtual void setRootState(std::shared_ptr<IStateNode> rootState) = 0;

    /**
     * @brief Get currently active states in this region
     * @return Vector of active state IDs
     */
    virtual std::vector<std::string> getActiveStates() const = 0;

    /**
     * @brief Reset this region to its initial state
     * @return Operation result
     */
    virtual ConcurrentOperationResult reset() = 0;

    /**
     * @brief Validate the configuration of this region
     * @return Vector of validation error messages (empty if valid)
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Set callback for invoke deferring (W3C SCXML 6.4 compliance)
     *
     * This callback allows the region to delegate invoke execution timing
     * to the StateMachine, ensuring proper SCXML semantics via dependency inversion.
     *
     * @param callback Function to call with stateId and invoke nodes for deferring
     */
    virtual void setInvokeCallback(
        std::function<void(const std::string &, const std::vector<std::shared_ptr<IInvokeNode>> &)> callback) = 0;

    /**
     * @brief Set condition evaluator callback for transition guard evaluation
     * @param evaluator Function to evaluate guard conditions (returns true/false)
     */
    virtual void setConditionEvaluator(std::function<bool(const std::string &)> evaluator) = 0;

    /**
     * @brief Set done state callback for done.state.{id} event generation (W3C SCXML 3.4)
     *
     * Lifecycle:
     * 1. StateMachine calls this during setupParallelStateCallbacks() initialization
     * 2. Callback remains valid throughout state machine lifetime
     * 3. ConcurrentRegion invokes callback when determineIfInFinalState() returns true
     * 4. Callback generates done.state.{regionId} event via StateMachine::generateDoneStateEvent()
     *
     * Thread Safety: Callback invoked synchronously in event processing thread
     *
     * @param callback Function to call with region ID when region reaches final state
     */
    virtual void setDoneStateCallback(std::function<void(const std::string &)> callback) = 0;

    /**
     * @brief Set execution context for action execution (W3C SCXML 403c compliance)
     * @param executionContext Context for JavaScript evaluation and action execution
     */
    virtual void setExecutionContext(std::shared_ptr<IExecutionContext> executionContext) = 0;

    /**
     * @brief Set desired initial child state from parent's initial attribute (W3C SCXML 3.3)
     *
     * When a parent compound state specifies deep initial targets, this method
     * sets the target state for this region, overriding the region's default initial state.
     *
     * @param childStateId The desired initial child state ID for this region
     */
    virtual void setDesiredInitialChild(const std::string &childStateId) = 0;

    /**
     * @brief Get current state of the region
     * @return Current state ID (empty if inactive)
     */
    virtual const std::string &getCurrentState() const = 0;

    /**
     * @brief Directly set current state (for W3C SCXML 3.3 deep initial targets)
     * @param stateId The state ID to set as current
     */
    virtual void setCurrentState(const std::string &stateId) = 0;
};

}  // namespace RSM