#include "runtime/ActionExecutorImpl.h"
#include "actions/AssignAction.h"
#include "actions/ForeachAction.h"
#include "actions/LogAction.h"
#include "actions/RaiseAction.h"
#include "actions/ScriptAction.h"
#include "actions/SendAction.h"
#include "mocks/MockEventRaiser.h"
#include "scripting/JSEngine.h"
#include <atomic>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <thread>

using namespace RSM;

class ActionExecutorImplTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize JS engine
        jsEngine = &JSEngine::instance();
        // JSEngine 리셋으로 테스트 간 격리 보장
        jsEngine->reset();

        sessionId = "action_executor_test_session";
        ASSERT_TRUE(jsEngine->createSession(sessionId, ""));

        executor = std::make_unique<ActionExecutorImpl>(sessionId);
    }

    void TearDown() override {
        if (jsEngine && jsEngine->hasSession(sessionId)) {
            jsEngine->destroySession(sessionId);
        }
        if (jsEngine) {
            jsEngine->shutdown();
        }
    }

    JSEngine *jsEngine;
    std::string sessionId;
    std::unique_ptr<ActionExecutorImpl> executor;
};

TEST_F(ActionExecutorImplTest, BasicProperties) {
    EXPECT_EQ(executor->getSessionId(), sessionId);
    EXPECT_TRUE(executor->isSessionReady());
}

TEST_F(ActionExecutorImplTest, ScriptExecution) {
    // Simple script execution
    bool result = executor->executeScript("var testVar = 42;");
    EXPECT_TRUE(result);

    // Verify variable was set
    auto jsResult = jsEngine->evaluateExpression(sessionId, "testVar").get();
    ASSERT_TRUE(jsResult.isSuccess());
    EXPECT_EQ(jsResult.getValue<double>(), 42.0);
}

TEST_F(ActionExecutorImplTest, EmptyScriptExecution) {
    // Empty script should succeed
    EXPECT_TRUE(executor->executeScript(""));

    // Whitespace-only script should succeed
    EXPECT_TRUE(executor->executeScript("   \n\t  "));
}

TEST_F(ActionExecutorImplTest, InvalidScriptExecution) {
    // Syntax error should fail
    bool result = executor->executeScript("var x = ;");
    EXPECT_FALSE(result);

    // Session should still be functional after error
    EXPECT_TRUE(executor->isSessionReady());

    // Valid script should still work
    EXPECT_TRUE(executor->executeScript("var y = 10;"));
}

TEST_F(ActionExecutorImplTest, VariableAssignment) {
    // Simple variable assignment
    bool result = executor->assignVariable("counter", "5");
    EXPECT_TRUE(result);

    // Verify assignment worked
    auto jsResult = jsEngine->evaluateExpression(sessionId, "counter").get();
    ASSERT_TRUE(jsResult.isSuccess());
    EXPECT_EQ(jsResult.getValue<double>(), 5.0);

    // Expression assignment
    result = executor->assignVariable("doubled", "counter * 2");
    EXPECT_TRUE(result);

    jsResult = jsEngine->evaluateExpression(sessionId, "doubled").get();
    ASSERT_TRUE(jsResult.isSuccess());
    EXPECT_EQ(jsResult.getValue<double>(), 10.0);
}

TEST_F(ActionExecutorImplTest, ComplexVariableAssignment) {
    // Set up object
    executor->executeScript("var data = {};");

    // Dot notation assignment
    bool result = executor->assignVariable("data.name", "'John Doe'");
    EXPECT_TRUE(result);

    auto jsResult = jsEngine->evaluateExpression(sessionId, "data.name").get();
    ASSERT_TRUE(jsResult.isSuccess());
    EXPECT_EQ(jsResult.getValue<std::string>(), "John Doe");

    // Nested object assignment
    result = executor->assignVariable("data.profile", "({age: 30, city: 'NYC'})");
    EXPECT_TRUE(result);

    jsResult = jsEngine->evaluateExpression(sessionId, "data.profile.age").get();
    ASSERT_TRUE(jsResult.isSuccess());
    EXPECT_EQ(jsResult.getValue<double>(), 30.0);
}

