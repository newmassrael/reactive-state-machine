#include "states/ParallelRegionOrchestrator.h"
#include "common/Logger.h"
#include "events/EventDescriptor.h"
#include <algorithm>
#include <sstream>

namespace RSM {

// OrchestrationResult 정적 팩토리 메서드들

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
    result.isSuccess = failed.empty();  // 실패한 것이 없으면 성공
    result.successfulRegions = successful;
    result.failedRegions = failed;
    result.errorMessage = error;
    return result;
}

// ParallelRegionOrchestrator 구현

ParallelRegionOrchestrator::ParallelRegionOrchestrator(const std::string &parentStateId)
    : parentStateId_(parentStateId) {
    Logger::debug("ParallelRegionOrchestrator::Constructor - Creating orchestrator for state: " + parentStateId_);
}

ParallelRegionOrchestrator::~ParallelRegionOrchestrator() {
    Logger::debug("ParallelRegionOrchestrator::Destructor - Destroying orchestrator for state: " + parentStateId_);

    // 안전한 종료: 모든 지역 비활성화
    if (!regions_.empty()) {
        deactivateAllRegions();
    }
}

// 지역 관리

ConcurrentOperationResult ParallelRegionOrchestrator::addRegion(std::shared_ptr<IConcurrentRegion> region) {
    if (!region) {
        return ConcurrentOperationResult::failure("", "Cannot add null region");
    }

    const std::string &regionId = region->getId();

    // 중복 검사
    if (regionMap_.find(regionId) != regionMap_.end()) {
        return ConcurrentOperationResult::failure(regionId, "Region with ID '" + regionId + "' already exists");
    }

    // 지역 추가
    regions_.push_back(region);
    regionMap_[regionId] = region;

    Logger::debug("ParallelRegionOrchestrator::addRegion() - Added region '" + regionId + "' to orchestrator for " +
                  parentStateId_);

    // 상태 변화 알림
    notifyStateChange(regionId, RegionStateChangeEvent::ACTIVATED, "Region added to orchestrator");

    return ConcurrentOperationResult::success(regionId);
}

