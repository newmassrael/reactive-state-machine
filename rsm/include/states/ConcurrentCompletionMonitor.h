#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace RSM {
struct EventDescriptor;
}

namespace RSM {

/**
 * @brief Simplified implementation of parallel state completion monitoring
 *
 * Tracks completion status of multiple parallel regions.
 */
class ConcurrentCompletionMonitor {
public:
    explicit ConcurrentCompletionMonitor(const std::string &parallelStateId);
    ~ConcurrentCompletionMonitor();

    // Keep only basic monitoring functionality
    bool startMonitoring();
    void stopMonitoring();
    bool isMonitoringActive() const;

    void updateRegionCompletion(const std::string &regionId, bool isComplete,
                                const std::vector<std::string> &finalStateIds = {});

    bool isCompletionCriteriaMet() const;
    std::vector<std::string> getRegisteredRegions() const;

private:
    std::string parallelStateId_;
    std::atomic<bool> isMonitoringActive_;
    mutable std::mutex regionsMutex_;
    std::unordered_map<std::string, bool> regions_;
};

}  // namespace RSM