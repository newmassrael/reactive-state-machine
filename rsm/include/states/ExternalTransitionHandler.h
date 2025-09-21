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
 * @brief 병렬 상태의 외부 전이 처리를 담당하는 클래스
 *
 * SCXML 병렬 상태에서 외부 전이가 발생할 때 모든 활성 지역을
 * 적절한 순서로 비활성화하는 역할을 담당합니다.
 */
class ExternalTransitionHandler {
public:
    /**
     * @brief 생성자
     * @param maxConcurrentTransitions 최대 동시 처리 가능한 전이 수
     */
    explicit ExternalTransitionHandler(size_t maxConcurrentTransitions = 10);

    /**
     * @brief 외부 전이 처리
     * @param parallelStateId 병렬 상태 ID
     * @param targetStateId 대상 상태 ID
     * @param transitionEvent 전이 이벤트
     * @return 처리 성공 여부
     */
    bool handleExternalTransition(const std::string &parallelStateId, const std::string &targetStateId,
                                  const std::string &transitionEvent);

    /**
     * @brief 병렬 상태 등록
     * @param parallelStateId 병렬 상태 ID
     * @param regionIds 지역 ID 목록
     */
    void registerParallelState(const std::string &parallelStateId, const std::vector<std::string> &regionIds);

    /**
     * @brief 현재 활성 전이 수 확인
     * @return 활성 전이 수
     */
    size_t getActiveTransitionCount() const;

    /**
     * @brief 전이 처리 중인지 확인
     * @return 처리 중이면 true
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

    // 내부 헬퍼 메서드들
    bool validateTransitionParameters(const std::string &parallelStateId, const std::string &targetStateId,
                                      const std::string &transitionEvent) const;

    bool deactivateAllRegions(const std::string &parallelStateId);

    bool executeRegionExitActions(const std::string &regionId, const std::string &parallelStateId);

    bool isExternalTransition(const std::string &sourceStateId, const std::string &targetStateId) const;

    bool isTargetReachable(const std::string &parallelStateId, const std::string &targetStateId) const;
};

}  // namespace RSM