ConcurrentOperationResult ParallelRegionOrchestrator::removeRegion(const std::string &regionId) {
    auto mapIt = regionMap_.find(regionId);
    if (mapIt == regionMap_.end()) {
        return ConcurrentOperationResult::failure(regionId, "Region with ID '" + regionId + "' not found");
    }

    // 지역이 활성화되어 있다면 먼저 비활성화
    auto region = mapIt->second;
    if (region->isActive()) {
        auto deactivateResult = region->deactivate();
        if (!deactivateResult.isSuccess) {
            Logger::warn("ParallelRegionOrchestrator::removeRegion() - Failed to deactivate region '" + regionId +
                         "': " + deactivateResult.errorMessage);
        }
    }

    // 벡터에서 제거
    auto vectorIt =
        std::find_if(regions_.begin(), regions_.end(),
                     [&regionId](const std::shared_ptr<IConcurrentRegion> &r) { return r->getId() == regionId; });

    if (vectorIt != regions_.end()) {
        regions_.erase(vectorIt);
    }

    // 맵에서 제거
    regionMap_.erase(mapIt);

    Logger::debug("ParallelRegionOrchestrator::removeRegion() - Removed region '" + regionId +
                  "' from orchestrator for " + parentStateId_);

    // 상태 변화 알림
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

// 생명주기 조율

ParallelRegionOrchestrator::OrchestrationResult ParallelRegionOrchestrator::activateAllRegions() {
    Logger::debug("ParallelRegionOrchestrator::activateAllRegions() - Activating " + std::to_string(regions_.size()) +
                  " regions for " + parentStateId_);

    std::vector<std::string> successful;
    std::vector<std::string> failed;
    std::ostringstream errorStream;

    for (auto &region : regions_) {
        const std::string &regionId = region->getId();
        auto result = region->activate();

        if (result.isSuccess) {
            successful.push_back(regionId);
            notifyStateChange(regionId, RegionStateChangeEvent::ACTIVATED);
            Logger::debug("ParallelRegionOrchestrator::activateAllRegions() - Successfully activated region: " +
                          regionId);
        } else {
            failed.push_back(regionId);
            if (!errorStream.str().empty()) {
                errorStream << "; ";
            }
            errorStream << regionId << ": " << result.errorMessage;
            notifyStateChange(regionId, RegionStateChangeEvent::ERROR_OCCURRED, result.errorMessage);
            Logger::warn("ParallelRegionOrchestrator::activateAllRegions() - Failed to activate region '" + regionId +
                         "': " + result.errorMessage);
        }
    }

    return OrchestrationResult::partial(successful, failed, errorStream.str());
}

ParallelRegionOrchestrator::OrchestrationResult ParallelRegionOrchestrator::deactivateAllRegions() {
    Logger::debug("ParallelRegionOrchestrator::deactivateAllRegions() - Deactivating " +
                  std::to_string(regions_.size()) + " regions for " + parentStateId_);

    std::vector<std::string> successful;
    std::vector<std::string> failed;
    std::ostringstream errorStream;

    for (auto &region : regions_) {
        const std::string &regionId = region->getId();
        auto result = region->deactivate();

        if (result.isSuccess) {
            successful.push_back(regionId);
            notifyStateChange(regionId, RegionStateChangeEvent::DEACTIVATED);
            Logger::debug("ParallelRegionOrchestrator::deactivateAllRegions() - Successfully deactivated region: " +
                          regionId);
        } else {
            failed.push_back(regionId);
            if (!errorStream.str().empty()) {
                errorStream << "; ";
            }
            errorStream << regionId << ": " << result.errorMessage;
            notifyStateChange(regionId, RegionStateChangeEvent::ERROR_OCCURRED, result.errorMessage);
            Logger::warn("ParallelRegionOrchestrator::deactivateAllRegions() - Failed to deactivate region '" +
                         regionId + "': " + result.errorMessage);
        }
    }

    return OrchestrationResult::partial(successful, failed, errorStream.str());
}

ParallelRegionOrchestrator::OrchestrationResult
ParallelRegionOrchestrator::activateRegions(const std::vector<std::string> &regionIds) {
    Logger::debug("ParallelRegionOrchestrator::activateRegions() - Activating " + std::to_string(regionIds.size()) +
                  " specific regions for " + parentStateId_);

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
    Logger::debug("ParallelRegionOrchestrator::deactivateRegions() - Deactivating " + std::to_string(regionIds.size()) +
                  " specific regions for " + parentStateId_);

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
    Logger::debug("ParallelRegionOrchestrator::restartAllRegions() - Restarting all regions for " + parentStateId_);

    // 먼저 모든 지역 비활성화
    auto deactivateResult = deactivateAllRegions();

    // 그다음 모든 지역 활성화
    auto activateResult = activateAllRegions();

    // 결과 합성
    std::vector<std::string> successful;
    std::vector<std::string> failed;
    std::ostringstream errorStream;

    // 활성화 성공한 것들만 최종 성공으로 간주
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

// 상태 모니터링

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

// 이벤트 처리

std::vector<ConcurrentOperationResult> ParallelRegionOrchestrator::broadcastEvent(const EventDescriptor &event) {
    Logger::debug("ParallelRegionOrchestrator::broadcastEvent() - Broadcasting event to " +
                  std::to_string(regions_.size()) + " regions for " + parentStateId_);

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

// 콜백 관리

void ParallelRegionOrchestrator::setStateChangeCallback(RegionStateChangeCallback callback) {
    stateChangeCallback_ = std::move(callback);
}

void ParallelRegionOrchestrator::clearStateChangeCallback() {
    stateChangeCallback_ = nullptr;
}

// 검증

std::vector<std::string> ParallelRegionOrchestrator::validateOrchestrator() const {
    std::vector<std::string> errors;

    // 지역 ID 중복 검사
    std::vector<std::string> regionIds = getRegionIds();
    std::sort(regionIds.begin(), regionIds.end());
    for (size_t i = 1; i < regionIds.size(); ++i) {
        if (regionIds[i] == regionIds[i - 1]) {
            errors.push_back("Duplicate region ID found: " + regionIds[i]);
        }
    }

    // 각 지역의 검증
    for (const auto &region : regions_) {
        auto regionErrors = region->validate();
        for (const auto &error : regionErrors) {
            errors.push_back("Region '" + region->getId() + "': " + error);
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

// 내부 헬퍼 메서드들

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