#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RSM {

/**
 * @brief Class responsible for handling external transitions in parallel states
 *
 * Responsible for deactivating all active regions in proper order
 * when external transitions occur in SCXML parallel states.
 */
class ExternalTransitionHandler {
public:
    /**
     * @brief Constructor
     * @param maxConcurrentTransitions Maximum number of concurrent transitions
     */
    explicit ExternalTransitionHandler(size_t maxConcurrentTransitions = 10);

    /**
     * @brief Handle external transition
     * @param parallelStateId Parallel state ID
     * @param targetStateId Target state ID
     * @param transitionEvent Transition event
     * @return Success status
     */
    bool handleExternalTransition(const std::string &parallelStateId, const std::string &targetStateId,
                                  const std::string &transitionEvent);

    /**
     * @brief Register parallel state
     * @param parallelStateId Parallel state ID
     * @param regionIds List of region IDs
     */
    void registerParallelState(const std::string &parallelStateId, const std::vector<std::string> &regionIds);

    /**
     * @brief Get current active transition count
     * @return Active transition count
     */
    size_t getActiveTransitionCount() const;

    /**
     * @brief Check if processing transitions
     * @return true if processing
     */
    bool isProcessingTransitions() const;

private:
    struct RegionInfo {
        std::string regionId;
        bool isActive = false;
        size_t activationCount = 0;
        size_t deactivationCount = 0;
    };

    struct ParallelStateInfo {
        std::string stateId;
        std::vector<std::string> regionIds;
        std::unordered_map<std::string, RegionInfo> regions;
        bool isActive = false;
    };

    size_t maxConcurrentTransitions_;
    std::atomic<size_t> activeTransitions_{0};
    std::atomic<bool> isProcessing_{false};

    mutable std::mutex parallelStatesMutex_;
    std::unordered_map<std::string, ParallelStateInfo> parallelStates_;

    // Internal helper methods
    bool validateTransitionParameters(const std::string &parallelStateId, const std::string &targetStateId,
                                      const std::string &transitionEvent) const;

    bool deactivateAllRegions(const std::string &parallelStateId);

    bool executeRegionExitActions(const std::string &regionId, const std::string &parallelStateId);

    bool isExternalTransition(const std::string &sourceStateId, const std::string &targetStateId) const;

    bool isTargetReachable(const std::string &parallelStateId, const std::string &targetStateId) const;
};

}  // namespace RSM