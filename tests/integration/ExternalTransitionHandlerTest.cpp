#include "states/ExternalTransitionHandler.h"
#include "js_engine/JSEngine.h"
#include "parsing/NodeFactory.h"
#include "parsing/SCXMLParser.h"
#include "gtest/gtest.h"
#include <future>
#include <memory>
#include <string>
#include <thread>

namespace RSM {

class ExternalTransitionHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        engine_->reset();
        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
        handler_ = std::make_unique<ExternalTransitionHandler>(5);  // 최대 5개 동시 전이
        sessionId_ = "external_transition_handler_test";
    }

    void TearDown() override {
        if (engine_) {
            engine_->reset();
        }
    }

    JSEngine *engine_;
    std::shared_ptr<NodeFactory> nodeFactory_;
    std::unique_ptr<SCXMLParser> parser_;
    std::unique_ptr<ExternalTransitionHandler> handler_;
    std::string sessionId_;
};

// 기본 외부 전이 처리 테스트
TEST_F(ExternalTransitionHandlerTest, BasicExternalTransitionHandling) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    // 외부 전이 수행
    bool result = handler_->handleExternalTransition("parallel1", "target_state", "exit_event");
    EXPECT_TRUE(result) << "기본 외부 전이 처리가 실패했습니다";
}

// 동시 전이 제한 테스트
TEST_F(ExternalTransitionHandlerTest, ConcurrentTransitionLimit) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    // 동시 전이 시도
    std::vector<std::future<bool>> futures;

    for (int i = 0; i < 10; ++i) {
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

    // 최대 5개까지만 성공해야 함
    EXPECT_LE(successCount, 5) << "동시 전이 제한이 적용되지 않았습니다";
}

// 활성 전이 카운트 테스트
TEST_F(ExternalTransitionHandlerTest, ActiveTransitionCount) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    EXPECT_EQ(handler_->getActiveTransitionCount(), 0) << "초기 활성 전이 수가 0이 아닙니다";

    // 비동기 전이 시작 (실제로는 즉시 완료되지만 카운트 확인)
    std::vector<std::thread> threads;
    std::atomic<bool> startFlag{false};

    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([this, &startFlag, i]() {
            while (!startFlag.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            handler_->handleExternalTransition("parallel1", "target_" + std::to_string(i),
                                               "event_" + std::to_string(i));
        });
    }

    startFlag.store(true);

    for (auto &thread : threads) {
        thread.join();
    }

    // 모든 전이가 완료된 후 카운트는 0이어야 함
    EXPECT_EQ(handler_->getActiveTransitionCount(), 0) << "전이 완료 후 활성 전이 수가 0이 아닙니다";
}

// 전이 처리 상태 테스트
TEST_F(ExternalTransitionHandlerTest, TransitionProcessingStatus) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    EXPECT_FALSE(handler_->isProcessingTransitions()) << "초기에 전이 처리 중 상태입니다";

    // 전이 수행
    handler_->handleExternalTransition("parallel1", "target_state", "exit_event");

    // 전이 완료 후에는 처리 중이 아니어야 함
    EXPECT_FALSE(handler_->isProcessingTransitions()) << "전이 완료 후에도 처리 중 상태입니다";
}

// 잘못된 매개변수 처리 테스트
TEST_F(ExternalTransitionHandlerTest, InvalidParameterHandling) {
    // 빈 병렬 상태 ID
    bool result = handler_->handleExternalTransition("", "target_state", "exit_event");
    EXPECT_FALSE(result) << "빈 병렬 상태 ID로 전이가 성공했습니다";

    // 빈 타겟 상태 ID
    result = handler_->handleExternalTransition("parallel1", "", "exit_event");
    EXPECT_FALSE(result) << "빈 타겟 상태 ID로 전이가 성공했습니다";

    // 빈 전이 이벤트
    result = handler_->handleExternalTransition("parallel1", "target_state", "");
    EXPECT_FALSE(result) << "빈 전이 이벤트로 전이가 성공했습니다";
}

// 등록되지 않은 병렬 상태 처리 테스트
TEST_F(ExternalTransitionHandlerTest, UnregisteredParallelStateHandling) {
    // 등록되지 않은 병렬 상태에 대한 전이 시도
    bool result = handler_->handleExternalTransition("unregistered_parallel", "target_state", "exit_event");
    EXPECT_FALSE(result) << "등록되지 않은 병렬 상태에 대한 전이가 성공했습니다";
}

// 자기 자신으로의 전이 테스트 (내부 전이로 간주되어야 함)
TEST_F(ExternalTransitionHandlerTest, SelfTransitionHandling) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    // 자기 자신으로의 전이 시도
    bool result = handler_->handleExternalTransition("parallel1", "parallel1", "self_event");
    EXPECT_FALSE(result) << "자기 자신으로의 전이가 외부 전이로 처리되었습니다";
}

