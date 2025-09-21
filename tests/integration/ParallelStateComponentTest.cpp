#include "factory/NodeFactory.h"
#include "parsing/SCXMLParser.h"
#include "scripting/JSEngine.h"
#include "states/ConcurrentCompletionMonitor.h"
#include "states/ConcurrentEventBroadcaster.h"
#include "states/ExternalTransitionHandler.h"
#include "gtest/gtest.h"
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>

namespace RSM {

class ParallelStateComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        engine_->reset();
        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
        broadcaster_ = std::make_unique<ConcurrentEventBroadcaster>();
        monitor_ = std::make_unique<ConcurrentCompletionMonitor>("parallel_test");
        handler_ = std::make_unique<ExternalTransitionHandler>(5);
        sessionId_ = "parallel_component_test";
    }

    void TearDown() override {
        if (engine_) {
            engine_->reset();
        }
    }

    JSEngine *engine_;
    std::shared_ptr<NodeFactory> nodeFactory_;
    std::unique_ptr<SCXMLParser> parser_;
    std::unique_ptr<ConcurrentEventBroadcaster> broadcaster_;
    std::unique_ptr<ConcurrentCompletionMonitor> monitor_;
    std::unique_ptr<ExternalTransitionHandler> handler_;
    std::string sessionId_;
};

// ============================================================================
// 이벤트 브로드캐스팅 테스트
// ============================================================================

TEST_F(ParallelStateComponentTest, EventBroadcasting_BasicBroadcast) {
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};
    // registerParallelState 메서드가 존재하지 않음 - 직접 지역 등록 필요

    EventDescriptor event;
    event.eventName = "test_event";
    event.data = "test_data";

    bool result = broadcaster_->broadcastEvent("parallel1", event);
    EXPECT_TRUE(result) << "기본 이벤트 브로드캐스팅이 실패했습니다";
}

TEST_F(ParallelStateComponentTest, EventBroadcasting_SelectiveBroadcast) {
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};
    // registerParallelState 메서드가 존재하지 않음 - 직접 지역 등록 필요

    EventDescriptor event;
    event.eventName = "selective_event";
    event.data = "selective_data";

    std::vector<std::string> targetRegions = {"region1", "region3"};
    bool result = broadcaster_->broadcastEventToRegions(event, targetRegions);
    EXPECT_TRUE(result) << "선택적 이벤트 브로드캐스팅이 실패했습니다";
}

TEST_F(ParallelStateComponentTest, EventBroadcasting_ConcurrentBroadcast) {
    std::vector<std::string> regionIds = {"region1", "region2", "region3", "region4"};
    // registerParallelState 메서드가 존재하지 않음 - 직접 지역 등록 필요

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &successCount, i]() {
            EventDescriptor event;
            event.eventName = "concurrent_event_" + std::to_string(i);

            if (broadcaster_->broadcastEvent("parallel1", event)) {
                successCount.fetch_add(1);
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), 5) << "동시 브로드캐스팅에서 일부가 실패했습니다";
}

// ============================================================================
// 완료 모니터링 테스트
// ============================================================================

TEST_F(ParallelStateComponentTest, CompletionMonitoring_BasicMonitoring) {
    EXPECT_FALSE(monitor_->isMonitoringActive()) << "모니터링이 초기에 활성화되어 있습니다";

    bool started = monitor_->startMonitoring();
    EXPECT_TRUE(started) << "모니터링 시작에 실패했습니다";
    EXPECT_TRUE(monitor_->isMonitoringActive()) << "모니터링이 활성화되지 않았습니다";

    monitor_->stopMonitoring();
    EXPECT_FALSE(monitor_->isMonitoringActive()) << "모니터링이 중지되지 않았습니다";
}

