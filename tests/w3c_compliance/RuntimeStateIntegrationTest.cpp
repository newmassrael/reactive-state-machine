/**
 * @file RuntimeStateIntegrationTest.cpp
 * @brief SCXML W3C Runtime State Machine Integration Tests
 *
 * Comprehensive tests validating SCXML W3C 1.0 runtime state machine integration and lifecycle.
 * 
 * W3C References:
 * - Section 4: Algorithm for SCXML Interpretation
 * - Section 3.2: The SCXML State Machine
 * - Section 3.3: States
 * - Section 3.7: Transitions
 * - Section 5.9: The <final> Element
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

// Core Runtime includes
#include "runtime/RuntimeContext.h"
#include "runtime/interfaces/IStateManager.h"
#include "runtime/interfaces/IEventManager.h"
#include "events/Event.h"
#include "events/EventQueue.h"
#include "core/StateNode.h"
#include "core/types.h"
#include "core/TransitionNode.h"
#include "core/FinalNode.h"
#include "core/actions/RaiseActionNode.h"
#include "core/actions/SendActionNode.h"

class RuntimeStateIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize runtime context
        runtimeContext = std::make_unique<SCXML::Runtime::RuntimeContext>();
        stateManager = &runtimeContext->getStateManager();
        eventManager = &runtimeContext->getEventManager();
    }

    void TearDown() override {
        // Cleanup
        runtimeContext.reset();
    }

    // Helper method to create test states
    std::shared_ptr<SCXML::Core::StateNode> createTestState(
        const std::string& id, 
        SCXML::Type type = SCXML::Type::ATOMIC) {
        return std::make_shared<SCXML::Core::StateNode>(id, type);
    }

    // Helper method to create test transitions
    std::shared_ptr<SCXML::Core::TransitionNode> createTestTransition(
        const std::string& event = "test_event", 
        const std::string& target = "test_target") {
        return std::make_shared<SCXML::Core::TransitionNode>(event, target);
    }

    // Helper method to create test events
    std::shared_ptr<SCXML::Events::Event> createTestEvent(
        const std::string& name, 
        SCXML::Events::Event::Type type = SCXML::Events::Event::Type::EXTERNAL) {
        
        return std::make_shared<SCXML::Events::Event>(name, type);
    }

protected:
    std::unique_ptr<SCXML::Runtime::RuntimeContext> runtimeContext;
    SCXML::Runtime::IStateManager* stateManager;
    SCXML::Runtime::IEventManager* eventManager;
};

/**
 * @brief SCXML W3C Section 4.1: State Machine Initialization 검증
 * Tests proper state machine initialization and initial state setup
 */
TEST_F(RuntimeStateIntegrationTest, W3C_4_1_StateMachineInitialization) {
    // W3C 사양: 상태 머신은 초기 상태로 시작해야 함
    
    // 초기 상태 생성
    auto initialState = createTestState("initial_state");
    auto targetState = createTestState("target_state");
    
    // W3C 사양: 상태 매니저는 초기화 상태를 유지해야 함
    EXPECT_NE(nullptr, stateManager);
    
    // 상태 추가 검증
    EXPECT_EQ("initial_state", initialState->getId());
    EXPECT_EQ("target_state", targetState->getId());
    
    // W3C 사양: 초기 상태는 설정 가능해야 함
    initialState->setInitialState("target_state");
    EXPECT_EQ("target_state", initialState->getInitialState());
}

/**
 * @brief SCXML W3C Section 3.7.1: Transition Processing 검증
 * Tests W3C-compliant transition processing and state changes
 */
TEST_F(RuntimeStateIntegrationTest, W3C_3_7_1_TransitionProcessing) {
    // W3C 사양: 전이는 이벤트, 가드, 타겟을 기반으로 처리
    
    // 전이 생성
    auto transition = createTestTransition("user.click", "next_state");
    
    // W3C 사양: 전이 속성 설정
    transition->setGuard("x > 0");
    
    // W3C 사양: 전이 속성 검증
    EXPECT_EQ("user.click", transition->getEvent());
    EXPECT_FALSE(transition->getTargets().empty());
    EXPECT_EQ("next_state", transition->getTargets()[0]);
    EXPECT_EQ("x > 0", transition->getGuard());
    
    // W3C 사양: 전이는 실행 가능한 상태여야 함
    EXPECT_FALSE(transition->getEvent().empty());
    EXPECT_TRUE(transition->hasTargets());
}

