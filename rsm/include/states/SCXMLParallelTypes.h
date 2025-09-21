#pragma once

#include "events/EventDescriptor.h"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RSM {

// Forward declarations
class IStateNode;
class IConcurrentRegion;

/**
 * @brief SCXML 병렬 상태 완료 기준
 * SCXML 사양: 병렬 상태는 모든 자식 상태가 최종 상태에 도달할 때 완료됨
 */
enum class ParallelCompletionCriteria {
    ALL_REGIONS_FINAL,      // 모든 지역이 최종 상태 (SCXML 표준)
    ANY_REGION_FINAL,       // 하나라도 최종 상태 (확장)
    MAJORITY_REGIONS_FINAL  // 과반수 지역이 최종 상태 (확장)
};

/**
 * @brief 지역 완료 정보
 * SCXML 사양에 따른 지역별 상태 추적
 */
struct RegionCompletionInfo {
    std::string regionId;
    bool isInFinalState = false;
    std::vector<std::string> finalStateIds;  // 지역 내 최종 상태들
    std::chrono::steady_clock::time_point completionTime;
    std::chrono::steady_clock::time_point lastUpdateTime;

    // SCXML 추가 정보
    std::string currentStateId;               // 현재 활성 상태
    std::vector<std::string> activeStateIds;  // 모든 활성 상태 (복합 상태용)
};

/**
 * @brief 병렬 상태 완료 정보
 * SCXML done.state 이벤트 생성을 위한 종합 정보
 */
struct ParallelStateCompletionInfo {
    std::string parallelStateId;
    bool isComplete = false;
    ParallelCompletionCriteria completionCriteria;
    size_t totalRegions = 0;
    size_t completedRegions = 0;
    std::vector<RegionCompletionInfo> regionCompletions;
    std::chrono::steady_clock::time_point completionTime;

    // SCXML done.state 이벤트 데이터
    std::string doneEventName;                              // "done.state.{id}"
    std::unordered_map<std::string, std::string> doneData;  // done 데이터
};

/**
 * @brief 완료 이벤트 타입
 * SCXML 사양에 따른 이벤트 분류
 */
enum class CompletionEventType {
    PARALLEL_STATE_COMPLETED,  // 병렬 상태 완료 (done.state)
    REGION_COMPLETED,          // 개별 지역 완료
    COMPLETION_ERROR           // 완료 처리 오류
};

/**
 * @brief 완료 이벤트
 * SCXML done.state 이벤트 표현
 */
struct CompletionEvent {
    CompletionEventType type;
    std::string parallelStateId;
    std::vector<std::string> completedRegions;
    std::chrono::steady_clock::time_point timestamp;
    std::string errorMessage;  // 오류 시에만 사용

    // SCXML done.state 이벤트 생성
    EventDescriptor toDoneStateEvent() const;
};

/**
 * @brief 병렬 상태 모니터링 설정
 * SCXML 사양과 확장 기능을 위한 설정
 */
struct ParallelMonitoringConfig {
    ParallelCompletionCriteria criteria = ParallelCompletionCriteria::ALL_REGIONS_FINAL;

    // SCXML 타이밍 설정
    bool generateDoneEvents = true;        // done.state 이벤트 생성 여부
    bool validateStateConsistency = true;  // 상태 일관성 검증

    // 성능 및 디버깅
    bool collectDetailedStatistics = false;             // 상세 통계 수집
    std::chrono::milliseconds monitoringInterval{100};  // 모니터링 주기

    // 확장 기능
    std::unordered_map<std::string, double> regionWeights;  // 지역별 가중치
    double weightedThreshold = 0.8;                         // 가중치 기반 완료 임계값
};

/**
 * @brief 모니터링 통계
 * SCXML 성능 분석을 위한 통계 정보
 */
struct MonitoringStatistics {
    size_t totalRegionsRegistered = 0;
    size_t totalCompletionEvents = 0;
    size_t totalStatusQueries = 0;
    std::chrono::microseconds averageCompletionCheckTime{0};
    bool isCurrentlyComplete = false;

    // SCXML 사양 준수 통계
    size_t doneEventsGenerated = 0;
    size_t stateConsistencyViolations = 0;
    std::chrono::steady_clock::time_point monitoringStartTime;
};

}  // namespace RSM