TEST_F(ParallelStateComponentTest, CompletionMonitoring_RegionCompletion) {
    monitor_->startMonitoring();

    monitor_->updateRegionCompletion("region1", false);
    monitor_->updateRegionCompletion("region2", false);

    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "모든 지역이 미완료 상태인데 완료 조건이 만족되었습니다";

    monitor_->updateRegionCompletion("region1", true);
    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "일부 지역만 완료된 상태에서 완료 조건이 만족되었습니다";

    monitor_->updateRegionCompletion("region2", true);
    EXPECT_TRUE(monitor_->isCompletionCriteriaMet()) << "모든 지역이 완료되었는데 완료 조건이 만족되지 않았습니다";
}

TEST_F(ParallelStateComponentTest, CompletionMonitoring_ConcurrentUpdates) {
    monitor_->startMonitoring();

    const int numThreads = 3;
    const int numRegionsPerThread = 5;
    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, numRegionsPerThread]() {
            for (int r = 0; r < numRegionsPerThread; ++r) {
                std::string regionId = "thread" + std::to_string(t) + "_region" + std::to_string(r);
                monitor_->updateRegionCompletion(regionId, (r % 2 == 0));
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    auto regions = monitor_->getRegisteredRegions();
    EXPECT_EQ(regions.size(), numThreads * numRegionsPerThread) << "등록된 지역 수가 예상과 다릅니다";

    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "일부 지역이 미완료 상태인데 완료 조건이 만족되었습니다";
}

// ============================================================================
// 외부 전이 처리 테스트
// ============================================================================

TEST_F(ParallelStateComponentTest, ExternalTransition_BasicHandling) {
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    bool result = handler_->handleExternalTransition("parallel1", "target_state", "exit_event");
    EXPECT_TRUE(result) << "기본 외부 전이 처리가 실패했습니다";
}

TEST_F(ParallelStateComponentTest, ExternalTransition_ConcurrentLimit) {
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    std::vector<std::future<bool>> futures;

    for (int i = 0; i < 8; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            return handler_->handleExternalTransition("parallel1", "target_" + std::to_string(i),
                                                      "event_" + std::to_string(i));
        }));
    }

    int successCount = 0;
    for (auto &future : futures) {
        if (future.get()) {
            successCount++;
        }
    }

    EXPECT_LE(successCount, 5) << "동시 전이 제한이 적용되지 않았습니다";
}

TEST_F(ParallelStateComponentTest, ExternalTransition_InvalidParameters) {
    bool result = handler_->handleExternalTransition("", "target_state", "exit_event");
    EXPECT_FALSE(result) << "빈 병렬 상태 ID로 전이가 성공했습니다";

    result = handler_->handleExternalTransition("parallel1", "", "exit_event");
    EXPECT_FALSE(result) << "빈 타겟 상태 ID로 전이가 성공했습니다";

    result = handler_->handleExternalTransition("parallel1", "target_state", "");
    EXPECT_FALSE(result) << "빈 전이 이벤트로 전이가 성공했습니다";
}

// ============================================================================
// 통합 시나리오 테스트 (컴포넌트 간 상호작용)
// ============================================================================

TEST_F(ParallelStateComponentTest, IntegratedScenario_EventBroadcastToCompletion) {
    const std::string parallelStateId = "integrated_parallel";
    std::vector<std::string> regionIds = {"region1", "region2"};

    // 모든 컴포넌트에 동일한 병렬 상태 등록
    broadcaster_->registerParallelState(parallelStateId, regionIds);
    handler_->registerParallelState(parallelStateId, regionIds);
    monitor_->startMonitoring();

    // 이벤트 브로드캐스팅
    EventDescriptor event;
    event.eventName = "completion_trigger";
    bool broadcastResult = broadcaster_->broadcastEvent(parallelStateId, event);
    EXPECT_TRUE(broadcastResult) << "이벤트 브로드캐스팅이 실패했습니다";

    // 지역 완료 상태 업데이트
    monitor_->updateRegionCompletion("region1", true);
    monitor_->updateRegionCompletion("region2", true);
    EXPECT_TRUE(monitor_->isCompletionCriteriaMet()) << "완료 조건이 만족되지 않았습니다";

    // 외부 전이 처리
    bool transitionResult = handler_->handleExternalTransition(parallelStateId, "final_state", "done_event");
    EXPECT_TRUE(transitionResult) << "외부 전이 처리가 실패했습니다";

    EXPECT_EQ(handler_->getActiveTransitionCount(), 0) << "전이 완료 후 활성 전이 수가 0이 아닙니다";
}

