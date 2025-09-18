#include <gtest/gtest.h>
#include <cmath>
#include "../../scxml/include/SCXMLEngine.h"

using namespace SCXML;

class DataModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = createSCXMLEngine();
        ASSERT_NE(engine, nullptr);
        ASSERT_TRUE(engine->initialize());
        ASSERT_TRUE(engine->createSession("test_session"));
    }

    void TearDown() override {
        if (engine) {
            engine->destroySession("test_session");
            engine->shutdown();
        }
        engine.reset();
    }

    std::unique_ptr<SCXMLEngine> engine;
    const std::string session_id = "test_session";
};

// Test basic variable types according to SCXML spec
TEST_F(DataModelTest, BasicVariableTypes) {
    // Boolean
    auto bool_result = engine->setVariable(session_id, "boolVar", true).get();
    EXPECT_TRUE(bool_result.success);
    
    auto bool_get = engine->getVariable(session_id, "boolVar").get();
    EXPECT_TRUE(bool_get.success);
    EXPECT_TRUE(std::holds_alternative<bool>(bool_get.value));
    EXPECT_TRUE(std::get<bool>(bool_get.value));
    
    // Integer
    auto int_result = engine->setVariable(session_id, "intVar", static_cast<int64_t>(42)).get();
    EXPECT_TRUE(int_result.success);
    
    auto int_get = engine->getVariable(session_id, "intVar").get();
    EXPECT_TRUE(int_get.success);
    EXPECT_TRUE(std::holds_alternative<double>(int_get.value));
    EXPECT_EQ(std::get<double>(int_get.value), 42.0);
    
    // Double
    auto double_result = engine->setVariable(session_id, "doubleVar", 3.14159).get();
    EXPECT_TRUE(double_result.success);
    
    auto double_get = engine->getVariable(session_id, "doubleVar").get();
    EXPECT_TRUE(double_get.success);
    EXPECT_TRUE(std::holds_alternative<double>(double_get.value));
    EXPECT_NEAR(std::get<double>(double_get.value), 3.14159, 0.00001);
    
    // String
    auto string_result = engine->setVariable(session_id, "stringVar", std::string("Hello SCXML")).get();
    EXPECT_TRUE(string_result.success);
    
    auto string_get = engine->getVariable(session_id, "stringVar").get();
    EXPECT_TRUE(string_get.success);
    EXPECT_TRUE(std::holds_alternative<std::string>(string_get.value));
    EXPECT_EQ(std::get<std::string>(string_get.value), "Hello SCXML");
    
    // Undefined/null
    auto undefined_result = engine->setVariable(session_id, "undefinedVar", std::monostate{}).get();
    EXPECT_TRUE(undefined_result.success);
    
    auto undefined_get = engine->getVariable(session_id, "undefinedVar").get();
    EXPECT_TRUE(undefined_get.success);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(undefined_get.value));
}

// Test variable operations through JavaScript
TEST_F(DataModelTest, JavaScriptVariableOperations) {
    // Set variables through script execution
    auto result1 = engine->executeScript(session_id, "var counter = 0; counter;").get();
    EXPECT_TRUE(result1.success);
    EXPECT_EQ(std::get<double>(result1.value), 0);
    
    // Increment counter
    auto result2 = engine->executeScript(session_id, "counter = counter + 1; counter;").get();
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(std::get<double>(result2.value), 1);
    
    // String operations
    auto result3 = engine->executeScript(session_id, "var message = 'Hello' + ' ' + 'World'; message;").get();
    EXPECT_TRUE(result3.success);
    EXPECT_EQ(std::get<std::string>(result3.value), "Hello World");
    
    // Boolean operations
    auto result4 = engine->executeScript(session_id, "var flag = true && false; flag;").get();
    EXPECT_TRUE(result4.success);
    EXPECT_FALSE(std::get<bool>(result4.value));
}

