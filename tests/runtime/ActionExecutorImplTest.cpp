#include "runtime/ActionExecutorImpl.h"
#include "scripting/JSEngine.h"
#include <future>
#include <gtest/gtest.h>
#include <memory>

using namespace RSM;

class ActionExecutorImplTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize JS engine
        jsEngine = &JSEngine::instance();
        ASSERT_TRUE(jsEngine->initialize());

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
    bool eventRaised = false;
    std::string raisedEventName;
    std::string raisedEventData;

    // Set up callback
    executor->setEventRaiseCallback([&](const std::string &name, const std::string &data) {
        eventRaised = true;
        raisedEventName = name;
        raisedEventData = data;
        return true;  // Simulate successful event raising
    });

    // Raise event without data
    bool result = executor->raiseEvent("test.event");
    EXPECT_TRUE(result);
    EXPECT_TRUE(eventRaised);
    EXPECT_EQ(raisedEventName, "test.event");
    EXPECT_TRUE(raisedEventData.empty());

    // Reset
    eventRaised = false;

    // Raise event with data
    result = executor->raiseEvent("user.login", "{userId: 123}");
    EXPECT_TRUE(result);
    EXPECT_TRUE(eventRaised);
    EXPECT_EQ(raisedEventName, "user.login");
    EXPECT_EQ(raisedEventData, "{userId: 123}");
}

TEST_F(ActionExecutorImplTest, EventRaisingWithoutCallback) {
    // Without callback, event raising should fail
    bool result = executor->raiseEvent("test.event");
    EXPECT_FALSE(result);

    // Empty event name should fail
    executor->setEventRaiseCallback([](const std::string &, const std::string &) { return true; });
    result = executor->raiseEvent("");
    EXPECT_FALSE(result);
}

TEST_F(ActionExecutorImplTest, CurrentEventHandling) {
    // Set current event
    executor->setCurrentEvent("user.action", "{action: 'click'}");

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