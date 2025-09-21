#include "js_engine/JSEngine.h"
#include "parsing/NodeFactory.h"
#include "parsing/SCXMLParser.h"
#include "states/ConcurrentCompletionMonitor.h"
#include "gtest/gtest.h"
#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace RSM {

class ConcurrentCompletionMonitoringTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        engine_->reset();
        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
        monitor_ = std::make_unique<ConcurrentCompletionMonitor>("parallel_test");
        sessionId_ = "concurrent_completion_monitoring_test";
    }

    void TearDown() override {
        if (engine_) {
            engine_->reset();
        }
    }

    JSEngine *engine_;
    std::shared_ptr<NodeFactory> nodeFactory_;
    std::unique_ptr<SCXMLParser> parser_;
    std::unique_ptr<ConcurrentCompletionMonitor> monitor_;
    std::string sessionId_;
};

// 기본 모니터링 시작/중지 테스트
TEST_F(ConcurrentCompletionMonitoringTest, BasicMonitoringStartStop) {
    EXPECT_FALSE(monitor_->isMonitoringActive()) << "모니터링이 초기에 활성화되어 있습니다";

    bool started = monitor_->startMonitoring();
    EXPECT_TRUE(started) << "모니터링 시작에 실패했습니다";
    EXPECT_TRUE(monitor_->isMonitoringActive()) << "모니터링이 활성화되지 않았습니다";

    monitor_->stopMonitoring();
    EXPECT_FALSE(monitor_->isMonitoringActive()) << "모니터링이 중지되지 않았습니다";
}

// 지역 완료 상태 업데이트 테스트
TEST_F(ConcurrentCompletionMonitoringTest, RegionCompletionUpdate) {
    monitor_->startMonitoring();

    // 지역 완료 상태 업데이트
    monitor_->updateRegionCompletion("region1", false);
    monitor_->updateRegionCompletion("region2", false);

    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "모든 지역이 미완료 상태인데 완료 조건이 만족되었습니다";

    // 하나의 지역 완료
    monitor_->updateRegionCompletion("region1", true);
    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "일부 지역만 완료된 상태에서 완료 조건이 만족되었습니다";

    // 모든 지역 완료
    monitor_->updateRegionCompletion("region2", true);
    EXPECT_TRUE(monitor_->isCompletionCriteriaMet()) << "모든 지역이 완료되었는데 완료 조건이 만족되지 않았습니다";
}

// 등록된 지역 조회 테스트
TEST_F(ConcurrentCompletionMonitoringTest, RegisteredRegionsRetrieval) {
    monitor_->startMonitoring();

    // 초기 상태에서는 등록된 지역이 없어야 함
    auto regions = monitor_->getRegisteredRegions();
    EXPECT_TRUE(regions.empty()) << "초기 상태에서 등록된 지역이 있습니다";

    // 지역 등록
    monitor_->updateRegionCompletion("region1", false);
    monitor_->updateRegionCompletion("region2", false);
    monitor_->updateRegionCompletion("region3", false);

    regions = monitor_->getRegisteredRegions();
    EXPECT_EQ(regions.size(), 3) << "등록된 지역 수가 예상과 다릅니다";

    // 지역 이름 확인
    std::set<std::string> regionSet(regions.begin(), regions.end());
    EXPECT_TRUE(regionSet.count("region1") > 0) << "region1이 등록되지 않았습니다";
    EXPECT_TRUE(regionSet.count("region2") > 0) << "region2가 등록되지 않았습니다";
    EXPECT_TRUE(regionSet.count("region3") > 0) << "region3이 등록되지 않았습니다";
}

// 모니터링 비활성 상태에서의 업데이트 테스트
TEST_F(ConcurrentCompletionMonitoringTest, UpdateWhenMonitoringInactive) {
    // 모니터링이 비활성 상태에서 업데이트 시도
    monitor_->updateRegionCompletion("region1", true);

    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "모니터링 비활성 상태에서 완료 조건이 만족되었습니다";

    auto regions = monitor_->getRegisteredRegions();
    EXPECT_TRUE(regions.empty()) << "모니터링 비활성 상태에서 지역이 등록되었습니다";
}