// Test arithmetic operations
TEST_F(DataModelTest, ArithmeticOperations) {
    // Basic arithmetic
    auto result1 = engine->executeScript(session_id, "var a = 10; var b = 5; a + b;").get();
    EXPECT_TRUE(result1.success);
    EXPECT_EQ(std::get<double>(result1.value), 15);
    
    auto result2 = engine->executeScript(session_id, "a - b;").get();
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(std::get<double>(result2.value), 5);
    
    auto result3 = engine->executeScript(session_id, "a * b;").get();
    EXPECT_TRUE(result3.success);
    EXPECT_EQ(std::get<double>(result3.value), 50);
    
    auto result4 = engine->executeScript(session_id, "a / b;").get();
    EXPECT_TRUE(result4.success);
    EXPECT_EQ(std::get<double>(result4.value), 2.0);
    
    // Modulo
    auto result5 = engine->executeScript(session_id, "a % 3;").get();
    EXPECT_TRUE(result5.success);
    EXPECT_EQ(std::get<double>(result5.value), 1);
}

// Test type conversions
TEST_F(DataModelTest, TypeConversions) {
    // String to number
    auto result1 = engine->executeScript(session_id, "var str = '42'; parseInt(str);").get();
    EXPECT_TRUE(result1.success);
    EXPECT_EQ(std::get<double>(result1.value), 42);
    
    auto result2 = engine->executeScript(session_id, "var floatStr = '3.14'; parseFloat(floatStr);").get();
    EXPECT_TRUE(result2.success);
    EXPECT_NEAR(std::get<double>(result2.value), 3.14, 0.001);
    
    // Number to string
    auto result3 = engine->executeScript(session_id, "var num = 123; num.toString();").get();
    EXPECT_TRUE(result3.success);
    EXPECT_EQ(std::get<std::string>(result3.value), "123");
    
    // Boolean conversions
    auto result4 = engine->executeScript(session_id, "Boolean(1);").get();
    EXPECT_TRUE(result4.success);
    EXPECT_TRUE(std::get<bool>(result4.value));
    
    auto result5 = engine->executeScript(session_id, "Boolean(0);").get();
    EXPECT_TRUE(result5.success);
    EXPECT_FALSE(std::get<bool>(result5.value));
}

// Test variable scope and persistence
TEST_F(DataModelTest, VariableScopeAndPersistence) {
    // Set variable through API
    auto set_result = engine->setVariable(session_id, "persistentVar", std::string("persistent_value")).get();
    EXPECT_TRUE(set_result.success);
    
    // Access through JavaScript
    auto js_result = engine->executeScript(session_id, "persistentVar;").get();
    EXPECT_TRUE(js_result.success);
    EXPECT_EQ(std::get<std::string>(js_result.value), "persistent_value");
    
    // Modify through JavaScript
    auto modify_result = engine->executeScript(session_id, "persistentVar = 'modified_value'; persistentVar;").get();
    EXPECT_TRUE(modify_result.success);
    EXPECT_EQ(std::get<std::string>(modify_result.value), "modified_value");
    
    // Verify through API
    auto api_result = engine->getVariable(session_id, "persistentVar").get();
    EXPECT_TRUE(api_result.success);
    EXPECT_EQ(std::get<std::string>(api_result.value), "modified_value");
}

// Test complex data structures (objects and arrays)
TEST_F(DataModelTest, ComplexDataStructures) {
    // Create object
    auto obj_result = engine->executeScript(session_id, 
        "var person = {name: 'John', age: 30, active: true}; person.name;").get();
    EXPECT_TRUE(obj_result.success);
    EXPECT_EQ(std::get<std::string>(obj_result.value), "John");
    
    // Access object properties
    auto age_result = engine->executeScript(session_id, "person.age;").get();
    EXPECT_TRUE(age_result.success);
    EXPECT_EQ(std::get<double>(age_result.value), 30);
    
    auto active_result = engine->executeScript(session_id, "person.active;").get();
    EXPECT_TRUE(active_result.success);
    EXPECT_TRUE(std::get<bool>(active_result.value));
    
    // Create array
    auto array_result = engine->executeScript(session_id, 
        "var numbers = [1, 2, 3, 4, 5]; numbers[2];").get();
    EXPECT_TRUE(array_result.success);
    EXPECT_EQ(std::get<double>(array_result.value), 3);
    
    // Array length
    auto length_result = engine->executeScript(session_id, "numbers.length;").get();
    EXPECT_TRUE(length_result.success);
    EXPECT_EQ(std::get<double>(length_result.value), 5);
}