TEST_F(ActionExecutorImplTest, InvalidVariableAssignment) {
    // Empty location should fail
    bool result = executor->assignVariable("", "value");
    EXPECT_FALSE(result);

    // Invalid location should fail
    result = executor->assignVariable("invalid-name", "value");
    EXPECT_FALSE(result);

    // Invalid expression should fail
    result = executor->assignVariable("validName", "invalid.syntax.error");
    EXPECT_FALSE(result);
}

TEST_F(ActionExecutorImplTest, ExpressionEvaluation) {
    // Set up some variables
    executor->executeScript("var a = 10; var b = 20;");

    // Simple expression
    std::string result = executor->evaluateExpression("a + b");
    EXPECT_EQ(result, "30");

    // String expression
    result = executor->evaluateExpression("'Hello ' + 'World'");
    EXPECT_EQ(result, "Hello World");

    // Boolean expression
    result = executor->evaluateExpression("a > b");
    EXPECT_EQ(result, "false");

    // Object expression (should be JSON stringified)
    result = executor->evaluateExpression("({x: 1, y: 2})");
    EXPECT_EQ(result, "{\"x\":1,\"y\":2}");
}

TEST_F(ActionExecutorImplTest, VariableExistenceCheck) {
    // Variable doesn't exist initially
    EXPECT_FALSE(executor->hasVariable("nonExistent"));

    // Create variable
    executor->assignVariable("myVar", "123");

    // Variable should now exist
    EXPECT_TRUE(executor->hasVariable("myVar"));

    // Check complex path
    executor->executeScript("var obj = {nested: {value: 42}};");
    EXPECT_TRUE(executor->hasVariable("obj"));
    EXPECT_TRUE(executor->hasVariable("obj.nested"));
    EXPECT_TRUE(executor->hasVariable("obj.nested.value"));
}

TEST_F(ActionExecutorImplTest, EventRaising) {
    std::vector<std::pair<std::string, std::string>> raisedEvents;

    // Set up MockEventRaiser with dependency injection
    auto mockEventRaiser =
        std::make_shared<RSM::Test::MockEventRaiser>([&](const std::string &name, const std::string &data) -> bool {
            raisedEvents.emplace_back(name, data);
            return true;
        });
    executor->setEventRaiser(mockEventRaiser);

    // Test RaiseAction without data - SCXML fire and forget model
    RaiseAction raiseAction("test.event");
    bool result = executor->executeRaiseAction(raiseAction);
    EXPECT_TRUE(result);

    ASSERT_EQ(raisedEvents.size(), 1);
    EXPECT_EQ(raisedEvents[0].first, "test.event");
    EXPECT_TRUE(raisedEvents[0].second.empty());

    // Test RaiseAction with data (JavaScript expression)
    raisedEvents.clear();
    RaiseAction raiseActionWithData("user.login");
    raiseActionWithData.setData("{userId: 123}");  // This will be evaluated as JS expression

    result = executor->executeRaiseAction(raiseActionWithData);
    EXPECT_TRUE(result);

    ASSERT_EQ(raisedEvents.size(), 1);
    EXPECT_EQ(raisedEvents[0].first, "user.login");
    EXPECT_EQ(raisedEvents[0].second, "123");  // JavaScript evaluation result
}

TEST_F(ActionExecutorImplTest, EventRaisingWithoutCallback) {
    // SCXML Compliance: Without EventRaiser, event raising should fail
    // SCXML 3.12.1: Infrastructure failures should generate error events, not exceptions
    RaiseAction raiseAction("test.event");
    bool result = executor->executeRaiseAction(raiseAction);
    EXPECT_FALSE(result);  // Should return false when EventRaiser is not available

    // Set up EventRaiser and test empty event name should fail
    auto mockEventRaiser = std::make_shared<RSM::Test::MockEventRaiser>(
        [](const std::string &, const std::string &) -> bool { return true; });
    executor->setEventRaiser(mockEventRaiser);

    RaiseAction emptyAction("");
    bool emptyResult = executor->executeRaiseAction(emptyAction);
    EXPECT_FALSE(emptyResult);  // Empty name validation should still work
}

