#include "scripting/JSEngine.h"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

namespace RSM {
namespace Tests {

class SimpleSCXMLTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        // JSEngine 리셋으로 테스트 간 격리 보장
        engine_->reset();

        // Create test session
        sessionId_ = "test_session";
        bool success = engine_->createSession(sessionId_, "");
        ASSERT_TRUE(success) << "Failed to create session";
    }

    void TearDown() override {
        if (engine_) {
            engine_->destroySession(sessionId_);
            engine_->shutdown();
        }
    }

    JSEngine *engine_;
    std::string sessionId_;
};

// Test basic JavaScript execution
TEST_F(SimpleSCXMLTest, BasicExecution) {
    auto result = engine_->evaluateExpression(sessionId_, "1 + 1").get();
    ASSERT_TRUE(result.isSuccess()) << "Failed to evaluate 1+1";

    auto value = result.getValue<double>();
    EXPECT_EQ(value, 2.0);
}

// Test SCXML system variables exist
TEST_F(SimpleSCXMLTest, SystemVariablesExist) {
    // Test _sessionid exists
    auto sessionIdResult = engine_->evaluateExpression(sessionId_, "typeof _sessionid").get();
    ASSERT_TRUE(sessionIdResult.isSuccess());
    auto sessionIdType = sessionIdResult.getValue<std::string>();
    EXPECT_EQ(sessionIdType, "string");

    // Test _name exists
    auto nameResult = engine_->evaluateExpression(sessionId_, "typeof _name").get();
    ASSERT_TRUE(nameResult.isSuccess());
    auto nameType = nameResult.getValue<std::string>();
    EXPECT_EQ(nameType, "string");

    // Test _ioprocessors exists
    auto ioResult = engine_->evaluateExpression(sessionId_, "typeof _ioprocessors").get();
    ASSERT_TRUE(ioResult.isSuccess());
    auto ioType = ioResult.getValue<std::string>();
    EXPECT_EQ(ioType, "object");
}

// Test _event object
TEST_F(SimpleSCXMLTest, EventObject) {
    auto eventResult = engine_->evaluateExpression(sessionId_, "typeof _event").get();
    ASSERT_TRUE(eventResult.isSuccess());
    auto eventType = eventResult.getValue<std::string>();
    EXPECT_EQ(eventType, "object");

    // Test _event has required properties
    auto hasNameResult = engine_->evaluateExpression(sessionId_, "_event.hasOwnProperty('name')").get();
    ASSERT_TRUE(hasNameResult.isSuccess());
    auto hasName = hasNameResult.getValue<bool>();
    EXPECT_TRUE(hasName);
}

// Test In() function
TEST_F(SimpleSCXMLTest, InFunction) {
    auto inTypeResult = engine_->evaluateExpression(sessionId_, "typeof In").get();
    ASSERT_TRUE(inTypeResult.isSuccess());
    auto inType = inTypeResult.getValue<std::string>();
    EXPECT_EQ(inType, "function");

    // Test In() returns false (since no state machine is connected)
    auto inCallResult = engine_->evaluateExpression(sessionId_, "In('testState')").get();
    ASSERT_TRUE(inCallResult.isSuccess());
    auto inResult = inCallResult.getValue<bool>();
    EXPECT_FALSE(inResult);
}

// Test console object
TEST_F(SimpleSCXMLTest, ConsoleFunction) {
    auto consoleResult = engine_->evaluateExpression(sessionId_, "typeof console").get();
    ASSERT_TRUE(consoleResult.isSuccess());
    auto consoleType = consoleResult.getValue<std::string>();
    EXPECT_EQ(consoleType, "object");

    auto logResult = engine_->evaluateExpression(sessionId_, "typeof console.log").get();
    ASSERT_TRUE(logResult.isSuccess());
    auto logType = logResult.getValue<std::string>();
    EXPECT_EQ(logType, "function");

    // Test console.log doesn't crash
    auto logCallResult = engine_->executeScript(sessionId_, "console.log('test'); 'ok'").get();
    ASSERT_TRUE(logCallResult.isSuccess());
}

// Test Math object
TEST_F(SimpleSCXMLTest, MathObject) {
    auto mathResult = engine_->evaluateExpression(sessionId_, "typeof Math").get();
    ASSERT_TRUE(mathResult.isSuccess());
    auto mathType = mathResult.getValue<std::string>();
    EXPECT_EQ(mathType, "object");

    // Test Math.max
    auto maxResult = engine_->evaluateExpression(sessionId_, "Math.max(1, 2, 3)").get();
    ASSERT_TRUE(maxResult.isSuccess());
    auto maxValue = maxResult.getValue<double>();
    EXPECT_EQ(maxValue, 3.0);

    // Test Math.PI
    auto piResult = engine_->evaluateExpression(sessionId_, "Math.PI").get();
    ASSERT_TRUE(piResult.isSuccess());
    auto piValue = piResult.getValue<double>();
    EXPECT_NEAR(piValue, 3.141592653589793, 0.000001);
}

// Test complex expression
TEST_F(SimpleSCXMLTest, ComplexExpression) {
    auto complexResult =
        engine_->evaluateExpression(sessionId_, "_name.length > 0 && typeof _sessionid === 'string'").get();
    ASSERT_TRUE(complexResult.isSuccess());
    auto complexValue = complexResult.getValue<bool>();
    EXPECT_TRUE(complexValue);
}

// Test error handling
TEST_F(SimpleSCXMLTest, ErrorHandling) {
    // Test syntax error
    auto errorResult = engine_->evaluateExpression(sessionId_, "invalid syntax here").get();
    EXPECT_FALSE(errorResult.isSuccess());
}

}  // namespace Tests
}  // namespace RSM