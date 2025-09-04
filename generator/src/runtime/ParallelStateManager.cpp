#include "runtime/ParallelStateManager.h"
#include "Event.h"
#include "common/Logger.h"
#include "core/ParallelStateNode.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>

namespace SCXML {

ParallelStateManager::ParallelStateManager() {
    Logger::debug("ParallelStateManager::Constructor - Initialized parallel state manager");
}

ParallelStateManager::ParallelExecutionResult
ParallelStateManager::enterParallelState(std::shared_ptr<ParallelStateNode> parallelState,
                                         SCXML::Runtime::RuntimeContext &context) {
    ParallelExecutionResult result;

    if (!parallelState) {
        result.errorMessages.push_back("Null parallel state provided");
        return result;
    }

    std::string parallelStateId = parallelState->getId();
    Logger::info("ParallelStateManager::enterParallelState - Entering parallel state: " + parallelStateId);

    try {
        std::lock_guard<std::mutex> lock(stateMutex_);

        // Register parallel state if not already registered
        if (registeredParallelStates_.find(parallelStateId) == registeredParallelStates_.end()) {
            registerParallelState(parallelState);
        }

        // Reset region states for re-entry
        parallelState->resetRegionStates();

        // Enter all regions
        auto regionResults = enterAllRegions(parallelState, context);

        // Update result
        const auto &regions = parallelState->getParallelRegions();
        for (size_t i = 0; i < regions.size() && i < regionResults.size(); ++i) {
            if (regions[i]) {
                std::string regionId = regions[i]->getId();
                if (regionResults[i]) {
                    result.activeRegions.push_back(regionId);
                } else {
                    result.errorMessages.push_back("Failed to enter region: " + regionId);
                }
            }
        }

        result.success = !result.activeRegions.empty();
        result.allRegionsComplete = parallelState->areAllRegionsComplete();

        if (result.success) {
            Logger::info("ParallelStateManager::enterParallelState - Successfully entered " +
                         std::to_string(result.activeRegions.size()) + " regions");
        }

    } catch (const std::exception &e) {
        result.errorMessages.push_back("Exception entering parallel state: " + std::string(e.what()));
        Logger::error("ParallelStateManager::enterParallelState - Exception: " + std::string(e.what()));
    }

    return result;
}

ParallelStateManager::ParallelExecutionResult
ParallelStateManager::exitParallelState(std::shared_ptr<ParallelStateNode> parallelState,
                                        SCXML::Runtime::RuntimeContext &context) {
    ParallelExecutionResult result;

    if (!parallelState) {
        result.errorMessages.push_back("Null parallel state provided");
        return result;
    }

    std::string parallelStateId = parallelState->getId();
    Logger::info("ParallelStateManager::exitParallelState - Exiting parallel state: " + parallelStateId);

    try {
        std::lock_guard<std::mutex> lock(stateMutex_);

        // Exit all regions
        auto regionResults = exitAllRegions(parallelState, context);

        // Update result
        const auto &regions = parallelState->getParallelRegions();
        for (size_t i = 0; i < regions.size() && i < regionResults.size(); ++i) {
            if (regions[i]) {
                std::string regionId = regions[i]->getId();
                if (regionResults[i]) {
                    result.completedRegions.push_back(regionId);
                } else {
                    result.errorMessages.push_back("Failed to exit region: " + regionId);
                }
            }
        }

        // Clear region tracking
        parallelStateRegions_.erase(parallelStateId);

        result.success = regionResults.size() == result.completedRegions.size();
        result.allRegionsComplete = true;

        Logger::info("ParallelStateManager::exitParallelState - Exited " +
                     std::to_string(result.completedRegions.size()) + " regions");

    } catch (const std::exception &e) {
        result.errorMessages.push_back("Exception exiting parallel state: " + std::string(e.what()));
        Logger::error("ParallelStateManager::exitParallelState - Exception: " + std::string(e.what()));
    }

    return result;
}

ParallelStateManager::ParallelExecutionResult
ParallelStateManager::processEventInParallelState(const std::string &parallelStateId, SCXML::Events::EventPtr event,
                                                  SCXML::Runtime::RuntimeContext &context) {
    ParallelExecutionResult result;

    if (!event) {
        result.errorMessages.push_back("Null event provided");
        return result;
    }

    Logger::debug("ParallelStateManager::processEventInParallelState - Processing event '" + event->getName() +
                  "' in parallel state: " + parallelStateId);

    try {
        std::lock_guard<std::mutex> lock(stateMutex_);

        auto it = parallelStateRegions_.find(parallelStateId);
        if (it == parallelStateRegions_.end()) {
            result.errorMessages.push_back("Parallel state not found: " + parallelStateId);
            return result;
        }

        // Process event in all active regions
        std::vector<RegionState> &regions = it->second;
        size_t processedRegions = 0;

        for (auto &region : regions) {
            if (!region.isComplete && !region.hasError) {
                bool processed = false;

                // Use custom region event processor if available
                if (regionEventProcessor_) {
                    processed = regionEventProcessor_(region.regionId, event, context);
                } else {
                    // Default processing - mark as processed
                    processed = true;
                    Logger::debug(
                        "ParallelStateManager::processEventInParallelState - Default processing for region: " +
                        region.regionId);
                }

                if (processed) {
                    processedRegions++;
                    result.activeRegions.push_back(region.regionId);

                    // Record transition information (simplified)
                    result.regionTransitions[region.regionId] = {event->getName()};
                } else {
                    result.errorMessages.push_back("Failed to process event in region: " + region.regionId);
                }
            } else if (region.isComplete) {
                result.completedRegions.push_back(region.regionId);
            }
        }

        result.success = processedRegions > 0;
        result.allRegionsComplete = checkAllRegionsComplete(parallelStateId);

        Logger::debug("ParallelStateManager::processEventInParallelState - Processed event in " +
                      std::to_string(processedRegions) +
                      " regions, all complete: " + (result.allRegionsComplete ? "true" : "false"));

    } catch (const std::exception &e) {
        result.errorMessages.push_back("Exception processing event: " + std::string(e.what()));
        Logger::error("ParallelStateManager::processEventInParallelState - Exception: " + std::string(e.what()));
    }

    return result;
}

bool ParallelStateManager::isParallelStateComplete(const std::string &parallelStateId) const {
    return checkAllRegionsComplete(parallelStateId);
}

std::vector<std::string> ParallelStateManager::getActiveRegions(const std::string &parallelStateId) const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    std::vector<std::string> activeRegions;

