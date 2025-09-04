#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations
namespace SCXML {

namespace Model {
class IStateNode;
}

namespace Runtime {
class RuntimeContext;
}

class ParallelStateNode;

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events
}  // namespace SCXML

/**
 * @brief Runtime manager for SCXML parallel states
 *
 * This class manages the execution of parallel states at runtime, handling:
 * - Concurrent region activation and deactivation
 * - Event broadcasting to all active regions
 * - Completion detection across all regions
 * - State configuration management for parallel execution
 */
class ParallelStateManager {
public:
    /**
     * @brief Parallel state execution result
     */
    struct ParallelExecutionResult {
        bool success = false;
        bool allRegionsComplete = false;
        std::vector<std::string> completedRegions;
        std::vector<std::string> activeRegions;
        std::vector<std::string> errorMessages;
        std::unordered_map<std::string, std::vector<std::string>> regionTransitions;
    };

    /**
     * @brief Parallel region state information
     */
    struct RegionState {
        std::string regionId;
        std::unordered_set<std::string> activeStates;
        std::string currentState;
        bool isComplete = false;
        bool hasError = false;
        std::string errorMessage;

        RegionState(const std::string &id) : regionId(id) {}
    };

    /**
     * @brief Event processing callback for regions
     */
    using RegionEventProcessor = std::function<bool(const std::string &regionId, SCXML::Events::EventPtr event,
                                                    SCXML::Runtime::RuntimeContext &context)>;

public:
    /**
     * @brief Constructor
     */
    ParallelStateManager();

    /**
     * @brief Destructor
     */
    virtual ~ParallelStateManager() = default;

    /**
     * @brief Enter a parallel state
     * @param parallelState Parallel state node to enter
     * @param context Runtime context
     * @return Execution result with region activation status
     */
    ParallelExecutionResult enterParallelState(std::shared_ptr<SCXML::ParallelStateNode> parallelState,
                                               SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Exit a parallel state
     * @param parallelState Parallel state node to exit
     * @param context Runtime context
     * @return Execution result with region deactivation status
     */
    ParallelExecutionResult exitParallelState(std::shared_ptr<SCXML::ParallelStateNode> parallelState,
                                              SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Process event across all active regions in parallel state
     * @param parallelStateId ID of the parallel state
     * @param event Event to process
     * @param context Runtime context
     * @return Execution result with region processing status
     */
    ParallelExecutionResult processEventInParallelState(const std::string &parallelStateId,
                                                        SCXML::Events::EventPtr event,
                                                        SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check if parallel state is complete
     * @param parallelStateId ID of the parallel state to check
     * @return true if all regions in the parallel state are complete
     */
    bool isParallelStateComplete(const std::string &parallelStateId) const;

    /**
     * @brief Get active regions in a parallel state
     * @param parallelStateId ID of the parallel state
     * @return Vector of active region IDs
     */
    std::vector<std::string> getActiveRegions(const std::string &parallelStateId) const;

    /**
     * @brief Get completed regions in a parallel state
     * @param parallelStateId ID of the parallel state
     * @return Vector of completed region IDs
     */
    std::vector<std::string> getCompletedRegions(const std::string &parallelStateId) const;

    /**
     * @brief Get current state configuration for all active parallel states
     * @return Map of parallel state ID to active region states
     */
    std::unordered_map<std::string, std::vector<RegionState>> getCurrentConfiguration() const;

    /**
     * @brief Set region event processor callback
     * @param processor Function to process events within regions
     */
    void setRegionEventProcessor(RegionEventProcessor processor);

    /**
     * @brief Register a parallel state for management
     * @param parallelState Parallel state to register
     * @return true if registration successful
     */
    bool registerParallelState(std::shared_ptr<SCXML::ParallelStateNode> parallelState);

    /**
     * @brief Unregister a parallel state
     * @param parallelStateId ID of parallel state to unregister
     * @return true if unregistration successful
     */
    bool unregisterParallelState(const std::string &parallelStateId);

    /**
     * @brief Update region state information
     * @param parallelStateId ID of the parallel state
     * @param regionId ID of the region
     * @param activeStates Current active states in the region
     * @param isComplete Whether the region has completed
     */
    void updateRegionState(const std::string &parallelStateId, const std::string &regionId,
                           const std::unordered_set<std::string> &activeStates, bool isComplete = false);

    /**
     * @brief Get detailed region state information
     * @param parallelStateId ID of the parallel state
     * @param regionId ID of the region
     * @return Region state information, or nullptr if not found
     */
    std::shared_ptr<RegionState> getRegionState(const std::string &parallelStateId, const std::string &regionId) const;

    /**
     * @brief Clear all parallel state tracking
     */
    void clearAllStates();

    /**
     * @brief Get statistics about parallel state execution
     */
    struct ParallelStateStatistics {
        size_t totalParallelStates = 0;
        size_t activeParallelStates = 0;
        size_t totalRegions = 0;
        size_t activeRegions = 0;
        size_t completedRegions = 0;
    };

    /**
     * @brief Get execution statistics
     * @return Current parallel state statistics
     */
    ParallelStateStatistics getStatistics() const;

protected:
    /**
     * @brief Enter all regions of a parallel state
     * @param parallelState Parallel state containing regions
     * @param context Runtime context
     * @return Vector of region entry results
     */
    std::vector<bool> enterAllRegions(std::shared_ptr<SCXML::ParallelStateNode> parallelState,
                                      SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Exit all regions of a parallel state
     * @param parallelState Parallel state containing regions
     * @param context Runtime context
     * @return Vector of region exit results
     */
    std::vector<bool> exitAllRegions(std::shared_ptr<SCXML::ParallelStateNode> parallelState,
                                     SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Enter a specific region
     * @param region Region state node to enter
     * @param initialState Initial state within the region
     * @param context Runtime context
     * @return true if region entry successful
     */
    bool enterRegion(std::shared_ptr<SCXML::Model::IStateNode> region, const std::string &initialState,
                     SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Exit a specific region
     * @param region Region state node to exit
     * @param context Runtime context
     * @return true if region exit successful
     */
    bool exitRegion(std::shared_ptr<SCXML::Model::IStateNode> region, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check if all regions in parallel state are complete
     * @param parallelStateId ID of parallel state to check
     * @return true if all regions are complete
     */
    bool checkAllRegionsComplete(const std::string &parallelStateId) const;

private:
    // Parallel state tracking
    std::unordered_map<std::string, std::shared_ptr<SCXML::ParallelStateNode>> registeredParallelStates_;
    std::unordered_map<std::string, std::vector<RegionState>> parallelStateRegions_;

    // Event processing
    RegionEventProcessor regionEventProcessor_;

    // Thread safety
    mutable std::mutex stateMutex_;

    /**
     * @brief Generate unique region key
     * @param parallelStateId Parallel state ID
     * @param regionId Region ID
     * @return Unique key for region identification
     */
    std::string generateRegionKey(const std::string &parallelStateId, const std::string &regionId) const;
};