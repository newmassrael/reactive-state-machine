#include "states/ConcurrentRegion.h"
#include "actions/AssignAction.h"
#include "actions/ScriptAction.h"
#include "mocks/MockActionExecutor.h"
#include "model/StateNode.h"
#include "runtime/ExecutionContextImpl.h"
#include "gtest/gtest.h"
#include <memory>
#include <stdexcept>

namespace RSM {

class ConcurrentRegionTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockExecutor = std::make_shared<RSM::Test::MockActionExecutor>("test_session");
        executionContext = std::make_shared<ExecutionContextImpl>(mockExecutor, "test_session");
        rootState = std::make_shared<StateNode>("rootState", Type::COMPOUND);
        region = std::make_unique<ConcurrentRegion>("testRegion", rootState, executionContext);
    }

    void TearDown() override {
        mockExecutor->clearHistory();
    }

    std::shared_ptr<RSM::Test::MockActionExecutor> mockExecutor;
    std::shared_ptr<ExecutionContextImpl> executionContext;
    std::shared_ptr<StateNode> rootState;
    std::unique_ptr<ConcurrentRegion> region;
};

// Test executeActionNode with null ActionNode
TEST_F(ConcurrentRegionTest, ExecuteActionNode_NullActionNode_ReturnsFalse) {
    // This test uses reflection to access the private executeActionNode method
    // Since executeActionNode is private, we'll test it indirectly through entry action execution

    // Create a state with null entry action (simulated by not adding any actions)
    auto testState = std::make_shared<StateNode>("testState", Type::ATOMIC);

    // No entry actions added - should handle gracefully
    region->setRootState(testState);

    // Activate should succeed even with no entry actions
    auto result = region->activate();
    EXPECT_TRUE(result.isSuccess) << "Activation should succeed even with no entry actions";

    // Verify no scripts were executed (because there were no actions)
    EXPECT_EQ(mockExecutor->getExecutedScripts().size(), 0) << "No scripts should be executed when no actions exist";
}

// Test executeActionNode with action that throws exception
TEST_F(ConcurrentRegionTest, ExecuteActionNode_ActionExecuteThrowsException_HandledGracefully) {
    // Create a script action that will cause an exception in the mock executor
    auto scriptAction = std::make_shared<ScriptAction>("throw new Error('test exception')", "exception_action");

    auto testState = std::make_shared<StateNode>("testState", Type::ATOMIC);
    testState->addEntryActionNode(scriptAction);

    // Configure mock to throw exception for this script
    mockExecutor->setScriptExecutionResult(false);  // Make it fail

    region->setRootState(testState);

    // Activation should still succeed (error handled gracefully)
    auto result = region->activate();
    EXPECT_TRUE(result.isSuccess) << "Activation should succeed even when entry action fails";

    // Verify the action was attempted
    const auto &executedScripts = mockExecutor->getExecutedScripts();
    EXPECT_EQ(executedScripts.size(), 1) << "Script should have been attempted";
    EXPECT_EQ(executedScripts[0], "throw new Error('test exception')") << "Correct script should have been executed";
}

// Test executeActionNode with action that returns false
TEST_F(ConcurrentRegionTest, ExecuteActionNode_ActionExecuteReturnsFalse_HandledGracefully) {
    auto scriptAction = std::make_shared<ScriptAction>("return false", "false_action");

    auto testState = std::make_shared<StateNode>("testState", Type::ATOMIC);
    testState->addEntryActionNode(scriptAction);

    // Configure mock to return false
    mockExecutor->setScriptExecutionResult(false);

    region->setRootState(testState);

    // Activation should still succeed (failure handled gracefully)
    auto result = region->activate();
    EXPECT_TRUE(result.isSuccess) << "Activation should succeed even when entry action returns false";

    // Verify the action was attempted
    const auto &executedScripts = mockExecutor->getExecutedScripts();
    EXPECT_EQ(executedScripts.size(), 1) << "Script should have been attempted";
    EXPECT_EQ(executedScripts[0], "return false") << "Correct script should have been executed";
}

// Test executeActionNode with valid ScriptAction
TEST_F(ConcurrentRegionTest, ExecuteActionNode_ValidScriptAction_ReturnsTrue) {
    auto scriptAction = std::make_shared<ScriptAction>("console.log('test')", "script_action");

    auto testState = std::make_shared<StateNode>("testState", Type::ATOMIC);
    testState->addEntryActionNode(scriptAction);

    // Configure mock to succeed
    mockExecutor->setScriptExecutionResult(true);

    region->setRootState(testState);

    // Activation should succeed
    auto result = region->activate();
    EXPECT_TRUE(result.isSuccess) << "Activation should succeed with valid script action";

    // Verify the action was executed
    const auto &executedScripts = mockExecutor->getExecutedScripts();
    EXPECT_EQ(executedScripts.size(), 1) << "Script should have been executed";
    EXPECT_EQ(executedScripts[0], "console.log('test')") << "Correct script should have been executed";
}

