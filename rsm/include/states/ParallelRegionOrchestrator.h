#pragma once

#include "events/EventDescriptor.h"
#include "states/ConcurrentStateTypes.h"
#include "states/IConcurrentRegion.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RSM {

// Forward declarations
class IStateNode;

/**
 * @brief 병렬 지역들의 생명주기를 조율하는 핵심 클래스
 *
 * 이 클래스는 SCXML 병렬 상태에서 여러 지역(region)들의 생명주기를
 * 조율하고 관리하는 역할을 담당합니다. 각 지역의 활성화, 비활성화,
 * 상태 추적 등을 통합적으로 관리합니다.
 *
 * SCXML 준수사항:
 * - 병렬 상태 진입시 모든 지역 동시 활성화
 * - 병렬 상태 탈출시 모든 지역 비활성화
 * - 각 지역의 독립적인 상태 머신 실행
 * - 지역별 오류 상황 격리 및 처리
 */
class ParallelRegionOrchestrator {
public:
    /**
     * @brief 지역 상태 변화 이벤트
     */
    enum class RegionStateChangeEvent {
        ACTIVATED,      // 지역 활성화됨
        DEACTIVATED,    // 지역 비활성화됨
        COMPLETED,      // 지역 완료됨 (final 상태 도달)
        ERROR_OCCURRED  // 지역에서 오류 발생
    };

    /**
     * @brief 지역 상태 변화 콜백 타입
     */
    using RegionStateChangeCallback =
        std::function<void(const std::string &regionId, RegionStateChangeEvent event, const std::string &details)>;

    /**
     * @brief 조율 결과 정보
     */
    struct OrchestrationResult {
        bool isSuccess = false;
        std::vector<std::string> successfulRegions;
        std::vector<std::string> failedRegions;
        std::string errorMessage;

        static OrchestrationResult success(const std::vector<std::string> &regions);
        static OrchestrationResult failure(const std::string &error);
        static OrchestrationResult partial(const std::vector<std::string> &successful,
                                           const std::vector<std::string> &failed, const std::string &error);
    };

    /**
     * @brief 병렬 지역 조율기 생성
     * @param parentStateId 부모 병렬 상태의 ID
     */
    explicit ParallelRegionOrchestrator(const std::string &parentStateId);

    /**
     * @brief 소멸자
     */
    ~ParallelRegionOrchestrator();

    // 지역 관리

    /**
     * @brief 병렬 지역 추가
     * @param region 추가할 지역
     * @return 작업 결과
     */
    ConcurrentOperationResult addRegion(std::shared_ptr<IConcurrentRegion> region);

    /**
     * @brief 지역 제거
     * @param regionId 제거할 지역 ID
     * @return 작업 결과
     */
    ConcurrentOperationResult removeRegion(const std::string &regionId);

    /**
     * @brief 특정 지역 찾기
     * @param regionId 찾을 지역 ID
     * @return 지역 포인터 (없으면 nullptr)
     */
    std::shared_ptr<IConcurrentRegion> getRegion(const std::string &regionId) const;

    /**
     * @brief 모든 지역 목록 가져오기
     * @return 지역 목록
     */
    const std::vector<std::shared_ptr<IConcurrentRegion>> &getAllRegions() const;

    /**
     * @brief 활성화된 지역만 가져오기
     * @return 활성화된 지역 목록
     */
    std::vector<std::shared_ptr<IConcurrentRegion>> getActiveRegions() const;

    // 생명주기 조율

    /**
     * @brief 모든 지역 활성화 (병렬 상태 진입시)
     * @return 조율 결과
     */
    OrchestrationResult activateAllRegions();

    /**
     * @brief 모든 지역 비활성화 (병렬 상태 탈출시)
     * @return 조율 결과
     */
    OrchestrationResult deactivateAllRegions();

    /**
     * @brief 특정 지역들만 활성화
     * @param regionIds 활성화할 지역 ID 목록
     * @return 조율 결과
     */
    OrchestrationResult activateRegions(const std::vector<std::string> &regionIds);

    /**
     * @brief 특정 지역들만 비활성화
     * @param regionIds 비활성화할 지역 ID 목록
     * @return 조율 결과
     */
    OrchestrationResult deactivateRegions(const std::vector<std::string> &regionIds);

    /**
     * @brief 모든 지역 재시작 (재초기화)
     * @return 조율 결과
     */
    OrchestrationResult restartAllRegions();

    // 상태 모니터링

    /**
     * @brief 모든 지역이 활성화되었는지 확인
     * @return true면 모든 지역 활성화됨
     */
    bool areAllRegionsActive() const;

    /**
     * @brief 모든 지역이 완료 상태인지 확인
     * @return true면 모든 지역 완료됨
     */
    bool areAllRegionsCompleted() const;

    /**
     * @brief 하나 이상의 지역에서 오류가 발생했는지 확인
     * @return true면 오류 발생한 지역 존재
     */
    bool hasAnyRegionErrors() const;

    /**
     * @brief 지역별 상태 정보 가져오기
     * @return 지역별 상태 정보 맵
     */
    std::unordered_map<std::string, ConcurrentRegionInfo> getRegionStates() const;

    // 이벤트 처리

    /**
     * @brief 모든 활성 지역에 이벤트 전파
     * @param event 전파할 이벤트
     * @return 지역별 처리 결과
     */
    std::vector<ConcurrentOperationResult> broadcastEvent(const EventDescriptor &event);

    /**
     * @brief 특정 지역에만 이벤트 전송
     * @param regionId 대상 지역 ID
     * @param event 전송할 이벤트
     * @return 처리 결과
     */
    ConcurrentOperationResult sendEventToRegion(const std::string &regionId, const EventDescriptor &event);

    // 콜백 관리

    /**
     * @brief 지역 상태 변화 콜백 등록
     * @param callback 콜백 함수
     */
    void setStateChangeCallback(RegionStateChangeCallback callback);

    /**
     * @brief 지역 상태 변화 콜백 제거
     */
    void clearStateChangeCallback();

    // 검증

    /**
     * @brief 조율기 상태 검증
     * @return 검증 오류 목록 (비어있으면 정상)
     */
    std::vector<std::string> validateOrchestrator() const;

    /**
     * @brief 통계 정보 가져오기
     * @return 통계 정보 문자열
     */
    std::string getStatistics() const;

private:
    std::string parentStateId_;
    std::vector<std::shared_ptr<IConcurrentRegion>> regions_;
    std::unordered_map<std::string, std::shared_ptr<IConcurrentRegion>> regionMap_;
    RegionStateChangeCallback stateChangeCallback_;

    // 내부 헬퍼 메서드
    void notifyStateChange(const std::string &regionId, RegionStateChangeEvent event, const std::string &details = "");

    bool isRegionIdValid(const std::string &regionId) const;
    std::vector<std::string> getRegionIds() const;
    void updateRegionMap();
};

}  // namespace RSM