#include "scripting/JSEngine.h"
#include <gtest/gtest.h>
#include <iostream>

class JSEngineBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &RSM::JSEngine::instance();
        // JSEngine 리셋으로 테스트 간 격리 보장
        engine_->reset();

        sessionId_ = "js_basic_test_session";
        bool createResult = engine_->createSession(sessionId_, "");
        ASSERT_TRUE(createResult) << "Failed to create JS basic test session";
    }

    void TearDown() override {
        if (engine_) {
            engine_->destroySession(sessionId_);
            engine_->shutdown();
        }
    }

    RSM::JSEngine *engine_;
    std::string sessionId_;
};

TEST_F(JSEngineBasicTest, BasicJavaScriptExecution) {
    // Test basic arithmetic
    auto result = engine_->evaluateExpression(sessionId_, "2 + 3").get();
    ASSERT_TRUE(result.isSuccess()) << "Failed to evaluate expression: " << result.errorMessage;
    EXPECT_EQ(result.getValue<double>(), 5.0);
}

TEST_F(JSEngineBasicTest, VariableAssignmentAndRetrieval) {
    // Assign variable
    auto assignResult = engine_->executeScript(sessionId_, "var testVar = 'Hello World'; testVar").get();
    ASSERT_TRUE(assignResult.isSuccess());
    EXPECT_EQ(assignResult.getValue<std::string>(), "Hello World");

    // Retrieve variable
    auto retrieveResult = engine_->evaluateExpression(sessionId_, "testVar").get();
    ASSERT_TRUE(retrieveResult.isSuccess());
    EXPECT_EQ(retrieveResult.getValue<std::string>(), "Hello World");
}

TEST_F(JSEngineBasicTest, SCXMLBuiltinFunctions) {
    // Test In() function exists
    auto inTypeResult = engine_->evaluateExpression(sessionId_, "typeof In").get();
    ASSERT_TRUE(inTypeResult.isSuccess());
    EXPECT_EQ(inTypeResult.getValue<std::string>(), "function");

    // Test console exists
    auto consoleTypeResult = engine_->evaluateExpression(sessionId_, "typeof console").get();
    ASSERT_TRUE(consoleTypeResult.isSuccess());
    EXPECT_EQ(consoleTypeResult.getValue<std::string>(), "object");

    // Test console.log exists
    auto logTypeResult = engine_->evaluateExpression(sessionId_, "typeof console.log").get();
    ASSERT_TRUE(logTypeResult.isSuccess());
    EXPECT_EQ(logTypeResult.getValue<std::string>(), "function");

    // Test Math exists
    auto mathTypeResult = engine_->evaluateExpression(sessionId_, "typeof Math").get();
    ASSERT_TRUE(mathTypeResult.isSuccess());
    EXPECT_EQ(mathTypeResult.getValue<std::string>(), "object");
}

TEST_F(JSEngineBasicTest, SystemVariables) {
    // Test _sessionid exists and is string
    auto sessionIdTypeResult = engine_->evaluateExpression(sessionId_, "typeof _sessionid").get();
    ASSERT_TRUE(sessionIdTypeResult.isSuccess());
    EXPECT_EQ(sessionIdTypeResult.getValue<std::string>(), "string");

    // Test _name exists and is string
    auto nameTypeResult = engine_->evaluateExpression(sessionId_, "typeof _name").get();
    ASSERT_TRUE(nameTypeResult.isSuccess());
    EXPECT_EQ(nameTypeResult.getValue<std::string>(), "string");

    // Test _ioprocessors exists and is object
    auto ioTypeResult = engine_->evaluateExpression(sessionId_, "typeof _ioprocessors").get();
    ASSERT_TRUE(ioTypeResult.isSuccess());
    EXPECT_EQ(ioTypeResult.getValue<std::string>(), "object");

    // Test _event exists and is object
    auto eventTypeResult = engine_->evaluateExpression(sessionId_, "typeof _event").get();
    ASSERT_TRUE(eventTypeResult.isSuccess());
    EXPECT_EQ(eventTypeResult.getValue<std::string>(), "object");
}

TEST_F(JSEngineBasicTest, ErrorHandling) {
    // Test syntax error handling
    auto syntaxErrorResult = engine_->evaluateExpression(sessionId_, "var x = ;").get();
    EXPECT_FALSE(syntaxErrorResult.isSuccess()) << "Syntax error should be caught";

    // Test reference error handling
    auto refErrorResult = engine_->evaluateExpression(sessionId_, "undefinedVariable").get();
    EXPECT_FALSE(refErrorResult.isSuccess()) << "Reference error should be caught";

    // Test that engine continues to work after errors
    auto workingResult = engine_->evaluateExpression(sessionId_, "1 + 1").get();
    ASSERT_TRUE(workingResult.isSuccess()) << "Engine should continue working after errors";
    EXPECT_EQ(workingResult.getValue<double>(), 2.0);
}