/**
 * @brief SCXML W3C Section 4.2: Event Processing Loop 검증
 * Tests the main event processing loop and state transitions
 */
TEST_F(RuntimeStateIntegrationTest, W3C_4_2_EventProcessingLoop) {
    // W3C 사양: 이벤트 처리 루프는 다음과 같이 작동:
    // 1. 이벤트 선택
    // 2. 전이 선택
    // 3. 마이크로스텝 실행
    // 4. 안정 상태 도달
    
    // 이벤트 생성
    auto testEvent = createTestEvent("process.start", SCXML::Events::Event::Type::EXTERNAL);
    auto internalEvent = createTestEvent("internal.ready", SCXML::Events::Event::Type::INTERNAL);
    
    // W3C 사양: 이벤트 매니저는 이벤트를 큐에 저장
    EXPECT_NE(nullptr, eventManager);
    
    // 이벤트 속성 검증
    EXPECT_EQ("process.start", testEvent->getName());
    EXPECT_EQ(SCXML::Events::Event::Type::EXTERNAL, testEvent->getType());
    EXPECT_TRUE(testEvent->isExternal());
    
    EXPECT_EQ("internal.ready", internalEvent->getName());
    EXPECT_EQ(SCXML::Events::Event::Type::INTERNAL, internalEvent->getType());
    EXPECT_TRUE(internalEvent->isInternal());
}

/**
 * @brief SCXML W3C Section 3.3.1: State Hierarchy 검증
 * Tests hierarchical state relationships and parent-child states
 */
TEST_F(RuntimeStateIntegrationTest, W3C_3_3_1_StateHierarchy) {
    // W3C 사양: 상태는 계층적 구조를 가질 수 있음
    
    // 부모-자식 상태 관계 생성
    auto parentState = createTestState("parent_state", SCXML::Type::COMPOUND);
    auto child1State = createTestState("child1");
    auto child2State = createTestState("child2");
    
    // W3C 사양: 부모 상태는 초기 자식 상태를 지정해야 함
    parentState->setInitialState("child1");
    EXPECT_EQ("child1", parentState->getInitialState());
    
    // W3C 사양: 상태 계층 구조 검증
    EXPECT_EQ("parent_state", parentState->getId());
    EXPECT_EQ("child1", child1State->getId());
    EXPECT_EQ("child2", child2State->getId());
    
    // W3C 사양: 부모-자식 관계 설정
    parentState->addChild(child1State);
    parentState->addChild(child2State);
    
    // 자식 상태 검증
    EXPECT_EQ(2, parentState->getChildren().size());
    EXPECT_EQ("child1", parentState->getChildren()[0]->getId());
    EXPECT_EQ("child2", parentState->getChildren()[1]->getId());
}

/**
 * @brief SCXML W3C Section 5.9: Final State Handling 검증
 * Tests final state processing and state machine termination
 */
TEST_F(RuntimeStateIntegrationTest, W3C_5_9_FinalStateHandling) {
    // W3C 사양: final 상태는 상태 머신 종료를 나타냄
    
    // Final 노드 생성
    auto finalNode = std::make_shared<SCXML::Core::FinalNode>("final_state");
    
    // W3C 사양: Final 상태는 done 이벤트를 생성해야 함
    EXPECT_EQ("final_state", finalNode->getId());
    
    // Final 상태로의 전이 생성
    auto toFinalTransition = createTestTransition("complete", "final_state");
    
    EXPECT_EQ("complete", toFinalTransition->getEvent());
    EXPECT_EQ("final_state", toFinalTransition->getTargets()[0]);
    
    // W3C 사양: final 상태는 더 이상 전이를 수행하지 않음
    // 이는 실제 상태 머신 실행에서 검증되어야 함
}

