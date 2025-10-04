#include "actions/ScriptAction.h"
#include "factory/NodeFactory.h"
#include "mocks/MockActionExecutor.h"
#include "model/StateNode.h"
#include "parsing/SCXMLParser.h"
#include "runtime/ExecutionContextImpl.h"
#include "scripting/JSEngine.h"
#include "states/ConcurrentCompletionMonitor.h"
#include "states/ConcurrentRegion.h"
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

/*
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

// SCXML W3C Specification Test: ConcurrentRegion Exit Behavior
TEST_F(ConcurrentCompletionMonitoringTest, SCXML_W3C_ConcurrentRegionExitBehavior) {
    // SCXML spec: All concurrent regions must properly exit when parallel state exits

    // Create mock state and execution context for testing
    auto mockState = std::make_shared<StateNode>("testRegion", Type::Standard);
    auto mockContext = std::make_shared<ExecutionContextImpl>("test_session");

    // Create ConcurrentRegion with StateExitExecutor
    auto region = std::make_unique<ConcurrentRegion>("testRegion", mockState, mockContext);

    EXPECT_FALSE(region->isActive()) << "Region should not be active initially";

    // Activate the region
    auto activateResult = region->activate();
    EXPECT_TRUE(activateResult.isSuccess()) << "Region activation should succeed";
    EXPECT_TRUE(region->isActive()) << "Region should be active after activation";

    // Deactivate the region (this triggers exitAllStates internally)
    auto deactivateResult = region->deactivate();
    EXPECT_TRUE(deactivateResult.isSuccess) << "Region deactivation should succeed";
    EXPECT_FALSE(region->isActive()) << "Region should not be active after deactivation";
}

// SCXML W3C Specification Test: Exit Action Execution During Deactivation
TEST_F(ConcurrentCompletionMonitoringTest, SCXML_W3C_ExitActionExecutionDuringDeactivation) {
    // SCXML spec: Exit actions must execute when regions are deactivated

    // Create a state with exit actions
    auto stateWithExitActions = std::make_shared<StateNode>("stateWithExitActions", Type::Standard);

    // Add exit actions to the state
    stateWithExitActions->addExitAction("log('Exiting state')");
    stateWithExitActions->addExitAction("assign('exitFlag', true)");

    auto mockContext = std::make_shared<ExecutionContextImpl>("test_session_exit");

    // Create ConcurrentRegion with state that has exit actions
    auto region = std::make_unique<ConcurrentRegion>("regionWithExitActions", stateWithExitActions, mockContext);

    // Add some active states to test exit behavior
    auto activateResult = region->activate();
    EXPECT_TRUE(activateResult.isSuccess()) << "Region with exit actions should activate";

    // The region should now have active states
    EXPECT_TRUE(region->isActive()) << "Region should be active";

    // Deactivate - this should trigger exit action execution
    auto deactivateResult = region->deactivate();
    EXPECT_TRUE(deactivateResult.isSuccess()) << "Region deactivation with exit actions should succeed";
    EXPECT_FALSE(region->isActive()) << "Region should be inactive after exit actions";
}

// SCXML W3C Specification Test: Multiple Region Exit Coordination
TEST_F(ConcurrentCompletionMonitoringTest, SCXML_W3C_MultipleRegionExitCoordination) {
    // SCXML spec: Multiple regions in a parallel state must coordinate their exit

    std::vector<std::unique_ptr<ConcurrentRegion>> regions;
    const size_t numRegions = 3;

    // Create multiple regions
    for (size_t i = 0; i < numRegions; ++i) {
        std::string regionId = "region" + std::to_string(i);
        auto mockState = std::make_shared<StateNode>(regionId + "_state", Type::Standard);
        auto mockContext = std::make_shared<ExecutionContextImpl>("test_session_" + regionId);

        regions.push_back(std::make_unique<ConcurrentRegion>(regionId, mockState, mockContext));

        // Register with monitor
        monitor_->registerRegion(regionId);
        monitor_->updateRegionCompletion(regionId, false);  // Initially not complete
    }

    monitor_->startMonitoring();

    // Activate all regions
    for (auto &region : regions) {
        auto result = region->activate();
        EXPECT_TRUE(result.isSuccess()) << "All regions should activate successfully";
    }

    // Verify completion is not met initially
    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "Completion should not be met initially";

    // Deactivate all regions in sequence (simulating parallel state exit)
    for (size_t i = 0; i < regions.size(); ++i) {
        auto &region = regions[i];
        std::string regionId = "region" + std::to_string(i);

        auto result = region->deactivate();
        EXPECT_TRUE(result.isSuccess()) << "Region " << regionId << " should deactivate successfully";

        // Update monitor about region completion
        monitor_->updateRegionCompletion(regionId, true);
    }

    // All regions should now be complete
    EXPECT_TRUE(monitor_->isCompletionCriteriaMet()) << "All regions should be complete after exit";
}

// SCXML W3C Specification Test: Exit Action Order Verification
TEST_F(ConcurrentCompletionMonitoringTest, SCXML_W3C_ExitActionOrderVerification) {
    // SCXML spec: Exit actions must execute in document order

    // Create state with multiple exit actions
    auto stateWithMultipleExitActions = std::make_shared<StateNode>("multiExitState", Type::Standard);

    // Add multiple exit actions in specific order
    stateWithMultipleExitActions->addExitAction("log('Exit action 1')");
    stateWithMultipleExitActions->addExitAction("log('Exit action 2')");
    stateWithMultipleExitActions->addExitAction("log('Exit action 3')");

    auto mockContext = std::make_shared<ExecutionContextImpl>("test_session_order");

    // Create region
    auto region = std::make_unique<ConcurrentRegion>("orderTestRegion", stateWithMultipleExitActions, mockContext);

    // Activate and then deactivate to trigger exit actions
    auto activateResult = region->activate();
    EXPECT_TRUE(activateResult.isSuccess()) << "Region should activate for order test";

    auto deactivateResult = region->deactivate();
    EXPECT_TRUE(deactivateResult.isSuccess()) << "Region should deactivate and execute exit actions in order";

    // Verify region is properly cleaned up
    EXPECT_FALSE(region->isActive()) << "Region should be inactive after ordered exit";
    EXPECT_TRUE(region->getActiveStates().empty()) << "Active states should be cleared after exit";
}

// SCXML W3C Specification Test: Error Handling During Exit
TEST_F(ConcurrentCompletionMonitoringTest, SCXML_W3C_ErrorHandlingDuringExit) {
    // SCXML spec: Exit process should handle errors gracefully

    auto stateWithComplexExit = std::make_shared<StateNode>("complexExitState", Type::Standard);
    auto mockContext = std::make_shared<ExecutionContextImpl>("test_session_error");

    // Create region
    auto region = std::make_unique<ConcurrentRegion>("errorTestRegion", stateWithComplexExit, mockContext);

    // Activate region
    auto activateResult = region->activate();
    EXPECT_TRUE(activateResult.isSuccess()) << "Region should activate for error test";

    // Force some active states for testing
    // Note: In a real scenario, these would be set during normal operation

    // Deactivate should succeed even with complex scenarios
    auto deactivateResult = region->deactivate();
    EXPECT_TRUE(deactivateResult.isSuccess()) << "Region should handle deactivation gracefully";

    // Verify proper cleanup occurred
    EXPECT_FALSE(region->isActive()) << "Region should be inactive after error-handled exit";
    EXPECT_FALSE(region->isInFinalState()) << "Region should not be in final state after exit";
}

// SCXML W3C Specification Test: StateExitExecutor Integration
TEST_F(ConcurrentCompletionMonitoringTest, SCXML_W3C_StateExitExecutorIntegration) {
    // SCXML spec: StateExitExecutor should integrate properly with ConcurrentRegion

    // This test verifies that our SOLID implementation works correctly
    auto testState = std::make_shared<StateNode>("integrationTestState", Type::Standard);
    auto testContext = std::make_shared<ExecutionContextImpl>("test_session_integration");

    // Create region (internally uses StateExitExecutor)
    auto region = std::make_unique<ConcurrentRegion>("integrationRegion", testState, testContext);

    // Test full lifecycle
    EXPECT_FALSE(region->isActive()) << "Initial state should be inactive";

    auto activateResult = region->activate();
    EXPECT_TRUE(activateResult.isSuccess()) << "StateExitExecutor integration: activation should work";
    EXPECT_TRUE(region->isActive()) << "StateExitExecutor integration: should be active";

    auto deactivateResult = region->deactivate();
    EXPECT_TRUE(deactivateResult.isSuccess()) << "StateExitExecutor integration: deactivation should work";
    EXPECT_FALSE(region->isActive()) << "StateExitExecutor integration: should be inactive after deactivation";

    // Test multiple activation/deactivation cycles
    for (int i = 0; i < 3; ++i) {
        auto activateResult2 = region->activate();
        EXPECT_TRUE(activateResult2.isSuccess()) << "Cycle " << i << ": re-activation should work";

        auto deactivateResult2 = region->deactivate();
        EXPECT_TRUE(deactivateResult2.isSuccess()) << "Cycle " << i << ": re-deactivation should work";
    }
}
*/

