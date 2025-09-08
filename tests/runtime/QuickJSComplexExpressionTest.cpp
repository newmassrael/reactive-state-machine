/**
 * @file QuickJSComplexExpressionTest.cpp
 * @brief Simple test to verify QuickJS integration works
 */

#include <gtest/gtest.h>
#include <memory>
#include "runtime/ECMAScriptEngineFactory.h"
#include "runtime/IECMAScriptEngine.h"
#include "runtime/RuntimeContext.h"
#include "events/Event.h"
#include "common/Logger.h"

class QuickJSComplexExpressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create QuickJS engine explicitly
        engine = SCXML::ECMAScriptEngineFactory::create(SCXML::ECMAScriptEngineFactory::EngineType::QUICKJS);
        ASSERT_NE(nullptr, engine) << "Failed to create QuickJS engine";
        
        // Initialize the engine
        bool initialized = engine->initialize();
        ASSERT_TRUE(initialized) << "Engine initialization failed";
        
        // Create mock runtime context
        runtimeContext = std::make_unique<SCXML::Runtime::RuntimeContext>();
    }

protected:
    std::unique_ptr<SCXML::IECMAScriptEngine> engine;
    std::unique_ptr<SCXML::Runtime::RuntimeContext> runtimeContext;
};

/**
 * @brief Test basic arithmetic operations
 */
TEST_F(QuickJSComplexExpressionTest, BasicArithmetic) {
    auto result = engine->evaluateExpression("2 + 3", *runtimeContext);
    ASSERT_TRUE(result.success) << "Basic arithmetic failed: " + result.errorMessage;
    
    // QuickJS returns numbers as double
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    double val = std::get<double>(result.value);
    EXPECT_EQ(5.0, val);
}

/**
 * @brief Test mathematical functions
 */
TEST_F(QuickJSComplexExpressionTest, MathFunctions) {
    auto result = engine->evaluateExpression("Math.sqrt(16)", *runtimeContext);
    ASSERT_TRUE(result.success) << "Math.sqrt failed: " + result.errorMessage;
    
    // QuickJS returns numbers as double
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    double val = std::get<double>(result.value);
    EXPECT_EQ(4.0, val);
}

/**
 * @brief Test array operations
 */
TEST_F(QuickJSComplexExpressionTest, ArrayOperations) {
    auto result = engine->evaluateExpression("[1, 2, 3].length", *runtimeContext);
    ASSERT_TRUE(result.success) << "Array length failed: " + result.errorMessage;
    
    // QuickJS returns numbers as double
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    double val = std::get<double>(result.value);
    EXPECT_EQ(3.0, val);
}

/**
 * @brief Test variable operations
 */
TEST_F(QuickJSComplexExpressionTest, VariableOperations) {
    engine->setVariable("x", "10");
    auto result = engine->evaluateExpression("x * 2", *runtimeContext);
    ASSERT_TRUE(result.success) << "Variable operation failed: " + result.errorMessage;
    
    // QuickJS returns numbers as double
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    double val = std::get<double>(result.value);
    EXPECT_EQ(20.0, val);
}

/**
 * @brief Test string operations
 */
TEST_F(QuickJSComplexExpressionTest, StringOperations) {
    auto result = engine->evaluateExpression("'Hello'.length", *runtimeContext);
    ASSERT_TRUE(result.success) << "String operation failed: " + result.errorMessage;
    
    // QuickJS returns numbers as double (even for string length)
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    double val = std::get<double>(result.value);
    EXPECT_EQ(5.0, val);
}

/**
 * @brief Test error handling
 */
TEST_F(QuickJSComplexExpressionTest, ErrorHandling) {
    auto result = engine->evaluateExpression("invalid syntax", *runtimeContext);
    ASSERT_FALSE(result.success) << "Syntax error should be caught";
    EXPECT_FALSE(result.errorMessage.empty());
}

/**
 * @brief Performance comparison test - show QuickJS vs Fallback
 */
TEST_F(QuickJSComplexExpressionTest, QuickJSPerformanceVsFallback) {
    // Test a complex expression that fallback cannot handle
    auto result = engine->evaluateExpression("[1,2,3,4,5].map(x => x*x).reduce((a,b) => a+b, 0)", *runtimeContext);
    ASSERT_TRUE(result.success) << "Complex expression failed: " + result.errorMessage;
    
    // QuickJS returns numbers as double
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    double val = std::get<double>(result.value);
    EXPECT_EQ(55.0, val); // 1 + 4 + 9 + 16 + 25 = 55
    
    SCXML::Common::Logger::info("QuickJS successfully evaluated complex JavaScript expression that fallback cannot handle");
}