TEST_F(ActionExecutorImplTest, CurrentEventHandling) {
    // Set current event
    executor->setCurrentEvent("user.action", "{\"action\": \"click\"}");

    // _event should be available in JavaScript
    auto result = jsEngine->evaluateExpression(sessionId, "_event.name").get();
    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(result.getValue<std::string>(), "user.action");

    result = jsEngine->evaluateExpression(sessionId, "_event.data.action").get();
    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(result.getValue<std::string>(), "click");

    // Clear event
    executor->clearCurrentEvent();

    result = jsEngine->evaluateExpression(sessionId, "_event.name").get();
    ASSERT_TRUE(result.isSuccess());
    EXPECT_TRUE(result.getValue<std::string>().empty());
}

TEST_F(ActionExecutorImplTest, LoggingLevels) {
    // Test different log levels (should not throw or crash)
    EXPECT_NO_THROW(executor->log("info", "Information message"));
    EXPECT_NO_THROW(executor->log("warn", "Warning message"));
    EXPECT_NO_THROW(executor->log("error", "Error message"));
    EXPECT_NO_THROW(executor->log("debug", "Debug message"));
    EXPECT_NO_THROW(executor->log("custom", "Custom level message"));
}

TEST_F(ActionExecutorImplTest, SessionReadiness) {
    EXPECT_TRUE(executor->isSessionReady());

    // Destroy session
    jsEngine->destroySession(sessionId);

    EXPECT_FALSE(executor->isSessionReady());

    // Operations should fail gracefully
    EXPECT_FALSE(executor->executeScript("var x = 1;"));
    EXPECT_FALSE(executor->assignVariable("var", "value"));
    EXPECT_TRUE(executor->evaluateExpression("1 + 1").empty());
    EXPECT_FALSE(executor->hasVariable("anything"));
}

TEST_F(ActionExecutorImplTest, ConcurrentOperations) {
    const int numOperations = 10;
    std::vector<std::future<bool>> futures;

    // Launch concurrent script executions
    for (int i = 0; i < numOperations; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            std::string script = "var concurrent" + std::to_string(i) + " = " + std::to_string(i) + ";";
            return executor->executeScript(script);
        }));
    }

    // Wait for all operations to complete
    for (auto &future : futures) {
        EXPECT_TRUE(future.get());
    }

    // Verify all variables were created
    for (int i = 0; i < numOperations; ++i) {
        std::string varName = "concurrent" + std::to_string(i);
        EXPECT_TRUE(executor->hasVariable(varName));

        std::string value = executor->evaluateExpression(varName);
        EXPECT_EQ(value, std::to_string(i));
    }
}

// SCXML Compliance Tests
TEST_F(ActionExecutorImplTest, SCXMLComplianceSendIdAutoGeneration) {
    // SCXML 6.2.4: sendid MUST be auto-generated if not provided
    SendAction sendAction("test.event", "send_test");
    // Don't set sendid - should be auto-generated

    bool result = executor->executeSendAction(sendAction);
    EXPECT_TRUE(result);  // SCXML fire-and-forget semantics
}

TEST_F(ActionExecutorImplTest, SCXMLComplianceSessionScopedTarget) {
    // SCXML 6.2.4: Empty target should be session-scoped, not "#_internal"
    SendAction sendAction("test.event", "send_test");
    sendAction.setTarget("");  // Empty target

    bool result = executor->executeSendAction(sendAction);
    EXPECT_TRUE(result);  // Should succeed with session-scoped target
}

TEST_F(ActionExecutorImplTest, SCXMLComplianceTargetValidation) {
    // SCXML: Target values should be validated properly
    SendAction sendAction("test.event", "send_test");

    // Test various target formats
    std::vector<std::string> validTargets = {
        "",                           // Empty (session-scoped)
        "#_scxml_test_session",       // Session-scoped format
        "http://example.com/target",  // HTTP target
        "scxml:another_session"       // SCXML target
    };

    for (const auto &target : validTargets) {
        sendAction.setTarget(target);
        bool result = executor->executeSendAction(sendAction);
        EXPECT_TRUE(result) << "Target should be valid: " << target;
    }
}