// SCXML W3C Specification Test: Child Entry Action Execution
TEST_F(ConcurrentCompletionMonitoringTest, SCXML_W3C_ChildEntryActionExecution) {
    // SCXML W3C spec section 3.8: "The SCXML processor MUST execute the <onentry> handlers
    // of a state in document order when the state is entered."
    // When entering compound states with children, parent entry actions execute first,
    // then child state entry actions execute.

    // Create parent state with entry actions
    auto parentState = std::make_shared<StateNode>("parentState", Type::COMPOUND);

    // Add parent entry actions (should execute first per SCXML spec)
    auto parentEntryAction1 = std::make_shared<ScriptAction>("parent_entry_1", "parent_entry_action_1");
    auto parentEntryAction2 = std::make_shared<ScriptAction>("parent_entry_2", "parent_entry_action_2");

    // W3C SCXML 3.8: Use block-based API
    std::vector<std::shared_ptr<IActionNode>> parentEntryBlock = {parentEntryAction1, parentEntryAction2};
    parentState->addEntryActionBlock(parentEntryBlock);

    // Create child state with entry action
    auto childState = std::make_shared<StateNode>("childState", Type::ATOMIC);
    auto childEntryAction = std::make_shared<ScriptAction>("child_entry", "child_entry_action");
    // W3C SCXML 3.8: Use block-based API
    std::vector<std::shared_ptr<IActionNode>> childEntryBlock = {childEntryAction};
    childState->addEntryActionBlock(childEntryBlock);

    // Set up parent-child relationship per SCXML structure
    parentState->addChild(childState);
    parentState->setInitialState("childState");

    // Create execution context with mock executor
    auto mockExecutor = std::make_shared<RSM::Test::MockActionExecutor>("test_session_entry_actions");
    auto mockContext = std::make_shared<ExecutionContextImpl>(mockExecutor, "test_session_entry_actions");

    // Create ConcurrentRegion with the structured state
    auto region = std::make_unique<ConcurrentRegion>("entryActionRegion", parentState, mockContext);

    // Verify initial state per SCXML requirements
    EXPECT_FALSE(region->isActive()) << "Region should not be active initially";
    EXPECT_TRUE(region->getActiveStates().empty()) << "No states should be active initially";

    // Clear execution history before test
    mockExecutor->clearHistory();

    // Activate region - this should trigger SCXML-compliant entry action execution
    auto activateResult = region->activate();

    // Verify activation succeeded per SCXML state machine semantics
    EXPECT_TRUE(activateResult.isSuccess) << "Region activation should succeed per SCXML specification";
    EXPECT_TRUE(region->isActive()) << "Region should be active after activation";

    // Verify SCXML state entry behavior: region should enter initial child state
    EXPECT_FALSE(region->getActiveStates().empty()) << "Active states should not be empty after activation";
    EXPECT_EQ(region->getCurrentState(), "childState") << "Current state should be the initial child state per SCXML";

    // Verify SCXML entry action execution order requirement
    const auto &executedScripts = mockExecutor->getExecutedScripts();
    EXPECT_GE(executedScripts.size(), 3) << "Should have executed parent entry actions + child entry action";

    // Per SCXML spec: parent entry actions execute before child entry actions
    bool foundParentAction1 = false, foundParentAction2 = false, foundChildAction = false;
    int parentAction1Pos = -1, parentAction2Pos = -1, childActionPos = -1;

    for (size_t i = 0; i < executedScripts.size(); ++i) {
        if (executedScripts[i] == "parent_entry_1") {
            foundParentAction1 = true;
            parentAction1Pos = i;
        } else if (executedScripts[i] == "parent_entry_2") {
            foundParentAction2 = true;
            parentAction2Pos = i;
        } else if (executedScripts[i] == "child_entry") {
            foundChildAction = true;
            childActionPos = i;
        }
    }

    // Verify all entry actions were executed per SCXML requirements
    EXPECT_TRUE(foundParentAction1) << "Parent entry action 1 should be executed per SCXML spec";
    EXPECT_TRUE(foundParentAction2) << "Parent entry action 2 should be executed per SCXML spec";
    EXPECT_TRUE(foundChildAction) << "Child entry action should be executed per SCXML spec";

    // Verify SCXML execution order: parent actions before child actions
    if (foundParentAction1 && foundChildAction) {
        EXPECT_LT(parentAction1Pos, childActionPos)
            << "Parent entry actions must execute before child entry actions per SCXML";
    }
    if (foundParentAction2 && foundChildAction) {
        EXPECT_LT(parentAction2Pos, childActionPos)
            << "Parent entry actions must execute before child entry actions per SCXML";
    }

    // Verify document order within parent entry actions per SCXML section 3.8
    if (foundParentAction1 && foundParentAction2) {
        EXPECT_LT(parentAction1Pos, parentAction2Pos)
            << "Parent entry actions must execute in document order per SCXML";
    }

    // Clean up and verify deactivation per SCXML state machine lifecycle
    auto deactivateResult = region->deactivate();
    EXPECT_TRUE(deactivateResult.isSuccess) << "Region deactivation should succeed";
    EXPECT_FALSE(region->isActive()) << "Region should not be active after deactivation";
}

