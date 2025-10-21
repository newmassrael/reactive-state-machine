#include "states/ConcurrentCompletionMonitor.h"

namespace RSM {

ConcurrentCompletionMonitor::ConcurrentCompletionMonitor(const std::string &parallelStateId)
    : parallelStateId_(parallelStateId), isMonitoringActive_(false) {}

ConcurrentCompletionMonitor::~ConcurrentCompletionMonitor() {
    stopMonitoring();
}

bool ConcurrentCompletionMonitor::startMonitoring() {
    isMonitoringActive_.store(true);
    return true;
}

void ConcurrentCompletionMonitor::stopMonitoring() {
    isMonitoringActive_.store(false);
}

bool ConcurrentCompletionMonitor::isMonitoringActive() const {
    return isMonitoringActive_.load();
}

void ConcurrentCompletionMonitor::updateRegionCompletion(const std::string &regionId, bool isComplete,
                                                         const std::vector<std::string> & /* finalStateIds */) {
    if (!isMonitoringActive_.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(regionsMutex_);
    regions_[regionId] = isComplete;
}

bool ConcurrentCompletionMonitor::isCompletionCriteriaMet() const {
    if (!isMonitoringActive_.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(regionsMutex_);

    if (regions_.empty()) {
        return false;
    }

    // Check if all regions are complete
    for (const auto &[regionId, isComplete] : regions_) {
        if (!isComplete) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> ConcurrentCompletionMonitor::getRegisteredRegions() const {
    std::lock_guard<std::mutex> lock(regionsMutex_);

    std::vector<std::string> regionIds;
    regionIds.reserve(regions_.size());

    for (const auto &[regionId, isComplete] : regions_) {
        regionIds.push_back(regionId);
    }

    return regionIds;
}

}  // namespace RSM