    auto it = parallelStateRegions_.find(parallelStateId);
    if (it != parallelStateRegions_.end()) {
        for (const auto &region : it->second) {
            if (!region.isComplete && !region.hasError) {
                activeRegions.push_back(region.regionId);
            }
        }
    }

    return activeRegions;
}

std::vector<std::string> ParallelStateManager::getCompletedRegions(const std::string &parallelStateId) const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    std::vector<std::string> completedRegions;

    auto it = parallelStateRegions_.find(parallelStateId);
    if (it != parallelStateRegions_.end()) {
        for (const auto &region : it->second) {
            if (region.isComplete) {
                completedRegions.push_back(region.regionId);
            }
        }
    }

    return completedRegions;
}

std::unordered_map<std::string, std::vector<ParallelStateManager::RegionState>>
ParallelStateManager::getCurrentConfiguration() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return parallelStateRegions_;
}

void ParallelStateManager::setRegionEventProcessor(RegionEventProcessor processor) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    regionEventProcessor_ = processor;
    Logger::debug("ParallelStateManager::setRegionEventProcessor - Set custom region event processor");
}

bool ParallelStateManager::registerParallelState(std::shared_ptr<ParallelStateNode> parallelState) {
    if (!parallelState) {
        Logger::warning("ParallelStateManager::registerParallelState - Null parallel state provided");
        return false;
    }

    std::string parallelStateId = parallelState->getId();

    std::lock_guard<std::mutex> lock(stateMutex_);

    // Validate parallel state structure
    auto validationErrors = parallelState->validateParallelStructure();
    if (!validationErrors.empty()) {
        Logger::error("ParallelStateManager::registerParallelState - Validation failed for " + parallelStateId);
        for (const auto &error : validationErrors) {
            Logger::error("  - " + error);
        }
        return false;
    }

    registeredParallelStates_[parallelStateId] = parallelState;

    // Initialize region tracking
    std::vector<RegionState> regions;
    for (const auto &region : parallelState->getParallelRegions()) {
        if (region) {
            regions.emplace_back(region->getId());
        }
    }
    parallelStateRegions_[parallelStateId] = std::move(regions);

    Logger::info("ParallelStateManager::registerParallelState - Registered parallel state: " + parallelStateId +
                 " with " + std::to_string(parallelState->getParallelRegions().size()) + " regions");

    return true;
}