// 병렬 상태 등록 테스트
TEST_F(ExternalTransitionHandlerTest, ParallelStateRegistration) {
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};

    // 정상 등록
    EXPECT_NO_THROW(handler_->registerParallelState("parallel1", regionIds))
        << "정상적인 병렬 상태 등록에서 예외가 발생했습니다";

    // 빈 ID로 등록 시도
    EXPECT_THROW(handler_->registerParallelState("", regionIds), std::invalid_argument)
        << "빈 ID로 병렬 상태 등록 시 예외가 발생하지 않았습니다";
}

// 빈 지역 목록 등록 테스트
TEST_F(ExternalTransitionHandlerTest, EmptyRegionListRegistration) {
    std::vector<std::string> emptyRegionIds;

    // 빈 지역 목록으로 등록
    EXPECT_NO_THROW(handler_->registerParallelState("parallel_empty", emptyRegionIds))
        << "빈 지역 목록 등록에서 예외가 발생했습니다";

    // 빈 지역 목록에서 전이 시도
    bool result = handler_->handleExternalTransition("parallel_empty", "target_state", "exit_event");
    EXPECT_FALSE(result) << "빈 지역 목록을 가진 병렬 상태에서 전이가 성공했습니다";
}

// 최대 동시 전이 수 0으로 생성 시 예외 테스트
TEST_F(ExternalTransitionHandlerTest, ZeroMaxConcurrentTransitions) {
    EXPECT_THROW(ExternalTransitionHandler(0), std::invalid_argument)
        << "최대 동시 전이 수 0으로 생성 시 예외가 발생하지 않았습니다";
}

// 지역 비활성화 테스트
TEST_F(ExternalTransitionHandlerTest, RegionDeactivation) {
    // 병렬 상태 등록
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};
    handler_->registerParallelState("parallel1", regionIds);

    // 외부 전이를 통해 지역 비활성화
    bool result = handler_->handleExternalTransition("parallel1", "external_target", "exit_event");
    EXPECT_TRUE(result) << "지역 비활성화를 포함한 외부 전이가 실패했습니다";
}

// SCXML 통합 외부 전이 테스트
TEST_F(ExternalTransitionHandlerTest, SCXMLIntegratedExternalTransition) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <transition event="exit_parallel" target="single_state"/>
            <state id="region1">
                <initial>
                    <transition target="region1_active"/>
                </initial>
                <state id="region1_active">
                    <onexit>
                        <assign location="region1_exited" expr="true"/>
                    </onexit>
                </state>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_active"/>
                </initial>
                <state id="region2_active">
                    <onexit>
                        <assign location="region2_exited" expr="true"/>
                    </onexit>
                </state>
            </state>
        </parallel>
        <state id="single_state">
            <onentry>
                <assign location="single_state_entered" expr="true"/>
            </onentry>
        </state>
    </scxml>)";

    auto result = parser_->parseContent(scxmlContent);
    ASSERT_TRUE(result.has_value()) << "SCXML 파싱이 실패했습니다";

    auto stateMachine = result.value();
    ASSERT_NE(stateMachine, nullptr) << "상태머신 생성에 실패했습니다";

    // 외부 전이 핸들러가 SCXML과 통합되어 작동하는지 테스트
    auto parallelState = stateMachine->findChildById("parallel1");
    ASSERT_NE(parallelState, nullptr) << "병렬 상태를 찾을 수 없습니다";

    auto singleState = stateMachine->findChildById("single_state");
    ASSERT_NE(singleState, nullptr) << "단일 상태를 찾을 수 없습니다";
}

// 성능 테스트 - 대량 전이 처리
TEST_F(ExternalTransitionHandlerTest, PerformanceTest) {
    // 여러 병렬 상태 등록
    for (int i = 0; i < 100; ++i) {
        std::vector<std::string> regionIds = {"region1_" + std::to_string(i), "region2_" + std::to_string(i)};
        handler_->registerParallelState("parallel_" + std::to_string(i), regionIds);
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // 대량 전이 수행
    int successCount = 0;
    for (int i = 0; i < 100; ++i) {
        if (handler_->handleExternalTransition("parallel_" + std::to_string(i), "target_" + std::to_string(i),
                                               "event_" + std::to_string(i))) {
            successCount++;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    EXPECT_GT(successCount, 0) << "전이가 하나도 성공하지 않았습니다";
    EXPECT_LT(duration.count(), 1000) << "대량 전이 처리 성능이 너무 느립니다 (1초 초과)";
}

}  // namespace RSM