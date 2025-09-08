/**
 * @file SCXMLJavaScriptIntegrationTest.cpp
 * @brief Comprehensive SCXML-JavaScript integration tests for QuickJS engine
 * 
 * Tests real-world SCXML scenarios with W3C specification compliance:
 * - SCXML system variables (_event, _sessionid, _name, _ioprocessors)
 * - In() function for state checking
 * - Complex guard conditions and data model operations
 * - Event data access and manipulation
 * - Error handling in realistic scenarios
 */

#include <gtest/gtest.h>
#include <memory>
#include "runtime/ECMAScriptEngineFactory.h"
#include "runtime/IECMAScriptEngine.h"
#include "runtime/RuntimeContext.h"
#include "events/Event.h"
#include "common/Logger.h"

class SCXMLJavaScriptIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create QuickJS engine
        engine = SCXML::ECMAScriptEngineFactory::create(SCXML::ECMAScriptEngineFactory::EngineType::QUICKJS);
        ASSERT_NE(nullptr, engine) << "Failed to create QuickJS engine";
        
        bool initialized = engine->initialize();
        ASSERT_TRUE(initialized) << "Engine initialization failed";
        
        // Create runtime context
        runtimeContext = std::make_unique<SCXML::Runtime::RuntimeContext>();
        
        // Set up mock In() function for state checking
        engine->setStateCheckFunction([this](const std::string& stateName) -> bool {
            return currentStates.find(stateName) != currentStates.end();
        });
        
        // Initialize current states for testing
        currentStates = {"idle", "main", "authenticated"};
        
        // Set up test data model variables with proper JSON format
        engine->setVariable("config", "{\"timeout\": 5000, \"retries\": 3}");
        engine->setVariable("user", "{\"name\": \"testuser\", \"age\": 25, \"role\": \"admin\"}");
        engine->setVariable("items", "[{\"id\": 1, \"price\": 100}, {\"id\": 2, \"price\": 200}]");
    }

protected:
    std::unique_ptr<SCXML::IECMAScriptEngine> engine;
    std::unique_ptr<SCXML::Runtime::RuntimeContext> runtimeContext;
    std::set<std::string> currentStates;
    
    // Helper to create test events
    void setCurrentEvent(const std::string& name, const std::string& data = "") {
        auto event = std::make_shared<SCXML::Events::Event>(name, SCXML::Events::Event::Type::EXTERNAL);
        if (!data.empty()) {
            event->setData(data);
        }
        engine->setCurrentEvent(event);
    }
};

/**
 * @brief Test SCXML system variables access
 * W3C SCXML Section B.2.1: System Variables
 */
TEST_F(SCXMLJavaScriptIntegrationTest, W3C_SystemVariables_EventAccess) {
    // Set up test event with data
    setCurrentEvent("user.click", "{\"x\": 150, \"y\": 200, \"button\": \"left\"}");
    
    // Test _event.name access
    auto result = engine->evaluateExpression("_event.name", *runtimeContext);
    ASSERT_TRUE(result.success) << "Failed to access _event.name: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<std::string>(result.value));
    EXPECT_EQ("user.click", std::get<std::string>(result.value));
    
    // Test _event.data access (if implemented)
    result = engine->evaluateExpression("typeof _event.data", *runtimeContext);
    ASSERT_TRUE(result.success) << "Failed to check _event.data type: " + result.errorMessage;
    
    SCXML::Common::Logger::info("SCXML system variables (_event) working correctly");
}

/**
 * @brief Test In() function for state checking
 * W3C SCXML Section B.2.2: In() Predicate
 */
TEST_F(SCXMLJavaScriptIntegrationTest, W3C_InFunction_StateChecking) {
    // Test single state check
    auto result = engine->evaluateExpression("In('idle')", *runtimeContext);
    ASSERT_TRUE(result.success) << "In() function failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_TRUE(std::get<bool>(result.value));
    
    // Test non-existent state
    result = engine->evaluateExpression("In('nonexistent')", *runtimeContext);
    ASSERT_TRUE(result.success) << "In() function failed for non-existent state: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_FALSE(std::get<bool>(result.value));
    
    // Test compound state conditions
    result = engine->evaluateExpression("In('idle') && In('main')", *runtimeContext);
    ASSERT_TRUE(result.success) << "Compound In() condition failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_TRUE(std::get<bool>(result.value));
    
    result = engine->evaluateExpression("In('idle') || In('processing')", *runtimeContext);
    ASSERT_TRUE(result.success) << "OR In() condition failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_TRUE(std::get<bool>(result.value));
    
    SCXML::Common::Logger::info("In() function working correctly for state checking");
}

/**
 * @brief Test complex guard conditions (realistic SCXML usage)
 * W3C SCXML Section 3.13: Conditional Execution
 */
