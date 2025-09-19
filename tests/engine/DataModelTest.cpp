#include "scripting/JSEngine.h"
#include <cmath>
#include <gtest/gtest.h>

class DataModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &RSM::JSEngine::instance();
        ASSERT_TRUE(engine_->initialize());

        sessionId_ = "test_session_datamodel";
        bool result = engine_->createSession(sessionId_, "");
        ASSERT_TRUE(result) << "Failed to create session";
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

// Test basic variable types
TEST_F(DataModelTest, BasicVariableTypes) {
    // Test number
    auto numberResult = engine_->executeScript(sessionId_, "var num = 42; num").get();
    ASSERT_TRUE(numberResult.isSuccess());
    EXPECT_EQ(numberResult.getValue<double>(), 42.0);

    // Test string
    auto stringResult = engine_->executeScript(sessionId_, "var str = 'hello'; str").get();
    ASSERT_TRUE(stringResult.isSuccess());
    EXPECT_EQ(stringResult.getValue<std::string>(), "hello");

    // Test boolean
    auto boolResult = engine_->executeScript(sessionId_, "var flag = true; flag").get();
    ASSERT_TRUE(boolResult.isSuccess());
    EXPECT_TRUE(boolResult.getValue<bool>());

    // Test null by checking typeof
    auto nullResult = engine_->executeScript(sessionId_, "var empty = null; typeof empty").get();
    ASSERT_TRUE(nullResult.isSuccess());
    EXPECT_EQ(nullResult.getValue<std::string>(),
              "object");  // null is typeof "object" in JS

    // Test undefined by checking typeof
    auto undefinedResult = engine_->executeScript(sessionId_, "var undef; typeof undef").get();
    ASSERT_TRUE(undefinedResult.isSuccess());
    EXPECT_EQ(undefinedResult.getValue<std::string>(), "undefined");
}

// Test object creation and access (using property access since objects can't be
// returned directly)
TEST_F(DataModelTest, ObjectTypes) {
    // Create object and verify properties individually
    auto setupResult = engine_
                           ->executeScript(sessionId_, "var obj = {name: 'test', value: 123, "
                                                       "nested: {inner: 'data'}}; 'created'")
                           .get();
    ASSERT_TRUE(setupResult.isSuccess());
    EXPECT_EQ(setupResult.getValue<std::string>(), "created");

    // Check object type
    auto typeResult = engine_->evaluateExpression(sessionId_, "typeof obj").get();
    ASSERT_TRUE(typeResult.isSuccess());
    EXPECT_EQ(typeResult.getValue<std::string>(), "object");

    // Access object properties
    auto nameResult = engine_->evaluateExpression(sessionId_, "obj.name").get();
    ASSERT_TRUE(nameResult.isSuccess());
    EXPECT_EQ(nameResult.getValue<std::string>(), "test");

    auto valueResult = engine_->evaluateExpression(sessionId_, "obj.value").get();
    ASSERT_TRUE(valueResult.isSuccess());
    EXPECT_EQ(valueResult.getValue<double>(), 123.0);

    auto nestedResult = engine_->evaluateExpression(sessionId_, "obj.nested.inner").get();
    ASSERT_TRUE(nestedResult.isSuccess());
    EXPECT_EQ(nestedResult.getValue<std::string>(), "data");
}

// Test array creation and manipulation
TEST_F(DataModelTest, ArrayTypes) {
    // Create array and verify through properties
    auto setupResult = engine_->executeScript(sessionId_, "var arr = [1, 2, 'three', {four: 4}]; 'created'").get();
    ASSERT_TRUE(setupResult.isSuccess());

    // Test Array.isArray
    auto isArrayResult = engine_->evaluateExpression(sessionId_, "Array.isArray(arr)").get();
    ASSERT_TRUE(isArrayResult.isSuccess());
    EXPECT_TRUE(isArrayResult.getValue<bool>());

    // Test array length
    auto lengthResult = engine_->evaluateExpression(sessionId_, "arr.length").get();
    ASSERT_TRUE(lengthResult.isSuccess());
    EXPECT_EQ(lengthResult.getValue<double>(), 4.0);

    // Test array access
    auto firstResult = engine_->evaluateExpression(sessionId_, "arr[0]").get();
    ASSERT_TRUE(firstResult.isSuccess());
    EXPECT_EQ(firstResult.getValue<double>(), 1.0);

    auto stringResult = engine_->evaluateExpression(sessionId_, "arr[2]").get();
    ASSERT_TRUE(stringResult.isSuccess());
    EXPECT_EQ(stringResult.getValue<std::string>(), "three");

    // Test array modification
    auto pushResult = engine_->executeScript(sessionId_, "arr.push('five'); arr.length").get();
    ASSERT_TRUE(pushResult.isSuccess());
    EXPECT_EQ(pushResult.getValue<double>(), 5.0);
}