TEST_F(JSEngineBasicTest, ComplexExpressions) {
    // Test complex expression with system variables
    auto complexResult = engine_
                             ->evaluateExpression(sessionId_, "_name.length > 0 && typeof _sessionid === "
                                                              "'string' && Math.max(1, 2) === 2")
                             .get();
    ASSERT_TRUE(complexResult.isSuccess());
    EXPECT_TRUE(complexResult.getValue<bool>());

    // Test function definition and execution
    auto functionResult = engine_
                              ->executeScript(sessionId_, "function factorial(n) { return n <= 1 ? 1 "
                                                          ": n * factorial(n - 1); } factorial(5)")
                              .get();
    ASSERT_TRUE(functionResult.isSuccess());
    EXPECT_EQ(functionResult.getValue<double>(), 120.0);

    // Test object manipulation
    auto objectResult = engine_->executeScript(sessionId_, "var obj = {a: 1, b: {c: 2}}; obj.b.c + obj.a").get();
    ASSERT_TRUE(objectResult.isSuccess());
    EXPECT_EQ(objectResult.getValue<double>(), 3.0);
}

TEST_F(JSEngineBasicTest, ConsoleLogging) {
    // Test console.log functionality
    auto logResult = engine_->executeScript(sessionId_, "console.log('Basic test message'); 'completed'").get();
    ASSERT_TRUE(logResult.isSuccess()) << "console.log should not crash";
    EXPECT_EQ(logResult.getValue<std::string>(), "completed");

    // Test console.log with multiple arguments
    auto multiLogResult = engine_
                              ->executeScript(sessionId_, "console.log('Multiple', 'arguments', 123, "
                                                          "true); 'multi_completed'")
                              .get();
    ASSERT_TRUE(multiLogResult.isSuccess());
    EXPECT_EQ(multiLogResult.getValue<std::string>(), "multi_completed");
}

TEST_F(JSEngineBasicTest, ExpressionValidation) {
    // Test valid expressions
    auto validResult1 = engine_->validateExpression(sessionId_, "1 + 2").get();
    EXPECT_TRUE(validResult1.isSuccess()) << "Simple arithmetic should be valid";

    auto validResult2 = engine_->validateExpression(sessionId_, "Math.max(1, 2)").get();
    EXPECT_TRUE(validResult2.isSuccess()) << "Math function call should be valid";

    auto validResult3 = engine_->validateExpression(sessionId_, "_sessionid.length > 0").get();
    EXPECT_TRUE(validResult3.isSuccess()) << "System variable access should be valid";

    auto validResult4 = engine_->validateExpression(sessionId_, "true && false").get();
    EXPECT_TRUE(validResult4.isSuccess()) << "Boolean expression should be valid";

    auto validResult5 = engine_->validateExpression(sessionId_, "{x: 1, y: 2}").get();
    EXPECT_TRUE(validResult5.isSuccess()) << "Object literal should be valid";

    // Test invalid expressions (syntax errors)
    auto invalidResult1 = engine_->validateExpression(sessionId_, "1 + ").get();
    EXPECT_FALSE(invalidResult1.isSuccess()) << "Incomplete expression should be invalid";

    auto invalidResult2 = engine_->validateExpression(sessionId_, "var x = ;").get();
    EXPECT_FALSE(invalidResult2.isSuccess()) << "Syntax error should be invalid";

    auto invalidResult3 = engine_->validateExpression(sessionId_, "function() {").get();
    EXPECT_FALSE(invalidResult3.isSuccess()) << "Unclosed function should be invalid";

    auto invalidResult4 = engine_->validateExpression(sessionId_, "[1, 2,").get();
    EXPECT_FALSE(invalidResult4.isSuccess()) << "Unclosed array should be invalid";

    // Test edge cases
    auto emptyResult = engine_->validateExpression(sessionId_, "").get();
    EXPECT_FALSE(emptyResult.isSuccess()) << "Empty expression should be invalid";

    auto whitespaceResult = engine_->validateExpression(sessionId_, "   ").get();
    EXPECT_FALSE(whitespaceResult.isSuccess()) << "Whitespace-only expression should be invalid";

    // Test complex valid expressions
    auto complexValid = engine_->validateExpression(sessionId_, "_event.data && _event.data.status === 'ready'").get();
    EXPECT_TRUE(complexValid.isSuccess()) << "Complex event data expression should be valid";
}