#pragma once

#include "ConcurrentStateTypes.h"
#include "IConcurrentRegion.h"
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
     * @return Operation result indicating success/failure
     */
    ConcurrentOperationResult deactivate() override;

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
    const std::string &getCurrentState() const;

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
    void setExecutionContext(std::shared_ptr<IExecutionContext> executionContext);

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
     * @return Operation result for state exit
     */
    ConcurrentOperationResult exitAllStates();
};

}  // namespace RSM