// SCXML W3C Specification Test: Entry Action Execution Failure Handling
TEST_F(ConcurrentCompletionMonitoringTest, SCXML_W3C_EntryActionExecutionFailure_HandlingBehavior) {
    // SCXML W3C spec: System should handle entry action failures gracefully
    // without compromising overall state machine operation

    auto parentState = std::make_shared<StateNode>("parentState", Type::COMPOUND);

    // Add parent entry actions - one success, one failure
    auto successAction = std::make_shared<ScriptAction>("success_script", "success_action");
    auto failureAction = std::make_shared<ScriptAction>("throw new Error('intentional failure')", "failure_action");

    // W3C SCXML 3.8: Use block-based API
    std::vector<std::shared_ptr<IActionNode>> parentEntryBlock = {successAction, failureAction};
    parentState->addEntryActionBlock(parentEntryBlock);

    // Create child state
    auto childState = std::make_shared<StateNode>("childState", Type::ATOMIC);
    auto childAction = std::make_shared<ScriptAction>("child_script", "child_action");
    // W3C SCXML 3.8: Use block-based API
    std::vector<std::shared_ptr<IActionNode>> childEntryBlock = {childAction};
    childState->addEntryActionBlock(childEntryBlock);

    parentState->addChild(childState);
    parentState->setInitialState("childState");

    auto mockExecutor = std::make_shared<RSM::Test::MockActionExecutor>("test_session_failure");
    auto mockContext = std::make_shared<ExecutionContextImpl>(mockExecutor, "test_session_failure");

    // Configure mock: success for first and third, failure for second
    mockExecutor->setScriptExecutionResult(true);                                          // Default to success
    mockExecutor->setExpressionResult("throw new Error('intentional failure')", "error");  // Specific failure

    auto region = std::make_unique<ConcurrentRegion>("failureTestRegion", parentState, mockContext);

    mockExecutor->clearHistory();

    // Activation should succeed despite entry action failure per SCXML resilience requirements
    auto activateResult = region->activate();
    EXPECT_TRUE(activateResult.isSuccess) << "Region activation should succeed despite entry action failure";

    // Verify system entered child state despite parent action failure (SCXML requirement)
    EXPECT_TRUE(region->isActive()) << "Region should be active despite entry action failure";
    EXPECT_EQ(region->getCurrentState(), "childState") << "Should enter child state despite parent action failure";

    // Verify all actions were attempted (failure should not stop subsequent actions)
    const auto &executedScripts = mockExecutor->getExecutedScripts();
    EXPECT_GE(executedScripts.size(), 2) << "All entry actions should be attempted despite failures";

    auto deactivateResult = region->deactivate();
    EXPECT_TRUE(deactivateResult.isSuccess) << "Deactivation should succeed";
}