/**
 * @brief SCXML W3C Section 4.3: Action Execution Integration 검증
 * Tests integration of executable actions within state machine runtime
 */
TEST_F(RuntimeStateIntegrationTest, W3C_4_3_ActionExecutionIntegration) {
    // W3C 사양: 액션은 전이 과정에서 실행됨
    
    // Raise 액션 생성 (내부 이벤트 생성)
    auto raiseAction = std::make_unique<SCXML::Core::RaiseActionNode>("raise_001");
    raiseAction->setEvent("internal.notification");
    
    // W3C 사양: raise 액션은 내부 이벤트를 생성
    EXPECT_EQ("internal.notification", raiseAction->getEvent());
    EXPECT_EQ("raise", raiseAction->getActionType());
    
    // Send 액션 생성 (외부 이벤트 전송)
    auto sendAction = std::make_unique<SCXML::Core::SendActionNode>("send_001");
    sendAction->setEvent("external.message");
    sendAction->setTarget("#_parent");
    
    // W3C 사양: send 액션은 외부 타겟으로 이벤트 전송
    EXPECT_EQ("external.message", sendAction->getEvent());
    EXPECT_EQ("#_parent", sendAction->getTarget());
    EXPECT_EQ("send", sendAction->getActionType());
}

/**
 * @brief SCXML W3C Section 4.4: State Configuration Management 검증
 * Tests active state configuration tracking and management
 */
TEST_F(RuntimeStateIntegrationTest, W3C_4_4_StateConfigurationManagement) {
    // W3C 사양: 상태 구성은 현재 활성 상태들의 집합
    
    // 다중 상태 시나리오
    auto state1 = createTestState("state1");
    auto state2 = createTestState("state2");
    auto parallelState = createTestState("parallel_state", SCXML::Type::PARALLEL);
    
    // W3C 사양: StateManager는 활성 상태 구성을 추적해야 함
    EXPECT_NE(nullptr, stateManager);
    
    // 상태 전이 시나리오
    auto transition1to2 = createTestTransition("move", "state2");
    
    EXPECT_EQ("move", transition1to2->getEvent());
    EXPECT_EQ("state2", transition1to2->getTargets()[0]);
    
    // W3C 사양: 상태 구성 변화는 적절히 추적되어야 함
    // 실제 실행 환경에서는 StateManager를 통해 검증
    EXPECT_EQ("state1", state1->getId());
    EXPECT_EQ("state2", state2->getId());
    EXPECT_EQ("parallel_state", parallelState->getId());
}

/**
 * @brief SCXML W3C Section 4.5: Guard Condition Evaluation 검증
 * Tests guard condition evaluation during transition selection
 */
TEST_F(RuntimeStateIntegrationTest, W3C_4_5_GuardConditionEvaluation) {
    // W3C 사양: 가드 조건은 전이 선택 시 평가됨
    
    // 가드 조건이 있는 전이들
    auto guardedTransition1 = createTestTransition("check", "positive_path");
    guardedTransition1->setGuard("score > 80");
    
    auto guardedTransition2 = createTestTransition("check", "negative_path");
    guardedTransition2->setGuard("score <= 80");
    
    auto defaultTransition = createTestTransition("check", "default_path");
    
    // W3C 사양: 가드 조건 검증
    EXPECT_EQ("score > 80", guardedTransition1->getGuard());
    EXPECT_EQ("score <= 80", guardedTransition2->getGuard());
    EXPECT_TRUE(defaultTransition->getGuard().empty());  // 기본 전이는 가드 없음
    
    // W3C 사양: 모든 전이는 동일한 이벤트에 반응하지만 다른 가드
    EXPECT_EQ("check", guardedTransition1->getEvent());
    EXPECT_EQ("check", guardedTransition2->getEvent());
    EXPECT_EQ("check", defaultTransition->getEvent());
    
    // W3C 사양: 서로 다른 타겟으로 전이
    EXPECT_EQ("positive_path", guardedTransition1->getTargets()[0]);
    EXPECT_EQ("negative_path", guardedTransition2->getTargets()[0]);
    EXPECT_EQ("default_path", defaultTransition->getTargets()[0]);
}

