#include "actions/ActionExecutorImpl.h"
#include "js_engine/JSEngine.h"
#include "parsing/NodeFactory.h"
#include "parsing/SCXMLParser.h"
#include "states/ConcurrentEventBroadcaster.h"
#include "gtest/gtest.h"
#include <memory>
#include <string>

namespace RSM {

class ParallelStateEventBroadcastingTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        engine_->reset();
        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
        broadcaster_ = std::make_unique<ConcurrentEventBroadcaster>();
        sessionId_ = "parallel_event_broadcasting_test";
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
    std::string sessionId_;
};

// 기본 이벤트 브로드캐스팅 테스트
TEST_F(ParallelStateEventBroadcastingTest, BasicEventBroadcasting) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // 이벤트 브로드캐스팅
    EventDescriptor event;
    event.name = "test_event";
    event.data = "test_data";

    bool result = broadcaster_->broadcastToRegions("parallel1", event);
    EXPECT_TRUE(result) << "이벤트 브로드캐스팅이 실패했습니다";
}

// 선택적 이벤트 브로드캐스팅 테스트
TEST_F(ParallelStateEventBroadcastingTest, SelectiveEventBroadcasting) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // 선택적 브로드캐스팅
    EventDescriptor event;
    event.name = "selective_event";
    event.data = "selective_data";

    std::vector<std::string> targetRegions = {"region1", "region3"};
    bool result = broadcaster_->broadcastToSpecificRegions("parallel1", event, targetRegions);
    EXPECT_TRUE(result) << "선택적 이벤트 브로드캐스팅이 실패했습니다";
}

// 이벤트 필터링 테스트
TEST_F(ParallelStateEventBroadcastingTest, EventFiltering) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // 이벤트 필터 설정
    broadcaster_->setEventFilter(
        "parallel1", [](const EventDescriptor &event) { return event.name.find("filtered") != std::string::npos; });

    // 필터링될 이벤트
    EventDescriptor filteredEvent;
    filteredEvent.name = "filtered_event";

    // 필터링되지 않을 이벤트
    EventDescriptor normalEvent;
    normalEvent.name = "normal_event";

    bool filteredResult = broadcaster_->broadcastToRegions("parallel1", filteredEvent);
    bool normalResult = broadcaster_->broadcastToRegions("parallel1", normalEvent);

    EXPECT_TRUE(filteredResult) << "필터링된 이벤트 브로드캐스팅이 실패했습니다";
    EXPECT_FALSE(normalResult) << "일반 이벤트가 필터링되지 않았습니다";
}

// 동시성 테스트
TEST_F(ParallelStateEventBroadcastingTest, ConcurrentBroadcasting) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2", "region3", "region4"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // 동시 브로드캐스팅 테스트
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &successCount, i]() {
            EventDescriptor event;
            event.name = "concurrent_event_" + std::to_string(i);

            if (broadcaster_->broadcastToRegions("parallel1", event)) {
                successCount.fetch_add(1);
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), 10) << "동시 브로드캐스팅에서 일부가 실패했습니다";
}

// 이벤트 우선순위 테스트
TEST_F(ParallelStateEventBroadcastingTest, EventPriority) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // 높은 우선순위 이벤트
    EventDescriptor highPriorityEvent;
    highPriorityEvent.name = "high_priority";
    highPriorityEvent.priority = EventPriority::HIGH;

    // 낮은 우선순위 이벤트
    EventDescriptor lowPriorityEvent;
    lowPriorityEvent.name = "low_priority";
    lowPriorityEvent.priority = EventPriority::LOW;

    bool highResult = broadcaster_->broadcastToRegions("parallel1", highPriorityEvent);
    bool lowResult = broadcaster_->broadcastToRegions("parallel1", lowPriorityEvent);

    EXPECT_TRUE(highResult) << "높은 우선순위 이벤트 브로드캐스팅이 실패했습니다";
    EXPECT_TRUE(lowResult) << "낮은 우선순위 이벤트 브로드캐스팅이 실패했습니다";
}

// 이벤트 배치 처리 테스트
TEST_F(ParallelStateEventBroadcastingTest, BatchEventProcessing) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // 배치 이벤트 생성
    std::vector<EventDescriptor> events;
    for (int i = 0; i < 5; ++i) {
        EventDescriptor event;
        event.name = "batch_event_" + std::to_string(i);
        events.push_back(event);
    }

    bool result = broadcaster_->broadcastBatchToRegions("parallel1", events);
    EXPECT_TRUE(result) << "배치 이벤트 브로드캐스팅이 실패했습니다";
}

// 이벤트 통계 테스트
TEST_F(ParallelStateEventBroadcastingTest, EventStatistics) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // 여러 이벤트 브로드캐스팅
    for (int i = 0; i < 5; ++i) {
        EventDescriptor event;
        event.name = "stats_event_" + std::to_string(i);
        broadcaster_->broadcastToRegions("parallel1", event);
    }

    auto stats = broadcaster_->getStatistics("parallel1");
    EXPECT_GT(stats.totalEventsBroadcast, 0) << "브로드캐스트된 이벤트 수가 0입니다";
    EXPECT_EQ(stats.totalRegions, regionIds.size()) << "등록된 지역 수가 일치하지 않습니다";
}

// 에러 처리 테스트
TEST_F(ParallelStateEventBroadcastingTest, ErrorHandling) {
    // 존재하지 않는 병렬 상태에 이벤트 브로드캐스팅
    EventDescriptor event;
    event.name = "error_test_event";

    bool result = broadcaster_->broadcastToRegions("nonexistent_parallel", event);
    EXPECT_FALSE(result) << "존재하지 않는 병렬 상태에 대한 브로드캐스팅이 성공했습니다";

    // 빈 지역 목록에 브로드캐스팅
    broadcaster_->registerParallelState("empty_parallel", {});
    result = broadcaster_->broadcastToRegions("empty_parallel", event);
    EXPECT_FALSE(result) << "빈 지역 목록에 대한 브로드캐스팅이 성공했습니다";
}

// SCXML 통합 이벤트 브로드캐스팅 테스트
TEST_F(ParallelStateEventBroadcastingTest, SCXMLIntegratedBroadcasting) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <state id="region1">
                <initial>
                    <transition target="region1_listening"/>
                </initial>
                <state id="region1_listening">
                    <transition event="broadcast_test" target="region1_received"/>
                </state>
                <state id="region1_received">
                    <onentry>
                        <assign location="region1_got_event" expr="true"/>
                    </onentry>
                </state>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_listening"/>
                </initial>
                <state id="region2_listening">
                    <transition event="broadcast_test" target="region2_received"/>
                </state>
                <state id="region2_received">
                    <onentry>
                        <assign location="region2_got_event" expr="true"/>
                    </onentry>
                </state>
            </state>
        </parallel>
    </scxml>)";

    auto result = parser_->parseContent(scxmlContent);
    ASSERT_TRUE(result.has_value()) << "SCXML 파싱이 실패했습니다";

    auto stateMachine = result.value();
    ASSERT_NE(stateMachine, nullptr) << "상태머신 생성에 실패했습니다";

    // 이벤트 브로드캐스팅이 SCXML과 통합되어 작동하는지 테스트
    auto parallelState = stateMachine->findChildById("parallel1");
    ASSERT_NE(parallelState, nullptr) << "병렬 상태를 찾을 수 없습니다";
}

}  // namespace RSM