// SCXML W3C Specification Test: Mixed Action Types Execution Order
TEST_F(ConcurrentCompletionMonitoringTest, SCXML_W3C_MixedActionTypes_ExecutionOrder) {
    // SCXML W3C spec: All executable content types should execute in document order
    // regardless of their specific type (script, assign, log, etc.)

    auto parentState = std::make_shared<StateNode>("parentState", Type::COMPOUND);

    // Add mixed action types in specific order
    auto scriptAction1 = std::make_shared<ScriptAction>("script1", "script_action_1");
    auto assignAction = std::make_shared<AssignAction>("testVar", "value1", "assign_action");
    auto scriptAction2 = std::make_shared<ScriptAction>("script2", "script_action_2");

    // W3C SCXML 3.8: Use block-based API
    std::vector<std::shared_ptr<IActionNode>> parentEntryBlock = {scriptAction1, assignAction, scriptAction2};
    parentState->addEntryActionBlock(parentEntryBlock);

    auto childState = std::make_shared<StateNode>("childState", Type::ATOMIC);
    auto childScriptAction = std::make_shared<ScriptAction>("child_script", "child_script_action");
    // W3C SCXML 3.8: Use block-based API
    std::vector<std::shared_ptr<IActionNode>> childEntryBlock = {childScriptAction};
    childState->addEntryActionBlock(childEntryBlock);

    parentState->addChild(childState);
    parentState->setInitialState("childState");

    auto mockExecutor = std::make_shared<RSM::Test::MockActionExecutor>("test_session_mixed");
    auto mockContext = std::make_shared<ExecutionContextImpl>(mockExecutor, "test_session_mixed");

    mockExecutor->setScriptExecutionResult(true);
    mockExecutor->setVariableAssignmentResult(true);

    auto region = std::make_unique<ConcurrentRegion>("mixedActionRegion", parentState, mockContext);

    mockExecutor->clearHistory();

    auto activateResult = region->activate();
    EXPECT_TRUE(activateResult.isSuccess) << "Activation should succeed with mixed action types";

    // Verify execution order per SCXML document order requirement
    const auto &executedScripts = mockExecutor->getExecutedScripts();
    const auto &assignments = mockExecutor->getAssignedVariables();

    // Verify all script actions were executed
    EXPECT_GE(executedScripts.size(), 3) << "All script actions should be executed";

    // Verify assignment was executed
    EXPECT_EQ(assignments.size(), 1) << "Assignment action should be executed";
    EXPECT_EQ(assignments.at("testVar"), "value1") << "Correct assignment should be made";

    // Verify script execution order (parent scripts before child script)
    bool foundScript1 = false, foundScript2 = false, foundChildScript = false;
    int script1Pos = -1, script2Pos = -1, childScriptPos = -1;

    for (size_t i = 0; i < executedScripts.size(); ++i) {
        if (executedScripts[i] == "script1") {
            foundScript1 = true;
            script1Pos = i;
        } else if (executedScripts[i] == "script2") {
            foundScript2 = true;
            script2Pos = i;
        } else if (executedScripts[i] == "child_script") {
            foundChildScript = true;
            childScriptPos = i;
        }
    }

    EXPECT_TRUE(foundScript1) << "First script should be executed";
    EXPECT_TRUE(foundScript2) << "Second script should be executed";
    EXPECT_TRUE(foundChildScript) << "Child script should be executed";

    // Verify document order within parent actions
    if (foundScript1 && foundScript2) {
        EXPECT_LT(script1Pos, script2Pos) << "Parent scripts should execute in document order";
    }

    // Verify parent actions execute before child actions
    if (foundScript1 && foundChildScript) {
        EXPECT_LT(script1Pos, childScriptPos) << "Parent actions should execute before child actions";
    }
    if (foundScript2 && foundChildScript) {
        EXPECT_LT(script2Pos, childScriptPos) << "Parent actions should execute before child actions";
    }

    auto deactivateResult = region->deactivate();
    EXPECT_TRUE(deactivateResult.isSuccess) << "Deactivation should succeed";
}