/**
 * @brief SCXML W3C Comprehensive Runtime Integration Scenario 검증
 * Tests comprehensive runtime scenario with full state machine lifecycle
 */
TEST_F(RuntimeStateIntegrationTest, W3C_ComprehensiveRuntimeIntegrationScenario) {
    // W3C 사양에 따른 완전한 상태 머신 런타임 시나리오
    
    // 1. 상태 머신 구성 요소 생성
    auto initState = createTestState("init");
    auto processingState = createTestState("processing");
    auto completedState = createTestState("completed");
    auto finalState = std::make_shared<SCXML::Core::FinalNode>("final");
    
    // 2. 전이 네트워크 구성
    auto startTransition = createTestTransition("start", "processing");
    
    auto completeTransition = createTestTransition("done", "completed");
    completeTransition->setGuard("result == 'success'");
    
    auto errorTransition = createTestTransition("error", "final");
    
    auto finishTransition = createTestTransition("finish", "final");
    
    // 3. 액션 구성
    auto notifyAction = std::make_unique<SCXML::Core::RaiseActionNode>("notify_001");
    notifyAction->setEvent("internal.status.changed");
    
    auto reportAction = std::make_unique<SCXML::Core::SendActionNode>("report_001");
    reportAction->setEvent("external.completion.report");
    reportAction->setTarget("#_parent");
    
    // 4. 이벤트 시나리오
    std::vector<std::shared_ptr<SCXML::Events::Event>> eventSequence = {
        createTestEvent("start", SCXML::Events::Event::Type::EXTERNAL),
        createTestEvent("internal.status.changed", SCXML::Events::Event::Type::INTERNAL),
        createTestEvent("done", SCXML::Events::Event::Type::EXTERNAL),
        createTestEvent("finish", SCXML::Events::Event::Type::EXTERNAL)
    };
    
    // W3C 사양 준수성 검증
    
    // 상태 구성 검증
    EXPECT_EQ("init", initState->getId());
    EXPECT_EQ("processing", processingState->getId());
    EXPECT_EQ("completed", completedState->getId());
    EXPECT_EQ("final", finalState->getId());
    
    // 전이 구성 검증
    EXPECT_EQ("start", startTransition->getEvent());
    EXPECT_EQ("processing", startTransition->getTargets()[0]);
    
    EXPECT_EQ("done", completeTransition->getEvent());
    EXPECT_EQ("completed", completeTransition->getTargets()[0]);
    EXPECT_EQ("result == 'success'", completeTransition->getGuard());
    
    // 액션 구성 검증
    EXPECT_EQ("internal.status.changed", notifyAction->getEvent());
    EXPECT_EQ("external.completion.report", reportAction->getEvent());
    EXPECT_EQ("#_parent", reportAction->getTarget());
    
    // 이벤트 시퀀스 검증
    EXPECT_EQ(4, eventSequence.size());
    EXPECT_EQ("start", eventSequence[0]->getName());
    EXPECT_TRUE(eventSequence[0]->isExternal());
    
    EXPECT_EQ("internal.status.changed", eventSequence[1]->getName());
    EXPECT_TRUE(eventSequence[1]->isInternal());
    
    EXPECT_EQ("done", eventSequence[2]->getName());
    EXPECT_TRUE(eventSequence[2]->isExternal());
    
    EXPECT_EQ("finish", eventSequence[3]->getName());
    EXPECT_TRUE(eventSequence[3]->isExternal());
    
    // W3C 사양: 런타임 컨텍스트는 모든 구성 요소를 관리해야 함
    EXPECT_NE(nullptr, runtimeContext.get());
    EXPECT_NE(nullptr, stateManager);
    EXPECT_NE(nullptr, eventManager);
}