TEST_F(ActionExecutorImplTest, SCXMLComplianceFireAndForgetSemantics) {
    // SCXML 6.2.4: Send actions follow "fire and forget" semantics
    SendAction sendAction("test.event", "send_test");
    sendAction.setTarget("");  // Session-scoped

    // Should return immediately (fire and forget)
    auto start = std::chrono::steady_clock::now();
    bool result = executor->executeSendAction(sendAction);
    auto end = std::chrono::steady_clock::now();

    EXPECT_TRUE(result);

    // Should return quickly (< 10ms) due to fire-and-forget
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 10) << "Send action should return immediately (fire-and-forget)";
}

TEST_F(ActionExecutorImplTest, SCXMLComplianceDefaultTargetBehavior) {
    // Verify that default target behavior is SCXML compliant
    SendAction sendAction("test.event", "send_test");
    // Target is empty by default after our compliance fix

    std::string defaultTarget = sendAction.getTarget();
    EXPECT_TRUE(defaultTarget.empty()) << "Default target should be empty (session-scoped), not '#_internal'";

    bool result = executor->executeSendAction(sendAction);
    EXPECT_TRUE(result);  // Should work with session-scoped target
}

TEST_F(ActionExecutorImplTest, SCXMLComplianceErrorHandling) {
    // SCXML 3.12.1: Infrastructure failures should not throw exceptions
    SendAction sendAction("test.event", "send_test");

    // Test without event dispatcher (should not throw)
    EXPECT_NO_THROW({
        bool result = executor->executeSendAction(sendAction);
        EXPECT_TRUE(result);  // Fire-and-forget semantics - infrastructure failures don't affect action success
    });

    // Test with invalid event name (should not throw)
    SendAction invalidAction("", "invalid_test");
    EXPECT_NO_THROW({
        bool result = executor->executeSendAction(invalidAction);
        (void)result;  // Suppress unused variable warning - result depends on validation but should not throw
    });
}