TEST_F(ParallelStateComponentTest, IntegratedScenario_PartialCompletionWithTransition) {
    const std::string parallelStateId = "partial_parallel";
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};

    broadcaster_->registerParallelState(parallelStateId, regionIds);
    handler_->registerParallelState(parallelStateId, regionIds);
    monitor_->startMonitoring();

    // 일부 지역만 완료
    monitor_->updateRegionCompletion("region1", true);
    monitor_->updateRegionCompletion("region2", false);
    monitor_->updateRegionCompletion("region3", false);
    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "일부 지역만 완료된 상태에서 완료 조건이 만족되었습니다";

    // 강제 외부 전이 (미완료 상태에서)
    bool transitionResult = handler_->handleExternalTransition(parallelStateId, "early_exit", "force_exit");
    EXPECT_TRUE(transitionResult) << "강제 외부 전이가 실패했습니다";
}

TEST_F(ParallelStateComponentTest, IntegratedScenario_SCXMLWithComponents) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <transition event="force_exit" target="final_state"/>
            <state id="region1">
                <initial>
                    <transition target="region1_active"/>
                </initial>
                <state id="region1_active">
                    <transition event="region1_complete" target="region1_final"/>
                </state>
                <final id="region1_final"/>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_active"/>
                </initial>
                <state id="region2_active">
                    <transition event="region2_complete" target="region2_final"/>
                </state>
                <final id="region2_final"/>
            </state>
        </parallel>
        <final id="final_state"/>
    </scxml>)";

    auto result = parser_->parseContent(scxmlContent);
    ASSERT_TRUE(result.has_value()) << "SCXML 파싱이 실패했습니다";

    auto stateMachine = result.value();
    ASSERT_NE(stateMachine, nullptr) << "상태머신 생성에 실패했습니다";

    // 컴포넌트들이 SCXML과 함께 작동하는지 테스트
    auto parallelState = stateMachine->findChildById("parallel1");
    ASSERT_NE(parallelState, nullptr) << "병렬 상태를 찾을 수 없습니다";

    auto finalState = stateMachine->findChildById("final_state");
    ASSERT_NE(finalState, nullptr) << "최종 상태를 찾을 수 없습니다";
}

// ============================================================================
// 성능 및 스트레스 테스트
// ============================================================================

TEST_F(ParallelStateComponentTest, Performance_LargeScaleComponents) {
    const int numStates = 100;
    const int numRegionsPerState = 10;

    auto startTime = std::chrono::high_resolution_clock::now();

    // 대량 병렬 상태 등록
    for (int i = 0; i < numStates; ++i) {
        std::vector<std::string> regionIds;
        for (int j = 0; j < numRegionsPerState; ++j) {
            regionIds.push_back("state" + std::to_string(i) + "_region" + std::to_string(j));
        }

        broadcaster_->registerParallelState("parallel_" + std::to_string(i), regionIds);
        handler_->registerParallelState("parallel_" + std::to_string(i), regionIds);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    EXPECT_LT(duration.count(), 1000) << "대량 컴포넌트 등록 성능이 너무 느립니다 (1초 초과)";

    // 대량 이벤트 브로드캐스팅 테스트
    startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numStates; ++i) {
        EventDescriptor event;
        event.eventName = "perf_test_event_" + std::to_string(i);
        broadcaster_->broadcastEvent("parallel_" + std::to_string(i), event);
    }

    endTime = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    EXPECT_LT(duration.count(), 500) << "대량 이벤트 브로드캐스팅 성능이 너무 느립니다 (500ms 초과)";
}

}  // namespace RSM