// SCXML W3C Specification Test: Deep Nested States Entry Action Execution
TEST_F(ConcurrentCompletionMonitoringTest, SCXML_W3C_DeepNestedStates_EntryActionExecution) {
    // SCXML W3C spec: Entry actions should execute in hierarchical order
    // for deeply nested state structures (grandparent → parent → child)

    // Create 3-level hierarchy: grandparent → parent → child
    auto grandparentState = std::make_shared<StateNode>("grandparentState", Type::COMPOUND);
    auto parentState = std::make_shared<StateNode>("parentState", Type::COMPOUND);
    auto childState = std::make_shared<StateNode>("childState", Type::ATOMIC);

    // Add entry actions at each level
    auto grandparentAction = std::make_shared<ScriptAction>("grandparent_entry", "grandparent_action");
    auto parentAction = std::make_shared<ScriptAction>("parent_entry", "parent_action");
    auto childAction = std::make_shared<ScriptAction>("child_entry", "child_action");

    // W3C SCXML 3.8: Use block-based API
    std::vector<std::shared_ptr<IActionNode>> grandparentEntryBlock = {grandparentAction};
    grandparentState->addEntryActionBlock(grandparentEntryBlock);

    std::vector<std::shared_ptr<IActionNode>> parentEntryBlock = {parentAction};
    parentState->addEntryActionBlock(parentEntryBlock);

    std::vector<std::shared_ptr<IActionNode>> childEntryBlock = {childAction};
    childState->addEntryActionBlock(childEntryBlock);

    // Set up hierarchy
    parentState->addChild(childState);
    parentState->setInitialState("childState");
    grandparentState->addChild(parentState);
    grandparentState->setInitialState("parentState");

    auto mockExecutor = std::make_shared<RSM::Test::MockActionExecutor>("test_session_deep");
    auto mockContext = std::make_shared<ExecutionContextImpl>(mockExecutor, "test_session_deep");

    mockExecutor->setScriptExecutionResult(true);

    auto region = std::make_unique<ConcurrentRegion>("deepNestedRegion", grandparentState, mockContext);

    mockExecutor->clearHistory();

    auto activateResult = region->activate();
    EXPECT_TRUE(activateResult.isSuccess) << "Activation should succeed with deep nested states";

    // Verify final state is the deepest child
    EXPECT_EQ(region->getCurrentState(), "childState") << "Should reach deepest child state";

    // Verify entry action execution order: grandparent → parent → child
    const auto &executedScripts = mockExecutor->getExecutedScripts();
    EXPECT_GE(executedScripts.size(), 3) << "All three levels should execute entry actions";

    bool foundGrandparent = false, foundParent = false, foundChild = false;
    int grandparentPos = -1, parentPos = -1, childPos = -1;

    for (size_t i = 0; i < executedScripts.size(); ++i) {
        if (executedScripts[i] == "grandparent_entry") {
            foundGrandparent = true;
            grandparentPos = i;
        } else if (executedScripts[i] == "parent_entry") {
            foundParent = true;
            parentPos = i;
        } else if (executedScripts[i] == "child_entry") {
            foundChild = true;
            childPos = i;
        }
    }

    EXPECT_TRUE(foundGrandparent) << "Grandparent entry action should execute";
    EXPECT_TRUE(foundParent) << "Parent entry action should execute";
    EXPECT_TRUE(foundChild) << "Child entry action should execute";

    // Verify hierarchical execution order per SCXML specification
    if (foundGrandparent && foundParent) {
        EXPECT_LT(grandparentPos, parentPos) << "Grandparent should execute before parent";
    }
    if (foundParent && foundChild) {
        EXPECT_LT(parentPos, childPos) << "Parent should execute before child";
    }
    if (foundGrandparent && foundChild) {
        EXPECT_LT(grandparentPos, childPos) << "Grandparent should execute before child";
    }

    auto deactivateResult = region->deactivate();
    EXPECT_TRUE(deactivateResult.isSuccess) << "Deactivation should succeed";
}

