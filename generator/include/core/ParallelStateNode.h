#pragma once
#include "core/StateNode.h"
#include <mutex>
#include <unordered_set>

namespace SCXML {
namespace Core {

/**
 * @brief SCXML Parallel State implementation
 *
 * A parallel state contains multiple child states that execute concurrently.
 * When a parallel state is entered, all its child states are entered simultaneously.
 * Events are broadcast to all active child states within the parallel state.
 *
 * Per W3C SCXML specification:
 * - All child states of a parallel state are active simultaneously
 * - Events are processed by all child states
 * - Parallel state completes when all child states reach final states
 * - Transitions out of parallel states are checked after all child processing
 */
class ParallelStateNode : public StateNode {
public:
    /**
     * @brief Construct a parallel state node
     * @param id State identifier
     */
    explicit ParallelStateNode(const std::string &id);

    /**
     * @brief Virtual destructor
     */
    virtual ~ParallelStateNode() = default;

    /**
     * @brief Get state type (always PARALLEL)
     * @return Type::PARALLEL
     */
    Type getType() const override {
        return Type::PARALLEL;
    }

    /**
     * @brief Add a child state (parallel region)
     * @param child Child state to add as parallel region
     */
    void addChild(std::shared_ptr<IStateNode> child) override;

    /**
     * @brief Add a parallel region with explicit configuration
     * @param region State node representing a parallel region
     * @param initialState Initial state within this region (optional)
     */
    void addParallelRegion(std::shared_ptr<IStateNode> region, const std::string &initialState = "");

    /**
     * @brief Get all parallel regions
     * @return Vector of parallel regions (same as children)
     */
    std::vector<std::shared_ptr<IStateNode>> getParallelRegions() const override;

    /**
     * @brief Check if all parallel regions have completed
     * @return true if all regions are in final states
     */
    bool areAllRegionsComplete() const;

    /**
     * @brief Get completion status for each region
     * @return Map of region ID to completion status
     */
    std::unordered_map<std::string, bool> getRegionCompletionStatus() const;

    /**
     * @brief Check if a specific region is complete
     * @param regionId ID of the region to check
     * @return true if region is in final state
     */
    bool isRegionComplete(const std::string &regionId) const;

    /**
     * @brief Set active states for a specific region
     * @param regionId Region identifier
     * @param activeStates Set of active state IDs in this region
     */
    void setRegionActiveStates(const std::string &regionId, const std::unordered_set<std::string> &activeStates);

    /**
     * @brief Get active states for a specific region
     * @param regionId Region identifier
     * @return Set of active state IDs in this region
     */
    std::unordered_set<std::string> getRegionActiveStates(const std::string &regionId) const;

    /**
     * @brief Get all active states across all regions
     * @return Set of all active state IDs
     */
    std::unordered_set<std::string> getAllActiveStates() const;

    /**
     * @brief Mark a region as completed
     * @param regionId Region identifier
     */
    void markRegionComplete(const std::string &regionId);

    /**
     * @brief Mark a region as active (not completed)
     * @param regionId Region identifier
     */
    void markRegionActive(const std::string &regionId);

    /**
     * @brief Reset all region states (for re-entry)
     */
    void resetRegionStates();

    /**
     * @brief Validate parallel state structure
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validateParallelStructure() const;

    /**
     * @brief Check if this parallel state can accept events
     * @return true if at least one region is active
     */
    bool canProcessEvents() const;

    /**
     * @brief Get initial states for all regions
     * @return Map of region ID to initial state ID
     */
    std::unordered_map<std::string, std::string> getRegionInitialStates() const;

protected:
    /**
     * @brief Find initial state for a given region
     * @param region Region state node
     * @return Initial state ID for the region
     */
    std::string findRegionInitialState(std::shared_ptr<IStateNode> region) const;

    /**
     * @brief Check if a region contains only final states
     * @param region Region state node to check
     * @return true if region has reached completion
     */
    bool isRegionInFinalState(std::shared_ptr<IStateNode> region) const;

    /**
     * @brief Check if a state is a final state
     * @param stateId State identifier to check
     * @return true if state is a final state
     */
    bool isStateFinal(const std::string &stateId) const;

private:
    // Region state tracking
    std::unordered_map<std::string, std::unordered_set<std::string>> regionActiveStates_;
    std::unordered_map<std::string, bool> regionCompletionStatus_;
    std::unordered_map<std::string, std::string> regionInitialStates_;

    // Thread safety for concurrent access
    mutable std::mutex regionStateMutex_;

    /**
     * @brief Initialize region tracking for a new child
     * @param child Child state being added
     */
    void initializeRegionTracking(std::shared_ptr<IStateNode> child);
};

} // namespace Core
}  // namespace SCXML
