#pragma once

#include "events/EventDescriptor.h"
#include "states/ConcurrentStateTypes.h"
#include "states/IConcurrentRegion.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RSM {

// Forward declarations
class IStateNode;

/**
 * @brief Core class that orchestrates the lifecycle of parallel regions
 *
 * This class orchestrates and manages the lifecycle of multiple regions
 * in SCXML parallel states. It manages activation, deactivation,
 * and state tracking of each region in an integrated manner.
 *
 * SCXML compliance:
 * - Simultaneously activate all regions when entering parallel state
 * - Deactivate all regions when exiting parallel state
 * - Independent state machine execution for each region
 * - Isolation and handling of per-region error situations
 */
class ParallelRegionOrchestrator {
public:
    /**
     * @brief Region state change event
     */
    enum class RegionStateChangeEvent {
        ACTIVATED,      // Region activated
        DEACTIVATED,    // Region deactivated
        COMPLETED,      // Region completed (reached final state)
        ERROR_OCCURRED  // Error occurred in region
    };

    /**
     * @brief Region state change callback type
     */
    using RegionStateChangeCallback =
        std::function<void(const std::string &regionId, RegionStateChangeEvent event, const std::string &details)>;

    /**
     * @brief Orchestration result information
     */
    struct OrchestrationResult {
        bool isSuccess = false;
        std::vector<std::string> successfulRegions;
        std::vector<std::string> failedRegions;
        std::string errorMessage;

        static OrchestrationResult success(const std::vector<std::string> &regions);
        static OrchestrationResult failure(const std::string &error);
        static OrchestrationResult partial(const std::vector<std::string> &successful,
                                           const std::vector<std::string> &failed, const std::string &error);
    };

    /**
     * @brief Create parallel region orchestrator
     * @param parentStateId ID of parent parallel state
     */
    explicit ParallelRegionOrchestrator(const std::string &parentStateId);

    /**
     * @brief Destructor
     */
    ~ParallelRegionOrchestrator();

    // Region management

    /**
     * @brief Add parallel region
     * @param region Region to add
     * @return Operation result
     */
    ConcurrentOperationResult addRegion(std::shared_ptr<IConcurrentRegion> region);

    /**
     * @brief Remove region
     * @param regionId Region ID to remove
     * @return Operation result
     */
    ConcurrentOperationResult removeRegion(const std::string &regionId);

    /**
     * @brief Find specific region
     * @param regionId Region ID to find
     * @return Region pointer (nullptr if not found)
     */
    std::shared_ptr<IConcurrentRegion> getRegion(const std::string &regionId) const;

    /**
     * @brief Get all regions list
     * @return List of regions
     */
    const std::vector<std::shared_ptr<IConcurrentRegion>> &getAllRegions() const;

    /**
     * @brief Get only active regions
     * @return List of active regions
     */
    std::vector<std::shared_ptr<IConcurrentRegion>> getActiveRegions() const;

    // Lifecycle orchestration

    /**
     * @brief Activate all regions (when entering parallel state)
     * @return Orchestration result
     */
    OrchestrationResult activateAllRegions();

    /**
     * @brief Deactivate all regions (when exiting parallel state)
     * @return Orchestration result
     */
    OrchestrationResult deactivateAllRegions();

    /**
     * @brief Activate only specific regions
     * @param regionIds List of region IDs to activate
     * @return Orchestration result
     */
    OrchestrationResult activateRegions(const std::vector<std::string> &regionIds);

    /**
     * @brief Deactivate only specific regions
     * @param regionIds List of region IDs to deactivate
     * @return Orchestration result
     */
    OrchestrationResult deactivateRegions(const std::vector<std::string> &regionIds);

    /**
     * @brief Restart all regions (reinitialize)
     * @return Orchestration result
     */
    OrchestrationResult restartAllRegions();

    // State monitoring

    /**
     * @brief Check if all regions are active
     * @return true if all regions are active
     */
    bool areAllRegionsActive() const;

    /**
     * @brief Check if all regions are completed
     * @return true if all regions are completed
     */
    bool areAllRegionsCompleted() const;

    /**
     * @brief Check if any region has errors
     * @return true if there are regions with errors
     */
    bool hasAnyRegionErrors() const;

    /**
     * @brief Get per-region state information
     * @return Map of per-region state information
     */
    std::unordered_map<std::string, ConcurrentRegionInfo> getRegionStates() const;

    // Event processing

    /**
     * @brief Broadcast event to all active regions
     * @param event Event to broadcast
     * @return Per-region processing results
     */
    std::vector<ConcurrentOperationResult> broadcastEvent(const EventDescriptor &event);

    /**
     * @brief Send event to specific region only
     * @param regionId Target region ID
     * @param event Event to send
     * @return Processing result
     */
    ConcurrentOperationResult sendEventToRegion(const std::string &regionId, const EventDescriptor &event);

    // Callback management

    /**
     * @brief Register region state change callback
     * @param callback Callback function
     */
    void setStateChangeCallback(RegionStateChangeCallback callback);

    /**
     * @brief Remove region state change callback
     */
    void clearStateChangeCallback();

    // Validation

    /**
     * @brief Validate orchestrator state
     * @return List of validation errors (empty if normal)
     */
    std::vector<std::string> validateOrchestrator() const;

    /**
     * @brief Get statistics information
     * @return Statistics information string
     */
    std::string getStatistics() const;

private:
    std::string parentStateId_;
    std::vector<std::shared_ptr<IConcurrentRegion>> regions_;
    std::unordered_map<std::string, std::shared_ptr<IConcurrentRegion>> regionMap_;
    RegionStateChangeCallback stateChangeCallback_;

    // Internal helper methods
    void notifyStateChange(const std::string &regionId, RegionStateChangeEvent event, const std::string &details = "");

    bool isRegionIdValid(const std::string &regionId) const;
    std::vector<std::string> getRegionIds() const;
    void updateRegionMap();
};

}  // namespace RSM