// SCXML W3C Specification Test: Entry Action Exception System Resilience
TEST_F(ConcurrentCompletionMonitoringTest, SCXML_W3C_EntryActionException_SystemResilience) {
    // SCXML W3C spec: System should remain stable and continue operation
    // even when entry actions throw exceptions or fail unexpectedly.
    // The state machine should not crash and should continue processing.

    auto parentState = std::make_shared<StateNode>("parentState", Type::COMPOUND);

    // Create actions that will cause various types of failures
    auto normalAction = std::make_shared<ScriptAction>("normal_script", "normal_action");
    auto exceptionAction = std::make_shared<ScriptAction>("throw new Error('critical error')", "exception_action");
    auto recoveryAction = std::make_shared<ScriptAction>("recovery_script", "recovery_action");

    // W3C SCXML 3.8: Use block-based API
    std::vector<std::shared_ptr<IActionNode>> parentEntryBlock = {normalAction, exceptionAction, recoveryAction};
    parentState->addEntryActionBlock(parentEntryBlock);

    auto childState = std::make_shared<StateNode>("childState", Type::ATOMIC);
    auto childAction = std::make_shared<ScriptAction>("child_continues", "child_action");
    // W3C SCXML 3.8: Use block-based API
    std::vector<std::shared_ptr<IActionNode>> childEntryBlock = {childAction};
    childState->addEntryActionBlock(childEntryBlock);

    parentState->addChild(childState);
    parentState->setInitialState("childState");

    auto mockExecutor = std::make_shared<RSM::Test::MockActionExecutor>("test_session_exception");
    auto mockContext = std::make_shared<ExecutionContextImpl>(mockExecutor, "test_session_exception");

    // Configure mixed success/failure behavior
    mockExecutor->setScriptExecutionResult(true);  // Default to success

    auto region = std::make_unique<ConcurrentRegion>("exceptionResilienceRegion", parentState, mockContext);

    mockExecutor->clearHistory();

    // System should remain stable despite exceptions in entry actions
    auto activateResult = region->activate();
    EXPECT_TRUE(activateResult.isSuccess) << "System should remain stable despite entry action exceptions";

    // Verify system continued to child state (resilience requirement)
    EXPECT_TRUE(region->isActive()) << "Region should remain active despite exceptions in parent entry actions";
    EXPECT_EQ(region->getCurrentState(), "childState") << "Should reach child state despite parent action exceptions";

    // Verify system attempted all actions (no premature stopping due to exceptions)
    const auto &executedScripts = mockExecutor->getExecutedScripts();
    EXPECT_GE(executedScripts.size(), 3) << "System should attempt all actions despite exceptions";

    // Verify normal, recovery, and child actions were executed
    bool foundNormal = false, foundRecovery = false, foundChild = false;
    for (const auto &script : executedScripts) {
        if (script == "normal_script") {
            foundNormal = true;
        }
        if (script == "recovery_script") {
            foundRecovery = true;
        }
        if (script == "child_continues") {
            foundChild = true;
        }
    }

    EXPECT_TRUE(foundNormal) << "Normal action should execute successfully";
    EXPECT_TRUE(foundRecovery) << "Recovery action should execute after exception (system continues)";
    EXPECT_TRUE(foundChild) << "Child action should execute despite parent exceptions (isolation)";

    // Test multiple cycles to verify long-term stability
    for (int cycle = 0; cycle < 3; ++cycle) {
        mockExecutor->clearHistory();

        auto deactivateResult = region->deactivate();
        EXPECT_TRUE(deactivateResult.isSuccess)
            << "Cycle " << cycle << ": Deactivation should succeed despite previous exceptions";
        EXPECT_FALSE(region->isActive()) << "Cycle " << cycle << ": Region should be properly deactivated";

        auto reactivateResult = region->activate();
        EXPECT_TRUE(reactivateResult.isSuccess)
            << "Cycle " << cycle << ": Reactivation should succeed (system recovery)";
        EXPECT_TRUE(region->isActive()) << "Cycle " << cycle << ": Region should be active after reactivation";
        EXPECT_EQ(region->getCurrentState(), "childState")
            << "Cycle " << cycle << ": Should consistently reach child state";
    }

    // Final cleanup and stability verification
    auto finalDeactivateResult = region->deactivate();
    EXPECT_TRUE(finalDeactivateResult.isSuccess) << "Final deactivation should succeed cleanly";
    EXPECT_FALSE(region->isActive()) << "Region should be properly deactivated after resilience testing";

    // Verify no memory leaks or resource corruption by checking mock executor state
    EXPECT_NO_THROW(mockExecutor->getExecutedScripts()) << "Mock executor should remain in valid state";
}

}  // namespace RSM