bool ParallelStateManager::unregisterParallelState(const std::string &parallelStateId) {
    std::lock_guard<std::mutex> lock(stateMutex_);

    bool found = false;

    auto stateIt = registeredParallelStates_.find(parallelStateId);
    if (stateIt != registeredParallelStates_.end()) {
        registeredParallelStates_.erase(stateIt);
        found = true;
    }

    auto regionIt = parallelStateRegions_.find(parallelStateId);
    if (regionIt != parallelStateRegions_.end()) {
        parallelStateRegions_.erase(regionIt);
        found = true;
    }

    if (found) {
        Logger::info("ParallelStateManager::unregisterParallelState - Unregistered parallel state: " + parallelStateId);
    }

    return found;
}

void ParallelStateManager::updateRegionState(const std::string &parallelStateId, const std::string &regionId,
                                             const std::unordered_set<std::string> &activeStates, bool isComplete) {
    std::lock_guard<std::mutex> lock(stateMutex_);

    auto it = parallelStateRegions_.find(parallelStateId);
    if (it == parallelStateRegions_.end()) {
        Logger::warning("ParallelStateManager::updateRegionState - Parallel state not found: " + parallelStateId);
        return;
    }

    // Find region and update its state
    bool regionFound = false;
    for (auto &region : it->second) {
        if (region.regionId == regionId) {
            region.activeStates = activeStates;
            region.isComplete = isComplete;

            // Update current state (use first active state as representative)
            if (!activeStates.empty()) {
                region.currentState = *activeStates.begin();
            } else {
                region.currentState = "";
            }

            regionFound = true;
            break;
        }
    }

    if (regionFound) {
        Logger::debug("ParallelStateManager::updateRegionState - Updated region " + regionId + " in parallel state " +
                      parallelStateId + ", complete: " + (isComplete ? "true" : "false"));
    } else {
        Logger::warning("ParallelStateManager::updateRegionState - Region not found: " + regionId);
    }
}

std::shared_ptr<ParallelStateManager::RegionState>
ParallelStateManager::getRegionState(const std::string &parallelStateId, const std::string &regionId) const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    auto it = parallelStateRegions_.find(parallelStateId);
    if (it != parallelStateRegions_.end()) {
        for (const auto &region : it->second) {
            if (region.regionId == regionId) {
                return std::make_shared<RegionState>(region);
            }
        }
    }

    return nullptr;
}

void ParallelStateManager::clearAllStates() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    registeredParallelStates_.clear();
    parallelStateRegions_.clear();

    Logger::info("ParallelStateManager::clearAllStates - Cleared all parallel state tracking");
}