// Test mathematical operations
TEST_F(DataModelTest, MathematicalOperations) {
    // Basic arithmetic
    auto addResult = engine_->evaluateExpression(sessionId_, "10 + 5").get();
    ASSERT_TRUE(addResult.isSuccess());
    EXPECT_EQ(addResult.getValue<double>(), 15.0);

    auto subResult = engine_->evaluateExpression(sessionId_, "10 - 3").get();
    ASSERT_TRUE(subResult.isSuccess());
    EXPECT_EQ(subResult.getValue<double>(), 7.0);

    auto mulResult = engine_->evaluateExpression(sessionId_, "4 * 6").get();
    ASSERT_TRUE(mulResult.isSuccess());
    EXPECT_EQ(mulResult.getValue<double>(), 24.0);

    auto divResult = engine_->evaluateExpression(sessionId_, "15 / 3").get();
    ASSERT_TRUE(divResult.isSuccess());
    EXPECT_EQ(divResult.getValue<double>(), 5.0);

    // Test Math object functions
    auto sqrtResult = engine_->evaluateExpression(sessionId_, "Math.sqrt(16)").get();
    ASSERT_TRUE(sqrtResult.isSuccess());
    EXPECT_EQ(sqrtResult.getValue<double>(), 4.0);

    auto maxResult = engine_->evaluateExpression(sessionId_, "Math.max(10, 20, 5)").get();
    ASSERT_TRUE(maxResult.isSuccess());
    EXPECT_EQ(maxResult.getValue<double>(), 20.0);
}

// Test string operations
TEST_F(DataModelTest, StringOperations) {
    // String concatenation
    auto concatResult = engine_->evaluateExpression(sessionId_, "'Hello' + ' ' + 'World'").get();
    ASSERT_TRUE(concatResult.isSuccess());
    EXPECT_EQ(concatResult.getValue<std::string>(), "Hello World");

    // String methods
    auto upperResult = engine_->evaluateExpression(sessionId_, "'hello'.toUpperCase()").get();
    ASSERT_TRUE(upperResult.isSuccess());
    EXPECT_EQ(upperResult.getValue<std::string>(), "HELLO");

    auto lengthResult = engine_->evaluateExpression(sessionId_, "'test'.length").get();
    ASSERT_TRUE(lengthResult.isSuccess());
    EXPECT_EQ(lengthResult.getValue<double>(), 4.0);

    auto substrResult = engine_->evaluateExpression(sessionId_, "'testing'.substr(1, 3)").get();
    ASSERT_TRUE(substrResult.isSuccess());
    EXPECT_EQ(substrResult.getValue<std::string>(), "est");
}

// Test boolean operations and comparisons
TEST_F(DataModelTest, BooleanOperations) {
    // Logical operations
    auto andResult = engine_->evaluateExpression(sessionId_, "true && false").get();
    ASSERT_TRUE(andResult.isSuccess());
    EXPECT_FALSE(andResult.getValue<bool>());

    auto orResult = engine_->evaluateExpression(sessionId_, "true || false").get();
    ASSERT_TRUE(orResult.isSuccess());
    EXPECT_TRUE(orResult.getValue<bool>());

    auto notResult = engine_->evaluateExpression(sessionId_, "!true").get();
    ASSERT_TRUE(notResult.isSuccess());
    EXPECT_FALSE(notResult.getValue<bool>());

    // Comparisons
    auto eqResult = engine_->evaluateExpression(sessionId_, "5 === 5").get();
    ASSERT_TRUE(eqResult.isSuccess());
    EXPECT_TRUE(eqResult.getValue<bool>());

    auto neqResult = engine_->evaluateExpression(sessionId_, "5 !== 3").get();
    ASSERT_TRUE(neqResult.isSuccess());
    EXPECT_TRUE(neqResult.getValue<bool>());

    auto gtResult = engine_->evaluateExpression(sessionId_, "10 > 5").get();
    ASSERT_TRUE(gtResult.isSuccess());
    EXPECT_TRUE(gtResult.getValue<bool>());

    auto ltResult = engine_->evaluateExpression(sessionId_, "3 < 8").get();
    ASSERT_TRUE(ltResult.isSuccess());
    EXPECT_TRUE(ltResult.getValue<bool>());
}