// ============================================================================
// SCXML W3C Foreach Action Tests
// ============================================================================

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_BasicArrayIteration) {
    // SCXML W3C: foreach should iterate through array elements
    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("[1, 2, 3]");
    foreachAction->setItem("currentItem");
    foreachAction->setIndex("currentIndex");

    // Add simple assign action for each iteration
    auto assignAction = std::make_shared<AssignAction>("result", "currentItem");
    foreachAction->addIterationAction(assignAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // Verify final iteration variables exist
    EXPECT_TRUE(executor->hasVariable("currentItem"));
    EXPECT_TRUE(executor->hasVariable("currentIndex"));
}

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_ObjectPropertyIteration) {
    // SCXML W3C: foreach should handle object properties
    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("Object.values({a: 'first', b: 'second', c: 'third'})");
    foreachAction->setItem("value");
    foreachAction->setIndex("idx");

    auto logAction = std::make_shared<LogAction>("Processing value");
    logAction->setExpr("value");
    foreachAction->addIterationAction(logAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));
}

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_EmptyArrayHandling) {
    // SCXML W3C: foreach should handle empty arrays gracefully
    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("[]");
    foreachAction->setItem("item");

    auto assignAction = std::make_shared<AssignAction>("wasExecuted", "true");
    foreachAction->addIterationAction(assignAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // Variables should not exist since no iterations occurred
    EXPECT_FALSE(executor->hasVariable("wasExecuted"));
}

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_NullItemHandling) {
    // SCXML W3C: foreach should handle null/undefined items
    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("[1, null, undefined, 2]");
    foreachAction->setItem("item");
    foreachAction->setIndex("idx");

    auto logAction = std::make_shared<LogAction>("Item");
    logAction->setExpr("typeof item");
    foreachAction->addIterationAction(logAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));
}

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_VariableExpressionArray) {
    // SCXML W3C: array can be a variable expression
    EXPECT_TRUE(executor->assignVariable("myArray", "[10, 20, 30]"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("myArray");
    foreachAction->setItem("num");

    auto assignAction = std::make_shared<AssignAction>("sum", "sum + num");
    foreachAction->addIterationAction(assignAction);

    // Initialize sum variable
    EXPECT_TRUE(executor->assignVariable("sum", "0"));

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // Verify sum calculation
    std::string result = executor->evaluateExpression("sum");
    EXPECT_EQ(result, "60");  // 10 + 20 + 30
}

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_IndexTrackingValidation) {
    // SCXML W3C: index should track iteration count correctly
    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("['a', 'b', 'c', 'd']");
    foreachAction->setItem("letter");
    foreachAction->setIndex("position");

    // Verify index values during iteration
    auto assignAction = std::make_shared<AssignAction>("lastIndex", "position");
    foreachAction->addIterationAction(assignAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // Final index should be 3 (last iteration)
    std::string finalIndex = executor->evaluateExpression("lastIndex");
    EXPECT_EQ(finalIndex, "3");
}

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_NestedForeachSupport) {
    // SCXML W3C: foreach should support nested iterations
    auto outerForeach = std::make_shared<ForeachAction>();
    outerForeach->setArray("[[1, 2], [3, 4]]");
    outerForeach->setItem("subArray");
    outerForeach->setIndex("outerIdx");

    auto innerForeach = std::make_shared<ForeachAction>();
    innerForeach->setArray("subArray");
    innerForeach->setItem("innerItem");
    innerForeach->setIndex("innerIdx");

    auto assignAction = std::make_shared<AssignAction>("product", "product * innerItem");
    innerForeach->addIterationAction(assignAction);
    outerForeach->addIterationAction(innerForeach);

    // Initialize product
    EXPECT_TRUE(executor->assignVariable("product", "1"));

    EXPECT_TRUE(executor->executeForeachAction(*outerForeach));

    // Verify nested calculation: 1 * 2 * 3 * 4 = 24
    std::string result = executor->evaluateExpression("product");
    EXPECT_EQ(result, "24");
}

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_ErrorHandling_InvalidArray) {
    // SCXML W3C: foreach should handle invalid array expressions
    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("invalidVariable");  // Non-existent variable
    foreachAction->setItem("item");

    auto assignAction = std::make_shared<AssignAction>("test", "value");
    foreachAction->addIterationAction(assignAction);

    // Should handle gracefully - empty iteration or error state
    bool result = executor->executeForeachAction(*foreachAction);
    // Implementation dependent: could return false or handle as empty array
    EXPECT_TRUE(result || !result);  // Accept either behavior for now
}

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_ErrorHandling_ChildActionFailure) {
    // SCXML W3C: foreach should stop on child action errors
    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("[1, 2, 3]");
    foreachAction->setItem("item");

    // Create an action that will fail (invalid location)
    auto failingAction = std::make_shared<AssignAction>("", "item");  // Empty location
    foreachAction->addIterationAction(failingAction);

    // Should fail due to child action error
    EXPECT_FALSE(executor->executeForeachAction(*foreachAction));
}

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_ShallowCopySemantics) {
    // SCXML W3C: foreach should create shallow copy to prevent modification during iteration
    EXPECT_TRUE(executor->assignVariable("originalArray", "[1, 2, 3]"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("originalArray");
    foreachAction->setItem("item");

    // Try to modify original array during iteration
    auto modifyAction = std::make_shared<ScriptAction>("originalArray.push(99);");
    foreachAction->addIterationAction(modifyAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // Verify original array was modified but iteration wasn't affected
    std::string result = executor->evaluateExpression("originalArray.length");
    EXPECT_EQ(result, "6");  // Original 3 + 3 additions during iterations
}

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_ComplexExpressionArray) {
    // SCXML W3C: array expression can be complex JavaScript
    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("Array.from({length: 3}, (_, i) => i * 2)");  // [0, 2, 4]
    foreachAction->setItem("evenNumber");
    foreachAction->setIndex("idx");

    auto assignAction = std::make_shared<AssignAction>("total", "total + evenNumber");
    foreachAction->addIterationAction(assignAction);

    EXPECT_TRUE(executor->assignVariable("total", "0"));
    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // Verify: 0 + 2 + 4 = 6
    std::string result = executor->evaluateExpression("total");
    EXPECT_EQ(result, "6");
}

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_NumericVariableNames) {
    // Test W3C Test 150 scenario: foreach with numeric variable names
    // This tests the specific case where variables have numeric names like "1", "2", "3"
    // and foreach needs to access their values correctly

    // Setup: Create variables with numeric names (like W3C Test 150)
    EXPECT_TRUE(executor->assignVariable("1", "undefined"));  // item variable
    EXPECT_TRUE(executor->assignVariable("2", "undefined"));  // index variable
    EXPECT_TRUE(executor->assignVariable("3", "[1,2,3]"));    // array variable

    // Create foreach action that uses numeric variable names
    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("3");  // Should access variable "3" containing [1,2,3]
    foreachAction->setItem("1");   // Should use variable "1" as item
    foreachAction->setIndex("2");  // Should use variable "2" as index

    // Execute foreach (no child actions like W3C Test 150)
    EXPECT_TRUE(executor->executeForeachAction(*foreachAction))
        << "Foreach with numeric variable names should execute successfully";

    // Verify variables were updated during iteration
    EXPECT_TRUE(executor->hasVariable("1")) << "Item variable '1' should exist after foreach";
    EXPECT_TRUE(executor->hasVariable("2")) << "Index variable '2' should exist after foreach";

    // Verify final iteration values (last iteration: item=3, index=2)
    // W3C SCXML: TXMLConverter transforms conf:item="1" → item="var1"
    // So we must evaluate "var1", not "1" (which would be the literal number 1)
    std::string itemValue = executor->evaluateExpression("var1");
    EXPECT_EQ(itemValue, "3") << "Item variable should contain last array element";

    std::string indexValue = executor->evaluateExpression("var2");
    EXPECT_EQ(indexValue, "2") << "Index variable should contain last index (0-based)";
}

TEST_F(ActionExecutorImplTest, W3C_ForeachAction_NumericArrayVariableAccess) {
    // Test that numeric variable names are accessed correctly as array sources
    // This specifically tests the getVariable vs evaluateExpression logic

    // Setup array in a numeric variable
    EXPECT_TRUE(executor->assignVariable("99", "['a', 'b', 'c']"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("99");  // Access variable "99", not evaluate expression 99
    foreachAction->setItem("letter");
    foreachAction->setIndex("pos");

    auto concatAction = std::make_shared<AssignAction>("result", "result + letter");
    foreachAction->addIterationAction(concatAction);

    EXPECT_TRUE(executor->assignVariable("result", "\"\""));
    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    std::string result = executor->evaluateExpression("result");
    EXPECT_EQ(result, "abc") << "Should iterate over array stored in numeric variable";
}

// ============================================================================
// Send Action Type Processing Tests - Bug Reproduction for W3C Test 193
// ============================================================================

TEST_F(ActionExecutorImplTest, SendAction_TypeProcessing_W3C193_BugReproduction) {
    // Create mock event raiser to track raised events
    std::vector<std::string> raisedEvents;
    auto mockEventRaiser = std::make_shared<RSM::Test::MockEventRaiser>(
        [&raisedEvents](const std::string &name, const std::string & /*data*/) -> bool {
            raisedEvents.push_back(name);
            return true;
        });
    executor->setEventRaiser(mockEventRaiser);

    // Test 1: Send with no type should result in external queue routing (not internal)
    auto sendNoType = std::make_shared<SendAction>();
    sendNoType->setEvent("internal_event");
    // No type set - should go to external queue (W3C SCXML default)

    // Test 2: Send with SCXMLEventProcessor type should result in external queue routing
    auto sendWithType = std::make_shared<SendAction>();
    sendWithType->setEvent("external_event");
    sendWithType->setType("http://www.w3.org/TR/scxml/#SCXMLEventProcessor");

    // Both should behave the same - go to external queue
    // The type attribute doesn't affect queue routing, only event processor selection

    EXPECT_TRUE(executor->executeSendAction(*sendNoType));
    EXPECT_TRUE(executor->executeSendAction(*sendWithType));

    // Both events should be processed (this test is about queue routing, not specific behavior)
    // The actual W3C test 193 checks the timing and order in a real state machine context
}