ParallelStateManager::ParallelStateStatistics ParallelStateManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    ParallelStateStatistics stats;

    stats.totalParallelStates = registeredParallelStates_.size();

    for (const auto &pair : parallelStateRegions_) {
        bool hasActiveRegions = false;

        for (const auto &region : pair.second) {
            stats.totalRegions++;

            if (region.isComplete) {
                stats.completedRegions++;
            } else if (!region.hasError) {
                stats.activeRegions++;
                hasActiveRegions = true;
            }
        }

        if (hasActiveRegions) {
            stats.activeParallelStates++;
        }
    }

    return stats;
}

// Protected methods implementation

std::vector<bool> ParallelStateManager::enterAllRegions(std::shared_ptr<ParallelStateNode> parallelState,
                                                        SCXML::Runtime::RuntimeContext &context) {
    std::vector<bool> results;

    const auto &regions = parallelState->getParallelRegions();
    const auto &initialStates = parallelState->getRegionInitialStates();

    for (const auto &region : regions) {
        if (region) {
            std::string regionId = region->getId();
            std::string initialState;

            auto it = initialStates.find(regionId);
            if (it != initialStates.end()) {
                initialState = it->second;
            }

            bool success = enterRegion(region, initialState, context);
            results.push_back(success);

            if (success) {
                Logger::debug("ParallelStateManager::enterAllRegions - Entered region: " + regionId);
            } else {
                Logger::warning("ParallelStateManager::enterAllRegions - Failed to enter region: " + regionId);
            }
        } else {
            results.push_back(false);
        }
    }

    return results;
}

std::vector<bool> ParallelStateManager::exitAllRegions(std::shared_ptr<ParallelStateNode> parallelState,
                                                       SCXML::Runtime::RuntimeContext &context) {
    std::vector<bool> results;

    const auto &regions = parallelState->getParallelRegions();

    for (const auto &region : regions) {
        if (region) {
            bool success = exitRegion(region, context);
            results.push_back(success);

            std::string regionId = region->getId();
            if (success) {
                Logger::debug("ParallelStateManager::exitAllRegions - Exited region: " + regionId);
            } else {
                Logger::warning("ParallelStateManager::exitAllRegions - Failed to exit region: " + regionId);
            }
        } else {
            results.push_back(false);
        }
    }

    return results;
}

bool ParallelStateManager::enterRegion(std::shared_ptr<SCXML::Model::IStateNode> region, const std::string &initialState,
                                       SCXML::Runtime::RuntimeContext &context) {
    if (!region) {
        return false;
    }

    // Implementation would depend on the specific state machine runtime
    // For now, simulate successful entry
    Logger::debug("ParallelStateManager::enterRegion - Entering region: " + region->getId() +
                  " with initial state: " + initialState);

    // Mark region as active
    std::unordered_set<std::string> activeStates;
    if (!initialState.empty()) {
        activeStates.insert(initialState);
    } else {
        activeStates.insert(region->getId());
    }

    // This would be called by the actual runtime when entering the region
    // updateRegionState(parentParallelStateId, region->getId(), activeStates, false);

    return true;
}

bool ParallelStateManager::exitRegion(std::shared_ptr<SCXML::Model::IStateNode> region,
                                      SCXML::Runtime::RuntimeContext &context) {
    if (!region) {
        return false;
    }

    // Implementation would depend on the specific state machine runtime
    Logger::debug("ParallelStateManager::exitRegion - Exiting region: " + region->getId());

    // This would be called by the actual runtime when exiting the region
    // updateRegionState(parentParallelStateId, region->getId(), {}, true);

    return true;
}

bool ParallelStateManager::checkAllRegionsComplete(const std::string &parallelStateId) const {
    auto it = parallelStateRegions_.find(parallelStateId);
    if (it == parallelStateRegions_.end()) {
        return false;
    }

    for (const auto &region : it->second) {
        if (!region.isComplete) {
            return false;
        }
    }

    return !it->second.empty();  // True only if we have regions and all are complete
}

std::string ParallelStateManager::generateRegionKey(const std::string &parallelStateId,
                                                    const std::string &regionId) const {
    return parallelStateId + "::" + regionId;
}

}  // namespace SCXML