// Test function definition and calling
TEST_F(DataModelTest, FunctionOperations) {
    // Define function and verify it exists
    auto defineResult = engine_->executeScript(sessionId_, "function add(a, b) { return a + b; } typeof add").get();
    ASSERT_TRUE(defineResult.isSuccess());
    EXPECT_EQ(defineResult.getValue<std::string>(), "function");

    // Call function
    auto callResult = engine_->evaluateExpression(sessionId_, "add(3, 7)").get();
    ASSERT_TRUE(callResult.isSuccess());
    EXPECT_EQ(callResult.getValue<double>(), 10.0);

    // Anonymous function
    auto anonResult =
        engine_->executeScript(sessionId_, "var multiply = function(x, y) { return x * y; }; multiply(4, 5)").get();
    ASSERT_TRUE(anonResult.isSuccess());
    EXPECT_EQ(anonResult.getValue<double>(), 20.0);
}

// Test variable scope and closure
TEST_F(DataModelTest, VariableScope) {
    // Global variable
    auto globalResult = engine_->executeScript(sessionId_, "var global = 'global_value'; global").get();
    ASSERT_TRUE(globalResult.isSuccess());
    EXPECT_EQ(globalResult.getValue<std::string>(), "global_value");

    // Function scope
    auto scopeResult = engine_
                           ->executeScript(sessionId_, "function testScope() { var local = 'local_value'; "
                                                       "return local; } testScope()")
                           .get();
    ASSERT_TRUE(scopeResult.isSuccess());
    EXPECT_EQ(scopeResult.getValue<std::string>(), "local_value");

    // Closure test - verify counter functionality
    auto setupResult = engine_
                           ->executeScript(sessionId_, "function createCounter() { var count = 0; return "
                                                       "function() { return ++count; }; } "
                                                       "var counter = createCounter(); 'setup'")
                           .get();
    ASSERT_TRUE(setupResult.isSuccess());

    // Test counter calls individually
    auto call1Result = engine_->evaluateExpression(sessionId_, "counter()").get();
    ASSERT_TRUE(call1Result.isSuccess());
    EXPECT_EQ(call1Result.getValue<double>(), 1.0);

    auto call2Result = engine_->evaluateExpression(sessionId_, "counter()").get();
    ASSERT_TRUE(call2Result.isSuccess());
    EXPECT_EQ(call2Result.getValue<double>(), 2.0);

    auto call3Result = engine_->evaluateExpression(sessionId_, "counter()").get();
    ASSERT_TRUE(call3Result.isSuccess());
    EXPECT_EQ(call3Result.getValue<double>(), 3.0);
}

// Test error handling in data model
TEST_F(DataModelTest, ErrorHandling) {
    // Syntax error
    auto syntaxResult = engine_->evaluateExpression(sessionId_, "var x = ;").get();
    EXPECT_FALSE(syntaxResult.isSuccess());

    // Reference error
    auto refResult = engine_->evaluateExpression(sessionId_, "nonExistentVariable").get();
    EXPECT_FALSE(refResult.isSuccess());

    // Type error - accessing property of null
    auto typeResult = engine_->evaluateExpression(sessionId_, "null.someProperty").get();
    EXPECT_FALSE(typeResult.isSuccess());
}

// Test JSON operations
TEST_F(DataModelTest, JSONOperations) {
    // JSON stringify
    auto setupResult = engine_->executeScript(sessionId_, "var obj = {name: 'test', value: 42}; 'setup'").get();
    ASSERT_TRUE(setupResult.isSuccess());

    auto stringifyResult = engine_->evaluateExpression(sessionId_, "JSON.stringify(obj)").get();
    ASSERT_TRUE(stringifyResult.isSuccess());
    EXPECT_EQ(stringifyResult.getValue<std::string>(), "{\"name\":\"test\",\"value\":42}");

    // JSON parse - verify the parsed object works
    auto parseSetupResult = engine_
                                ->executeScript(sessionId_, "var jsonStr = '{\"parsed\": true, \"number\": 123}'; "
                                                            "var parsed = JSON.parse(jsonStr); 'parsed'")
                                .get();
    ASSERT_TRUE(parseSetupResult.isSuccess());

    // Test parsed object properties
    auto parsedBoolResult = engine_->evaluateExpression(sessionId_, "parsed.parsed").get();
    ASSERT_TRUE(parsedBoolResult.isSuccess());
    EXPECT_TRUE(parsedBoolResult.getValue<bool>());

    auto parsedNumResult = engine_->evaluateExpression(sessionId_, "parsed.number").get();
    ASSERT_TRUE(parsedNumResult.isSuccess());
    EXPECT_EQ(parsedNumResult.getValue<double>(), 123.0);
}
