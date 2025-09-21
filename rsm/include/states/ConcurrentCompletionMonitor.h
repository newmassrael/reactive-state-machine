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
 * @brief 병렬 상태 완료 모니터링의 간소화된 구현체
 *
 * 여러 병렬 지역의 완료 상태를 추적합니다.
 */
class ConcurrentCompletionMonitor {
public:
    explicit ConcurrentCompletionMonitor(const std::string &parallelStateId);
    ~ConcurrentCompletionMonitor();

    // 기본 모니터링 기능만 유지
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