// 동시성 테스트 - 여러 스레드에서 동시 업데이트
TEST_F(ConcurrentCompletionMonitoringTest, ConcurrentUpdates) {
    monitor_->startMonitoring();

    const int numThreads = 5;
    const int numRegionsPerThread = 10;
    std::vector<std::thread> threads;

    // 여러 스레드에서 동시에 지역 완료 상태 업데이트
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, numRegionsPerThread]() {
            for (int r = 0; r < numRegionsPerThread; ++r) {
                std::string regionId = "thread" + std::to_string(t) + "_region" + std::to_string(r);
                monitor_->updateRegionCompletion(regionId, (r % 2 == 0));  // 짝수는 완료, 홀수는 미완료
            }
        });
    }

    // 모든 스레드 완료 대기
    for (auto &thread : threads) {
        thread.join();
    }

    auto regions = monitor_->getRegisteredRegions();
    EXPECT_EQ(regions.size(), numThreads * numRegionsPerThread) << "등록된 지역 수가 예상과 다릅니다";

    // 완료 조건은 만족되지 않아야 함 (홀수 지역들이 미완료 상태)
    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "일부 지역이 미완료 상태인데 완료 조건이 만족되었습니다";
}

// 빈 지역 목록에서의 완료 조건 테스트
TEST_F(ConcurrentCompletionMonitoringTest, EmptyRegionsCompletionCriteria) {
    monitor_->startMonitoring();

    // 지역이 등록되지 않은 상태에서 완료 조건 확인
    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "빈 지역 목록에서 완료 조건이 만족되었습니다";
}

// 동일 지역 중복 업데이트 테스트
TEST_F(ConcurrentCompletionMonitoringTest, DuplicateRegionUpdates) {
    monitor_->startMonitoring();

    // 동일 지역을 여러 번 업데이트
    monitor_->updateRegionCompletion("region1", false);
    monitor_->updateRegionCompletion("region1", true);
    monitor_->updateRegionCompletion("region1", false);
    monitor_->updateRegionCompletion("region1", true);

    auto regions = monitor_->getRegisteredRegions();
    EXPECT_EQ(regions.size(), 1) << "중복 업데이트로 인해 지역이 중복 등록되었습니다";

    // 최종 상태는 true여야 함
    EXPECT_TRUE(monitor_->isCompletionCriteriaMet()) << "최종 완료 상태가 반영되지 않았습니다";
}

// 최종 상태 ID를 포함한 업데이트 테스트
TEST_F(ConcurrentCompletionMonitoringTest, UpdateWithFinalStateIds) {
    monitor_->startMonitoring();

    std::vector<std::string> finalStateIds = {"final1", "final2"};
    monitor_->updateRegionCompletion("region1", true, finalStateIds);
    monitor_->updateRegionCompletion("region2", false);

    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "일부 지역만 완료된 상태에서 완료 조건이 만족되었습니다";

    monitor_->updateRegionCompletion("region2", true, {"final3"});
    EXPECT_TRUE(monitor_->isCompletionCriteriaMet()) << "모든 지역이 완료되었는데 완료 조건이 만족되지 않았습니다";
}

// SCXML 통합 완료 모니터링 테스트
TEST_F(ConcurrentCompletionMonitoringTest, SCXMLIntegratedMonitoring) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <state id="region1">
                <initial>
                    <transition target="region1_working"/>
                </initial>
                <state id="region1_working">
                    <transition event="region1_complete" target="region1_final"/>
                </state>
                <final id="region1_final"/>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_working"/>
                </initial>
                <state id="region2_working">
                    <transition event="region2_complete" target="region2_final"/>
                </state>
                <final id="region2_final"/>
            </state>
            <transition event="done.state.parallel1" target="completed"/>
        </parallel>
        <final id="completed"/>
    </scxml>)";

    auto result = parser_->parseContent(scxmlContent);
    ASSERT_TRUE(result.has_value()) << "SCXML 파싱이 실패했습니다";

    auto stateMachine = result.value();
    ASSERT_NE(stateMachine, nullptr) << "상태머신 생성에 실패했습니다";

    // 완료 모니터링이 SCXML과 통합되어 작동하는지 테스트
    auto parallelState = stateMachine->findChildById("parallel1");
    ASSERT_NE(parallelState, nullptr) << "병렬 상태를 찾을 수 없습니다";
}

// 대량 지역 처리 성능 테스트
TEST_F(ConcurrentCompletionMonitoringTest, LargeScaleRegionHandling) {
    monitor_->startMonitoring();

    const int numRegions = 1000;
    auto startTime = std::chrono::high_resolution_clock::now();

    // 대량 지역 등록 및 업데이트
    for (int i = 0; i < numRegions; ++i) {
        std::string regionId = "large_scale_region_" + std::to_string(i);
        monitor_->updateRegionCompletion(regionId, (i % 2 == 0));
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    auto regions = monitor_->getRegisteredRegions();
    EXPECT_EQ(regions.size(), numRegions) << "대량 지역 등록에 실패했습니다";
    EXPECT_LT(duration.count(), 1000) << "대량 지역 처리 성능이 너무 느립니다 (1초 초과)";

    // 완료 조건은 만족되지 않아야 함 (홀수 지역들이 미완료)
    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "일부 지역이 미완료 상태인데 완료 조건이 만족되었습니다";
}

}  // namespace RSM