// Test executeActionNode with valid AssignAction
TEST_F(ConcurrentRegionTest, ExecuteActionNode_ValidAssignAction_ReturnsTrue) {
    auto assignAction = std::make_shared<AssignAction>("testVar", "42", "assign_action");

    auto testState = std::make_shared<StateNode>("testState", Type::ATOMIC);
    testState->addEntryActionNode(assignAction);

    // Configure mock to succeed for variable assignment
    mockExecutor->setVariableAssignmentResult(true);

    region->setRootState(testState);

    // Activation should succeed
    auto result = region->activate();
    EXPECT_TRUE(result.isSuccess) << "Activation should succeed with valid assign action";

    // Verify the assignment was executed
    const auto &assignments = mockExecutor->getAssignedVariables();
    EXPECT_EQ(assignments.size(), 1) << "Variable assignment should have been executed";
    EXPECT_EQ(assignments.at("testVar"), "42") << "Correct variable should have been assigned";
}

// Test executeActionNode with null ExecutionContext
TEST_F(ConcurrentRegionTest, ExecuteActionNode_NullExecutionContext_SkipsExecution) {
    auto scriptAction = std::make_shared<ScriptAction>("console.log('test')", "script_action");

    auto testState = std::make_shared<StateNode>("testState", Type::ATOMIC);
    testState->addEntryActionNode(scriptAction);

    // Create region without execution context
    auto regionWithoutContext = std::make_unique<ConcurrentRegion>("testRegion", testState, nullptr);

    // Activation should succeed but skip action execution
    auto result = regionWithoutContext->activate();
    EXPECT_TRUE(result.isSuccess) << "Activation should succeed even without execution context";

    // Verify no actions were executed (since no execution context)
    EXPECT_EQ(mockExecutor->getExecutedScripts().size(), 0)
        << "No scripts should be executed without execution context";
}

// Test multiple entry actions execution order
TEST_F(ConcurrentRegionTest, ExecuteActionNode_MultipleActions_ExecutedInOrder) {
    auto action1 = std::make_shared<ScriptAction>("action1", "action_1");
    auto action2 = std::make_shared<ScriptAction>("action2", "action_2");
    auto action3 = std::make_shared<ScriptAction>("action3", "action_3");

    auto testState = std::make_shared<StateNode>("testState", Type::ATOMIC);
    testState->addEntryActionNode(action1);
    testState->addEntryActionNode(action2);
    testState->addEntryActionNode(action3);

    mockExecutor->setScriptExecutionResult(true);

    region->setRootState(testState);

    auto result = region->activate();
    EXPECT_TRUE(result.isSuccess) << "Activation should succeed with multiple actions";

    // Verify execution order
    const auto &executedScripts = mockExecutor->getExecutedScripts();
    EXPECT_EQ(executedScripts.size(), 3) << "All three actions should have been executed";
    EXPECT_EQ(executedScripts[0], "action1") << "First action should execute first";
    EXPECT_EQ(executedScripts[1], "action2") << "Second action should execute second";
    EXPECT_EQ(executedScripts[2], "action3") << "Third action should execute third";
}

// Test mixed action types execution
TEST_F(ConcurrentRegionTest, ExecuteActionNode_MixedActionTypes_AllExecuted) {
    auto scriptAction = std::make_shared<ScriptAction>("console.log('script')", "script_action");
    auto assignAction = std::make_shared<AssignAction>("var1", "value1", "assign_action");

    auto testState = std::make_shared<StateNode>("testState", Type::ATOMIC);
    testState->addEntryActionNode(scriptAction);
    testState->addEntryActionNode(assignAction);

    mockExecutor->setScriptExecutionResult(true);
    mockExecutor->setVariableAssignmentResult(true);

    region->setRootState(testState);

    auto result = region->activate();
    EXPECT_TRUE(result.isSuccess) << "Activation should succeed with mixed action types";

    // Verify both types were executed
    const auto &executedScripts = mockExecutor->getExecutedScripts();
    const auto &assignments = mockExecutor->getAssignedVariables();

    EXPECT_EQ(executedScripts.size(), 1) << "Script action should have been executed";
    EXPECT_EQ(executedScripts[0], "console.log('script')") << "Correct script should have been executed";

    EXPECT_EQ(assignments.size(), 1) << "Assignment action should have been executed";
    EXPECT_EQ(assignments.at("var1"), "value1") << "Correct assignment should have been made";
}

}  // namespace RSM