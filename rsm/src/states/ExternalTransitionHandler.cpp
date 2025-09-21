#include "states/ExternalTransitionHandler.h"
#include <algorithm>
#include <stdexcept>

namespace RSM {

ExternalTransitionHandler::ExternalTransitionHandler(size_t maxConcurrentTransitions)
    : maxConcurrentTransitions_(maxConcurrentTransitions) {
    if (maxConcurrentTransitions == 0) {
        throw std::invalid_argument("maxConcurrentTransitions must be greater than 0");
    }
}

bool ExternalTransitionHandler::handleExternalTransition(const std::string &parallelStateId,
                                                         const std::string &targetStateId,
                                                         const std::string &transitionEvent) {
    // Check concurrent transition limit
    if (activeTransitions_.load() >= maxConcurrentTransitions_) {
        return false;
    }

    // Validate parameters
    if (!validateTransitionParameters(parallelStateId, targetStateId, transitionEvent)) {
        return false;
    }

    // Check if transition is actually external
    if (!isExternalTransition(parallelStateId, targetStateId)) {
        return false;
    }

    // Check if target is reachable
    if (!isTargetReachable(parallelStateId, targetStateId)) {
        return false;
    }

    isProcessing_.store(true);
    activeTransitions_.fetch_add(1);

    bool success = false;
    try {
        // SCXML compliance: Deactivate all child regions in reverse document order
        success = deactivateAllRegions(parallelStateId);

        if (success) {
            // Update parallel state info
            std::lock_guard<std::mutex> lock(parallelStatesMutex_);
            auto it = parallelStates_.find(parallelStateId);
            if (it != parallelStates_.end()) {
                it->second.isActive = false;
            }
        }
    } catch (const std::exception &) {
        success = false;
    }

    activeTransitions_.fetch_sub(1);
    if (activeTransitions_.load() == 0) {
        isProcessing_.store(false);
    }

    return success;
}

bool ExternalTransitionHandler::deactivateAllRegions(const std::string &parallelStateId) {
    std::lock_guard<std::mutex> lock(parallelStatesMutex_);

    auto it = parallelStates_.find(parallelStateId);
    if (it == parallelStates_.end()) {
        return false;
    }

    auto &parallelState = it->second;

    // SCXML compliance: Deactivate regions in reverse document order
    std::vector<std::string> regionIds = parallelState.regionIds;
    std::reverse(regionIds.begin(), regionIds.end());

    bool allSuccess = true;
    for (const auto &regionId : regionIds) {
        auto regionIt = parallelState.regions.find(regionId);
        if (regionIt != parallelState.regions.end() && regionIt->second.isActive) {
            bool success = executeRegionExitActions(regionId, parallelStateId);
            if (success) {
                regionIt->second.isActive = false;
                regionIt->second.deactivationCount++;
            } else {
                allSuccess = false;
            }
        }
    }

    return allSuccess;
}

bool ExternalTransitionHandler::executeRegionExitActions(const std::string &regionId,
                                                         const std::string &parallelStateId) {
    // Validate parameters
    if (regionId.empty() || parallelStateId.empty()) {
        return false;
    }

    // Check if region exists and is active
    auto it = parallelStates_.find(parallelStateId);
    if (it == parallelStates_.end()) {
        return false;
    }

    auto regionIt = it->second.regions.find(regionId);
    if (regionIt == it->second.regions.end() || !regionIt->second.isActive) {
        return false;
    }

    // In a real implementation, this would execute the actual exit actions
    // For now, we simulate successful execution
    return true;
}

bool ExternalTransitionHandler::isExternalTransition(const std::string &sourceStateId,
                                                     const std::string &targetStateId) const {
    if (sourceStateId == targetStateId) {
        return false;
    }

    // Simplified: assume all non-self transitions are external
    return true;
}

bool ExternalTransitionHandler::isTargetReachable(const std::string &parallelStateId,
                                                  const std::string &targetStateId) const {
    if (parallelStateId.empty() || targetStateId.empty()) {
        return false;
    }

    // In a simplified implementation, we assume all states are reachable
    return true;
}

void ExternalTransitionHandler::registerParallelState(const std::string &parallelStateId,
                                                      const std::vector<std::string> &regionIds) {
    if (parallelStateId.empty()) {
        throw std::invalid_argument("parallelStateId cannot be empty");
    }

    std::lock_guard<std::mutex> lock(parallelStatesMutex_);

    ParallelStateInfo stateInfo;
    stateInfo.stateId = parallelStateId;
    stateInfo.regionIds = regionIds;
    stateInfo.isActive = false;

    // Initialize region info
    for (const auto &regionId : regionIds) {
        if (!regionId.empty()) {
            RegionInfo regionInfo;
            regionInfo.regionId = regionId;
            regionInfo.isActive = false;
            stateInfo.regions[regionId] = regionInfo;
        }
    }

    parallelStates_[parallelStateId] = std::move(stateInfo);
}

size_t ExternalTransitionHandler::getActiveTransitionCount() const {
    return activeTransitions_.load();
}

bool ExternalTransitionHandler::isProcessingTransitions() const {
    return isProcessing_.load();
}

// Private methods
bool ExternalTransitionHandler::validateTransitionParameters(const std::string &parallelStateId,
                                                             const std::string &targetStateId,
                                                             const std::string &transitionEvent) const {
    return !parallelStateId.empty() && !targetStateId.empty() && !transitionEvent.empty();
}

}  // namespace RSM