// Test error handling for undefined variables
TEST_F(DataModelTest, UndefinedVariableHandling) {
    // Access undefined variable through API
    auto undefined_result = engine->getVariable(session_id, "undefinedVariable").get();
    EXPECT_FALSE(undefined_result.success);
    EXPECT_FALSE(undefined_result.errorMessage.empty());
    
    // Access undefined variable through JavaScript should return undefined
    auto js_undefined = engine->executeScript(session_id, "typeof undefinedVariable;").get();
    EXPECT_TRUE(js_undefined.success);
    EXPECT_EQ(std::get<std::string>(js_undefined.value), "undefined");
}

// Test variable names with special characters
TEST_F(DataModelTest, SpecialVariableNames) {
    // Valid JavaScript variable names
    EXPECT_TRUE(engine->setVariable(session_id, "_private", 123).get().success);
    EXPECT_TRUE(engine->setVariable(session_id, "$special", std::string("special")).get().success);
    EXPECT_TRUE(engine->setVariable(session_id, "camelCase", true).get().success);
    
    // Verify access
    auto private_result = engine->getVariable(session_id, "_private").get();
    EXPECT_TRUE(private_result.success);
    EXPECT_EQ(std::get<double>(private_result.value), 123);
    
    auto special_result = engine->getVariable(session_id, "$special").get();
    EXPECT_TRUE(special_result.success);
    EXPECT_EQ(std::get<std::string>(special_result.value), "special");
    
    auto camel_result = engine->getVariable(session_id, "camelCase").get();
    EXPECT_TRUE(camel_result.success);
    EXPECT_TRUE(std::get<bool>(camel_result.value));
}

// Test mathematical functions
TEST_F(DataModelTest, MathematicalFunctions) {
    // Math object functions
    auto sqrt_result = engine->executeScript(session_id, "Math.sqrt(16);").get();
    EXPECT_TRUE(sqrt_result.success);
    EXPECT_EQ(std::get<double>(sqrt_result.value), 4.0);
    
    auto pow_result = engine->executeScript(session_id, "Math.pow(2, 3);").get();
    EXPECT_TRUE(pow_result.success);
    EXPECT_EQ(std::get<double>(pow_result.value), 8.0);
    
    auto max_result = engine->executeScript(session_id, "Math.max(10, 20, 5);").get();
    EXPECT_TRUE(max_result.success);
    EXPECT_EQ(std::get<double>(max_result.value), 20.0);
    
    auto pi_result = engine->executeScript(session_id, "Math.PI;").get();
    EXPECT_TRUE(pi_result.success);
    EXPECT_NEAR(std::get<double>(pi_result.value), M_PI, 0.000001);
}

// Test string manipulation functions
TEST_F(DataModelTest, StringManipulation) {
    // String methods
    auto upper_result = engine->executeScript(session_id, 
        "var text = 'hello world'; text.toUpperCase();").get();
    EXPECT_TRUE(upper_result.success);
    EXPECT_EQ(std::get<std::string>(upper_result.value), "HELLO WORLD");
    
    auto substring_result = engine->executeScript(session_id, "text.substring(0, 5);").get();
    EXPECT_TRUE(substring_result.success);
    EXPECT_EQ(std::get<std::string>(substring_result.value), "hello");
    
    auto length_result = engine->executeScript(session_id, "text.length;").get();
    EXPECT_TRUE(length_result.success);
    EXPECT_EQ(std::get<double>(length_result.value), 11);
    
    auto index_result = engine->executeScript(session_id, "text.indexOf('world');").get();
    EXPECT_TRUE(index_result.success);
    EXPECT_EQ(std::get<double>(index_result.value), 6);
}