TEST_F(SCXMLJavaScriptIntegrationTest, W3C_ComplexGuardConditions) {
    setCurrentEvent("payment.request", "{\"amount\": 1500, \"currency\": \"USD\"}");
    
    // Test authentication + amount check (typical SCXML guard)
    auto result = engine->evaluateExpression(
        "In('authenticated') && _event.data && _event.data.amount > 1000", 
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "Complex guard condition failed: " + result.errorMessage;
    
    // Test user role-based access control
    result = engine->evaluateExpression(
        "user.role === 'admin' && In('authenticated')", 
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "Role-based guard failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_TRUE(std::get<bool>(result.value));
    
    // Test timeout conditions
    result = engine->evaluateExpression(
        "config.timeout > 3000 && user.age >= 18", 
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "Timeout condition failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_TRUE(std::get<bool>(result.value));
    
    SCXML::Common::Logger::info("Complex guard conditions working correctly");
}

/**
 * @brief Test data model manipulation (realistic SCXML data operations)
 * W3C SCXML Section B: ECMAScript Data Model
 */
TEST_F(SCXMLJavaScriptIntegrationTest, W3C_DataModelManipulation) {
    // Test array operations on data model
    auto result = engine->evaluateExpression(
        "items.filter(item => item.price > 150).length", 
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "Array filter operation failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    EXPECT_EQ(1.0, std::get<double>(result.value)); // Only item with price 200
    
    // Test reduce operation (sum of prices)
    result = engine->evaluateExpression(
        "items.reduce((sum, item) => sum + item.price, 0)", 
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "Array reduce operation failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    EXPECT_EQ(300.0, std::get<double>(result.value)); // 100 + 200
    
    // Test object property access
    result = engine->evaluateExpression(
        "config.timeout / 1000", 
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "Object property calculation failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    EXPECT_EQ(5.0, std::get<double>(result.value));
    
    SCXML::Common::Logger::info("Data model manipulation working correctly");
}

/**
 * @brief Test mathematical and logical operations in SCXML context
 * W3C SCXML Section B: ECMAScript Mathematical Operations
 */
TEST_F(SCXMLJavaScriptIntegrationTest, W3C_MathematicalOperations) {
    setCurrentEvent("sensor.reading", "{\"temperature\": 22.5, \"humidity\": 65}");
    
    // Test temperature range checking (common in IoT SCXML)
    auto result = engine->evaluateExpression(
        "Math.abs(_event.data.temperature - 20) < 5", 
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "Temperature range check failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_TRUE(std::get<bool>(result.value));
    
    // Test statistical operations
    result = engine->evaluateExpression(
        "Math.max(...items.map(item => item.price))", 
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "Math.max operation failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    EXPECT_EQ(200.0, std::get<double>(result.value));
    
    // Test trigonometric functions
    result = engine->evaluateExpression("Math.sin(Math.PI / 2)", *runtimeContext);
    ASSERT_TRUE(result.success) << "Trigonometric function failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    EXPECT_NEAR(1.0, std::get<double>(result.value), 1e-10);
    
    SCXML::Common::Logger::info("Mathematical operations working correctly");
}

/**
 * @brief Test string manipulation in SCXML context
 * W3C SCXML Section B: ECMAScript String Operations
 */
TEST_F(SCXMLJavaScriptIntegrationTest, W3C_StringManipulation) {
    setCurrentEvent("message.received", "{\"text\": \"Hello World\", \"sender\": \"user123\"}");
    
    // Test string processing
    auto result = engine->evaluateExpression(
        "_event.data.text.toLowerCase().includes('hello')", 
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "String processing failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_TRUE(std::get<bool>(result.value));
    
    // Test regular expressions
    result = engine->evaluateExpression(
        "/^user\\d+$/.test(_event.data.sender)", 
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "Regular expression test failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_TRUE(std::get<bool>(result.value));
    
    // Test string concatenation
    result = engine->evaluateExpression(
        "'Response from: ' + user.name", 
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "String concatenation failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<std::string>(result.value));
    EXPECT_EQ("Response from: testuser", std::get<std::string>(result.value));
    
    SCXML::Common::Logger::info("String manipulation working correctly");
}

/**
 * @brief Test error scenarios and edge cases
 * W3C SCXML Section B: Error Handling
 */
TEST_F(SCXMLJavaScriptIntegrationTest, W3C_ErrorHandling) {
    // Test undefined variable access
    auto result = engine->evaluateExpression("typeof nonexistentVariable", *runtimeContext);
    ASSERT_TRUE(result.success) << "Undefined variable check failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<std::string>(result.value));
    EXPECT_EQ("undefined", std::get<std::string>(result.value));
    
    // Test null property access (should handle gracefully)
    result = engine->evaluateExpression("null && null.property", *runtimeContext);
    ASSERT_TRUE(result.success) << "Null property access failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<std::monostate>(result.value));
    // JavaScript "null && null.property" correctly returns null, not false
    
    // Test division by zero
    result = engine->evaluateExpression("1 / 0", *runtimeContext);
    ASSERT_TRUE(result.success) << "Division by zero failed: " + result.errorMessage;
    // JavaScript returns Infinity for division by zero
    
    // Test syntax error
    result = engine->evaluateExpression("invalid syntax here", *runtimeContext);
    ASSERT_FALSE(result.success) << "Syntax error should be caught";
    EXPECT_FALSE(result.errorMessage.empty());
    
    SCXML::Common::Logger::info("Error handling working correctly");
}

/**
 * @brief Test comprehensive SCXML scenario (realistic workflow)
 * W3C SCXML Section: Complete Integration Test
 */
TEST_F(SCXMLJavaScriptIntegrationTest, W3C_ComprehensiveScenario) {
    // Simulate a complete authentication workflow
    setCurrentEvent("login.attempt", "{\"username\": \"testuser\", \"timestamp\": 1234567890}");
    
    // Multi-condition guard evaluation
    auto result = engine->evaluateExpression(
        "In('idle') && !In('locked') && "
        "_event.data.username === user.name && "
        "user.age >= 18 && "
        "config.retries > 0",
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "Comprehensive scenario failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_TRUE(std::get<bool>(result.value));
    
    // Complex data transformation
    result = engine->evaluateExpression(
        "items"
        ".filter(item => item.price >= 100)"
        ".map(item => ({...item, discounted: item.price * 0.9}))"
        ".reduce((total, item) => total + item.discounted, 0)",
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "Complex data transformation failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    EXPECT_EQ(270.0, std::get<double>(result.value)); // (100 + 200) * 0.9 = 270
    
    SCXML::Common::Logger::info("Comprehensive SCXML-JavaScript integration test passed");
}