#include "states/ParallelRegionOrchestrator.h"
#include "common/Logger.h"
#include "events/EventDescriptor.h"
#include <algorithm>
#include <sstream>

namespace RSM {

// OrchestrationResult static factory methods

ParallelRegionOrchestrator::OrchestrationResult
ParallelRegionOrchestrator::OrchestrationResult::success(const std::vector<std::string> &regions) {
    OrchestrationResult result;
    result.isSuccess = true;
    result.successfulRegions = regions;
    return result;
}

ParallelRegionOrchestrator::OrchestrationResult
ParallelRegionOrchestrator::OrchestrationResult::failure(const std::string &error) {
    OrchestrationResult result;
    result.isSuccess = false;
    result.errorMessage = error;
    return result;
}

ParallelRegionOrchestrator::OrchestrationResult ParallelRegionOrchestrator::OrchestrationResult::partial(
    const std::vector<std::string> &successful, const std::vector<std::string> &failed, const std::string &error) {
    OrchestrationResult result;
    result.isSuccess = failed.empty();  // Success if nothing failed
    result.successfulRegions = successful;
    result.failedRegions = failed;
    result.errorMessage = error;
    return result;
}

// ParallelRegionOrchestrator implementation

ParallelRegionOrchestrator::ParallelRegionOrchestrator(const std::string &parentStateId)
    : parentStateId_(parentStateId) {
    LOG_DEBUG("Creating orchestrator for state: {}", parentStateId_);
}

ParallelRegionOrchestrator::~ParallelRegionOrchestrator() {
    LOG_DEBUG("Destroying orchestrator for state: {}", parentStateId_);

    // Safe shutdown: deactivate all regions
    if (!regions_.empty()) {
        deactivateAllRegions();
    }
}

// Region management

ConcurrentOperationResult ParallelRegionOrchestrator::addRegion(std::shared_ptr<IConcurrentRegion> region) {
    if (!region) {
        return ConcurrentOperationResult::failure("", "Cannot add null region");
    }

    const std::string &regionId = region->getId();

    // Check for duplicates
    if (regionMap_.find(regionId) != regionMap_.end()) {
        return ConcurrentOperationResult::failure(regionId,
                                                  fmt::format("Region with ID '{}' already exists", regionId));
    }

    // Add region
    regions_.push_back(region);
    regionMap_[regionId] = region;

    LOG_DEBUG("Added region '{}' to orchestrator for {}", regionId, parentStateId_);

    // Notify state change
    notifyStateChange(regionId, RegionStateChangeEvent::ACTIVATED, "Region added to orchestrator");

    return ConcurrentOperationResult::success(regionId);
}

ConcurrentOperationResult ParallelRegionOrchestrator::removeRegion(const std::string &regionId) {
    auto mapIt = regionMap_.find(regionId);
    if (mapIt == regionMap_.end()) {
        return ConcurrentOperationResult::failure(regionId, fmt::format("Region with ID '{}' not found", regionId));
    }

    // Deactivate region first if it's active
    auto region = mapIt->second;
    if (region->isActive()) {
        auto deactivateResult = region->deactivate();
        if (!deactivateResult.isSuccess) {
            LOG_WARN("Failed to deactivate region '{}': {}", regionId, deactivateResult.errorMessage);
        }
    }

    // Remove from vector
    auto vectorIt =
        std::find_if(regions_.begin(), regions_.end(),
                     [&regionId](const std::shared_ptr<IConcurrentRegion> &r) { return r->getId() == regionId; });

    if (vectorIt != regions_.end()) {
        regions_.erase(vectorIt);
    }

    // Remove from map
    regionMap_.erase(mapIt);

    LOG_DEBUG("Removed region '{}' from orchestrator for {}", regionId, parentStateId_);

    // Notify state change
    notifyStateChange(regionId, RegionStateChangeEvent::DEACTIVATED, "Region removed from orchestrator");

    return ConcurrentOperationResult::success(regionId);
}

std::shared_ptr<IConcurrentRegion> ParallelRegionOrchestrator::getRegion(const std::string &regionId) const {
    auto it = regionMap_.find(regionId);
    return (it != regionMap_.end()) ? it->second : nullptr;
}

const std::vector<std::shared_ptr<IConcurrentRegion>> &ParallelRegionOrchestrator::getAllRegions() const {
    return regions_;
}

std::vector<std::shared_ptr<IConcurrentRegion>> ParallelRegionOrchestrator::getActiveRegions() const {
    std::vector<std::shared_ptr<IConcurrentRegion>> activeRegions;

    for (const auto &region : regions_) {
        if (region->isActive()) {
            activeRegions.push_back(region);
        }
    }

    return activeRegions;
}

// Lifecycle orchestration

ParallelRegionOrchestrator::OrchestrationResult ParallelRegionOrchestrator::activateAllRegions() {
    LOG_DEBUG("Activating {} regions for {}", regions_.size(), parentStateId_);

    std::vector<std::string> successful;
    std::vector<std::string> failed;
    std::ostringstream errorStream;

    for (auto &region : regions_) {
        const std::string &regionId = region->getId();
        auto result = region->activate();

        if (result.isSuccess) {
            successful.push_back(regionId);
            notifyStateChange(regionId, RegionStateChangeEvent::ACTIVATED);
            LOG_DEBUG("Successfully activated region: {}", regionId);
        } else {
            failed.push_back(regionId);
            if (!errorStream.str().empty()) {
                errorStream << "; ";
            }
            errorStream << regionId << ": " << result.errorMessage;
            notifyStateChange(regionId, RegionStateChangeEvent::ERROR_OCCURRED, result.errorMessage);
            LOG_WARN("Failed to activate region '{}': {}", regionId, result.errorMessage);
        }
    }

    return OrchestrationResult::partial(successful, failed, errorStream.str());
}

ParallelRegionOrchestrator::OrchestrationResult ParallelRegionOrchestrator::deactivateAllRegions() {
    LOG_DEBUG("Deactivating {} regions for {}", regions_.size(), parentStateId_);

    std::vector<std::string> successful;
    std::vector<std::string> failed;
    std::ostringstream errorStream;

    for (auto &region : regions_) {
        const std::string &regionId = region->getId();
        auto result = region->deactivate();

        if (result.isSuccess) {
            successful.push_back(regionId);
            notifyStateChange(regionId, RegionStateChangeEvent::DEACTIVATED);
            LOG_DEBUG("Successfully deactivated region: {}", regionId);
        } else {
            failed.push_back(regionId);
            if (!errorStream.str().empty()) {
                errorStream << "; ";
            }
            errorStream << regionId << ": " << result.errorMessage;
            notifyStateChange(regionId, RegionStateChangeEvent::ERROR_OCCURRED, result.errorMessage);
            LOG_WARN("Failed to deactivate region '{}': {}", regionId, result.errorMessage);
        }
    }

    return OrchestrationResult::partial(successful, failed, errorStream.str());
}

ParallelRegionOrchestrator::OrchestrationResult
ParallelRegionOrchestrator::activateRegions(const std::vector<std::string> &regionIds) {
    LOG_DEBUG("Activating {} specific regions for {}", regionIds.size(), parentStateId_);

    std::vector<std::string> successful;
    std::vector<std::string> failed;
    std::ostringstream errorStream;

    for (const auto &regionId : regionIds) {
        auto region = getRegion(regionId);
        if (!region) {
            failed.push_back(regionId);
            if (!errorStream.str().empty()) {
                errorStream << "; ";
            }
            errorStream << regionId << ": Region not found";
            continue;
        }

        auto result = region->activate();
        if (result.isSuccess) {
            successful.push_back(regionId);
            notifyStateChange(regionId, RegionStateChangeEvent::ACTIVATED);
        } else {
            failed.push_back(regionId);
            if (!errorStream.str().empty()) {
                errorStream << "; ";
            }
            errorStream << regionId << ": " << result.errorMessage;
            notifyStateChange(regionId, RegionStateChangeEvent::ERROR_OCCURRED, result.errorMessage);
        }
    }

    return OrchestrationResult::partial(successful, failed, errorStream.str());
}

ParallelRegionOrchestrator::OrchestrationResult
ParallelRegionOrchestrator::deactivateRegions(const std::vector<std::string> &regionIds) {
    LOG_DEBUG("Deactivating {} specific regions for {}", regionIds.size(), parentStateId_);

    std::vector<std::string> successful;
    std::vector<std::string> failed;
    std::ostringstream errorStream;

    for (const auto &regionId : regionIds) {
        auto region = getRegion(regionId);
        if (!region) {
            failed.push_back(regionId);
            if (!errorStream.str().empty()) {
                errorStream << "; ";
            }
            errorStream << regionId << ": Region not found";
            continue;
        }

        auto result = region->deactivate();
        if (result.isSuccess) {
            successful.push_back(regionId);
            notifyStateChange(regionId, RegionStateChangeEvent::DEACTIVATED);
        } else {
            failed.push_back(regionId);
            if (!errorStream.str().empty()) {
                errorStream << "; ";
            }
            errorStream << regionId << ": " << result.errorMessage;
            notifyStateChange(regionId, RegionStateChangeEvent::ERROR_OCCURRED, result.errorMessage);
        }
    }

    return OrchestrationResult::partial(successful, failed, errorStream.str());
}

ParallelRegionOrchestrator::OrchestrationResult ParallelRegionOrchestrator::restartAllRegions() {
    LOG_DEBUG("Restarting all regions for {}", parentStateId_);

    // First deactivate all regions
    auto deactivateResult = deactivateAllRegions();

    // Then activate all regions
    auto activateResult = activateAllRegions();

    // Synthesize results
    std::vector<std::string> successful;
    std::vector<std::string> failed;
    std::ostringstream errorStream;

    // Only consider activation successes as final success
    successful = activateResult.successfulRegions;
    failed = activateResult.failedRegions;

    if (!deactivateResult.isSuccess && !deactivateResult.errorMessage.empty()) {
        if (!errorStream.str().empty()) {
            errorStream << "; ";
        }
        errorStream << "Deactivation errors: " << deactivateResult.errorMessage;
    }

    if (!activateResult.isSuccess && !activateResult.errorMessage.empty()) {
        if (!errorStream.str().empty()) {
            errorStream << "; ";
        }
        errorStream << "Activation errors: " << activateResult.errorMessage;
    }

    return OrchestrationResult::partial(successful, failed, errorStream.str());
}

// State monitoring

bool ParallelRegionOrchestrator::areAllRegionsActive() const {
    if (regions_.empty()) {
        return false;
    }

    return std::all_of(regions_.begin(), regions_.end(),
                       [](const std::shared_ptr<IConcurrentRegion> &region) { return region->isActive(); });
}

bool ParallelRegionOrchestrator::areAllRegionsCompleted() const {
    if (regions_.empty()) {
        return false;
    }

    return std::all_of(regions_.begin(), regions_.end(),
                       [](const std::shared_ptr<IConcurrentRegion> &region) { return region->isInFinalState(); });
}

bool ParallelRegionOrchestrator::hasAnyRegionErrors() const {
    return std::any_of(regions_.begin(), regions_.end(), [](const std::shared_ptr<IConcurrentRegion> &region) {
        return region->getStatus() == ConcurrentRegionStatus::ERROR;
    });
}

std::unordered_map<std::string, ConcurrentRegionInfo> ParallelRegionOrchestrator::getRegionStates() const {
    std::unordered_map<std::string, ConcurrentRegionInfo> states;

    for (const auto &region : regions_) {
        states[region->getId()] = region->getInfo();
    }

    return states;
}

// Event processing

std::vector<ConcurrentOperationResult> ParallelRegionOrchestrator::broadcastEvent(const EventDescriptor &event) {
    LOG_DEBUG("Broadcasting event to {} regions for {}", regions_.size(), parentStateId_);

    std::vector<ConcurrentOperationResult> results;
    results.reserve(regions_.size());

    for (auto &region : regions_) {
        if (region->isActive()) {
            auto result = region->processEvent(event);
            results.push_back(result);

            if (!result.isSuccess) {
                notifyStateChange(region->getId(), RegionStateChangeEvent::ERROR_OCCURRED, result.errorMessage);
            }
        }
    }

    return results;
}

ConcurrentOperationResult ParallelRegionOrchestrator::sendEventToRegion(const std::string &regionId,
                                                                        const EventDescriptor &event) {
    auto region = getRegion(regionId);
    if (!region) {
        return ConcurrentOperationResult::failure(regionId, "Region not found");
    }

    if (!region->isActive()) {
        return ConcurrentOperationResult::failure(regionId, "Region is not active");
    }

    auto result = region->processEvent(event);
    if (!result.isSuccess) {
        notifyStateChange(regionId, RegionStateChangeEvent::ERROR_OCCURRED, result.errorMessage);
    }

    return result;
}

// Callback management

void ParallelRegionOrchestrator::setStateChangeCallback(RegionStateChangeCallback callback) {
    stateChangeCallback_ = std::move(callback);
}

void ParallelRegionOrchestrator::clearStateChangeCallback() {
    stateChangeCallback_ = nullptr;
}

// Validation

std::vector<std::string> ParallelRegionOrchestrator::validateOrchestrator() const {
    std::vector<std::string> errors;

    // Check for duplicate region IDs
    std::vector<std::string> regionIds = getRegionIds();
    std::sort(regionIds.begin(), regionIds.end());
    for (size_t i = 1; i < regionIds.size(); ++i) {
        if (regionIds[i] == regionIds[i - 1]) {
            errors.push_back(fmt::format("Duplicate region ID found: {}", regionIds[i]));
        }
    }

    // Validate each region
    for (const auto &region : regions_) {
        auto regionErrors = region->validate();
        for (const auto &error : regionErrors) {
            errors.push_back(fmt::format("Region '{}': {}", region->getId(), error));
        }
    }

    return errors;
}

std::string ParallelRegionOrchestrator::getStatistics() const {
    std::ostringstream stats;

    stats << "ParallelRegionOrchestrator Statistics for " << parentStateId_ << ":\n";
    stats << "  Total regions: " << regions_.size() << "\n";

    size_t activeCount = 0;
    size_t completedCount = 0;
    size_t errorCount = 0;

    for (const auto &region : regions_) {
        if (region->isActive()) {
            activeCount++;
        }
        if (region->isInFinalState()) {
            completedCount++;
        }
        if (region->getStatus() == ConcurrentRegionStatus::ERROR) {
            errorCount++;
        }
    }

    stats << "  Active regions: " << activeCount << "\n";
    stats << "  Completed regions: " << completedCount << "\n";
    stats << "  Error regions: " << errorCount << "\n";

    return stats.str();
}

// Internal helper methods

void ParallelRegionOrchestrator::notifyStateChange(const std::string &regionId, RegionStateChangeEvent event,
                                                   const std::string &details) {
    if (stateChangeCallback_) {
        stateChangeCallback_(regionId, event, details);
    }
}

bool ParallelRegionOrchestrator::isRegionIdValid(const std::string &regionId) const {
    return !regionId.empty() && regionMap_.find(regionId) != regionMap_.end();
}

std::vector<std::string> ParallelRegionOrchestrator::getRegionIds() const {
    std::vector<std::string> ids;
    ids.reserve(regions_.size());

    for (const auto &region : regions_) {
        ids.push_back(region->getId());
    }

    return ids;
}

void ParallelRegionOrchestrator::updateRegionMap() {
    regionMap_.clear();
    for (const auto &region : regions_) {
        regionMap_[region->getId()] = region;
    }
}

}  // namespace RSM