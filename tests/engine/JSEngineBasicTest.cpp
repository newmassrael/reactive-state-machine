#include "W3CEventTestHelper.h"
#include "common/Logger.h"
#include "runtime/StateMachine.h"
#include "scripting/JSEngine.h"
#include <gtest/gtest.h>
#include <iostream>
#include <memory>

class JSEngineBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &RSM::JSEngine::instance();
        // Ensure test isolation with JSEngine reset
        engine_->reset();

        sessionId_ = "js_basic_test_session";
        bool createResult = engine_->createSession(sessionId_, "");
        ASSERT_TRUE(createResult) << "Failed to create JS basic test session";

        // Initialize W3C SCXML 5.10 test helper
        w3cHelper_.initialize(engine_, sessionId_);
    }

    void TearDown() override {
        if (engine_) {
            engine_->destroySession(sessionId_);
        }
    }

    RSM::JSEngine *engine_;
    std::string sessionId_;
    RSM::Tests::W3CEventTestHelper w3cHelper_;

    // Helper methods to reduce test code duplication
    template <typename T> T evaluateAndExpect(const std::string &expression, const std::string &errorMsg = "") {
        auto result = engine_->evaluateExpression(sessionId_, expression).get();
        EXPECT_TRUE(result.isSuccess()) << (errorMsg.empty() ? "Expression evaluation failed: " + expression
                                                             : errorMsg);
        return result.getValue<T>();
    }

    void expectExpressionType(const std::string &expression, const std::string &expectedType) {
        auto typeResult = evaluateAndExpect<std::string>("typeof " + expression);
        EXPECT_EQ(typeResult, expectedType) << expression << " should be of type " << expectedType;
    }

    void expectExpressionValue(const std::string &expression, const auto &expectedValue) {
        using ValueType = std::decay_t<decltype(expectedValue)>;
        auto actualValue = evaluateAndExpect<ValueType>(expression);
        EXPECT_EQ(actualValue, expectedValue) << "Expression " << expression << " should equal expected value";
    }

    bool tryEvaluateExpression(const std::string &expression) {
        auto result = engine_->evaluateExpression(sessionId_, expression).get();
        return result.isSuccess();
    }
};

TEST_F(JSEngineBasicTest, ECMAScript_BasicArithmeticExpression) {
    // Test basic arithmetic
    auto result = engine_->evaluateExpression(sessionId_, "2 + 3").get();
    ASSERT_TRUE(result.isSuccess()) << "Failed to evaluate expression";
    EXPECT_EQ(result.getValue<double>(), 5.0);
}

TEST_F(JSEngineBasicTest, ECMAScript_DataModel_VariableAssignment) {
    // Assign variable
    auto assignResult = engine_->executeScript(sessionId_, "var testVar = 'Hello World'; testVar").get();
    ASSERT_TRUE(assignResult.isSuccess());
    EXPECT_EQ(assignResult.getValue<std::string>(), "Hello World");

    // Retrieve variable
    auto retrieveResult = engine_->evaluateExpression(sessionId_, "testVar").get();
    ASSERT_TRUE(retrieveResult.isSuccess());
    EXPECT_EQ(retrieveResult.getValue<std::string>(), "Hello World");
}

TEST_F(JSEngineBasicTest, SCXML_BuiltinFunction_InPredicate) {
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

TEST_F(JSEngineBasicTest, SCXML_SystemVariables_EventAndSession) {
    // Test _sessionid exists and is string
    expectExpressionType("_sessionid", "string");

    // Test _name exists and is string
    expectExpressionType("_name", "string");

    // Test _ioprocessors exists and is object
    expectExpressionType("_ioprocessors", "object");

    // W3C SCXML 5.10: _event should NOT exist before first event
    w3cHelper_.assertEventUndefined();

    // Trigger first event to initialize _event
    w3cHelper_.triggerEvent();

    // Test _event exists and is object after first event
    w3cHelper_.assertEventObject();
}

TEST_F(JSEngineBasicTest, SCXML_ErrorHandling_ExecutionErrors) {
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

TEST_F(JSEngineBasicTest, ECMAScript_ExpressionEvaluation_ComplexExpressions) {
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

TEST_F(JSEngineBasicTest, ECMAScript_ConsoleAPI_LoggingSupport) {
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

TEST_F(JSEngineBasicTest, SCXML_ExpressionValidation_SyntaxChecking) {
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

TEST_F(JSEngineBasicTest, ECMAScript_DataTypes_ArrayHandling) {
    // Test array literal creation and validation
    auto arrayValidation1 = engine_->validateExpression(sessionId_, "[]").get();
    EXPECT_TRUE(arrayValidation1.isSuccess()) << "Empty array literal should be valid";

    auto arrayValidation2 = engine_->validateExpression(sessionId_, "[1, 2, 3]").get();
    EXPECT_TRUE(arrayValidation2.isSuccess()) << "Array with numbers should be valid";

    auto arrayValidation3 = engine_->validateExpression(sessionId_, "['a', 'b', 'c']").get();
    EXPECT_TRUE(arrayValidation3.isSuccess()) << "Array with strings should be valid";

    auto arrayValidation4 = engine_->validateExpression(sessionId_, "[1, 'mixed', true]").get();
    EXPECT_TRUE(arrayValidation4.isSuccess()) << "Mixed type array should be valid";

    // Test array evaluation and access
    auto emptyArrayResult = engine_->evaluateExpression(sessionId_, "[]").get();
    EXPECT_TRUE(emptyArrayResult.isSuccess()) << "Empty array evaluation should succeed";

    auto arrayLengthResult = engine_->evaluateExpression(sessionId_, "[1, 2, 3].length").get();
    EXPECT_TRUE(arrayLengthResult.isSuccess()) << "Array length access should work";
    if (arrayLengthResult.isSuccess()) {
        EXPECT_EQ(arrayLengthResult.getValue<double>(), 3.0) << "Array length should be 3";
    }

    // Test array assignment to variables
    auto arrayAssignResult = engine_->executeScript(sessionId_, "var myArray = [1, 2, 3]; myArray").get();
    EXPECT_TRUE(arrayAssignResult.isSuccess()) << "Array assignment should succeed";

    // Test array element access
    auto elementAccessResult = engine_->evaluateExpression(sessionId_, "myArray[0]").get();
    EXPECT_TRUE(elementAccessResult.isSuccess()) << "Array element access should work";
    if (elementAccessResult.isSuccess()) {
        EXPECT_EQ(elementAccessResult.getValue<double>(), 1.0) << "First element should be 1";
    }

    // Test array modification
    auto pushResult = engine_->executeScript(sessionId_, "myArray.push(4); myArray.length").get();
    EXPECT_TRUE(pushResult.isSuccess()) << "Array push should work";
    if (pushResult.isSuccess()) {
        EXPECT_EQ(pushResult.getValue<double>(), 4.0) << "Array length after push should be 4";
    }

    // Test SCXML-style array initialization (the critical test case)
    auto scxmlArrayResult = engine_->evaluateExpression(sessionId_, "[]").get();
    EXPECT_TRUE(scxmlArrayResult.isSuccess()) << "SCXML-style empty array should work";

    // Test assignment of empty array to data model variable
    auto dataModelArrayResult = engine_->executeScript(sessionId_, "var entry_sequence = []; entry_sequence").get();
    EXPECT_TRUE(dataModelArrayResult.isSuccess()) << "Data model array assignment should succeed";

    // Test array push operation in data model context
    auto arrayPushDataResult =
        engine_->executeScript(sessionId_, "entry_sequence.push('test'); entry_sequence.length").get();
    EXPECT_TRUE(arrayPushDataResult.isSuccess()) << "Data model array push should work";
    if (arrayPushDataResult.isSuccess()) {
        EXPECT_EQ(arrayPushDataResult.getValue<double>(), 1.0) << "Array should have one element after push";
    }
}

TEST_F(JSEngineBasicTest, ECMAScript_DataTypes_ObjectLiterals) {
    // Test object creation and evaluation
    auto objectResult = engine_->evaluateExpression(sessionId_, "({name: 'test', value: 42})").get();
    ASSERT_TRUE(objectResult.isSuccess()) << "Object literal should be evaluable";
    EXPECT_TRUE(objectResult.isObject()) << "Result should be recognized as object";

    auto obj = objectResult.getObject();
    ASSERT_NE(obj, nullptr) << "Object should not be null";
    EXPECT_EQ(obj->properties.size(), 2u) << "Object should have 2 properties";

    auto nameValue = objectResult.getObjectProperty("name");
    EXPECT_TRUE(std::holds_alternative<std::string>(nameValue)) << "Name should be string";
    EXPECT_EQ(std::get<std::string>(nameValue), "test") << "Name value should be 'test'";

    auto valueProperty = objectResult.getObjectProperty("value");
    bool isNumber = std::holds_alternative<double>(valueProperty) || std::holds_alternative<int64_t>(valueProperty);
    EXPECT_TRUE(isNumber) << "Value should be number";

    double actualValue = 0.0;
    if (std::holds_alternative<double>(valueProperty)) {
        actualValue = std::get<double>(valueProperty);
    } else if (std::holds_alternative<int64_t>(valueProperty)) {
        actualValue = static_cast<double>(std::get<int64_t>(valueProperty));
    }
    EXPECT_EQ(actualValue, 42.0) << "Value should be 42";

    // Test array creation and evaluation
    auto arrayResult = engine_->evaluateExpression(sessionId_, "[1, 'hello', true]").get();
    ASSERT_TRUE(arrayResult.isSuccess()) << "Array literal should be evaluable";
    EXPECT_TRUE(arrayResult.isArray()) << "Result should be recognized as array";

    auto arr = arrayResult.getArray();
    ASSERT_NE(arr, nullptr) << "Array should not be null";
    EXPECT_EQ(arr->elements.size(), 3u) << "Array should have 3 elements";

    auto firstElement = arrayResult.getArrayElement(0);
    bool isFirstNumber = std::holds_alternative<double>(firstElement) || std::holds_alternative<int64_t>(firstElement);
    EXPECT_TRUE(isFirstNumber) << "First element should be number";

    double firstValue = 0.0;
    if (std::holds_alternative<double>(firstElement)) {
        firstValue = std::get<double>(firstElement);
    } else if (std::holds_alternative<int64_t>(firstElement)) {
        firstValue = static_cast<double>(std::get<int64_t>(firstElement));
    }
    EXPECT_EQ(firstValue, 1.0) << "First element should be 1";

    auto secondElement = arrayResult.getArrayElement(1);
    EXPECT_TRUE(std::holds_alternative<std::string>(secondElement)) << "Second element should be string";
    EXPECT_EQ(std::get<std::string>(secondElement), "hello") << "Second element should be 'hello'";

    auto thirdElement = arrayResult.getArrayElement(2);
    EXPECT_TRUE(std::holds_alternative<bool>(thirdElement)) << "Third element should be boolean";
    EXPECT_EQ(std::get<bool>(thirdElement), true) << "Third element should be true";

    // Test nested structures
    auto nestedResult = engine_->evaluateExpression(sessionId_, "{data: [1, 2, 3], info: {count: 3}}").get();
    ASSERT_TRUE(nestedResult.isSuccess()) << "Nested structure should be evaluable";
    EXPECT_TRUE(nestedResult.isObject()) << "Nested result should be object";

    auto dataProperty = nestedResult.getObjectProperty("data");
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<ScriptArray>>(dataProperty)) << "Data should be array";

    auto dataArray = std::get<std::shared_ptr<ScriptArray>>(dataProperty);
    EXPECT_EQ(dataArray->elements.size(), 3u) << "Data array should have 3 elements";

    auto infoProperty = nestedResult.getObjectProperty("info");
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<ScriptObject>>(infoProperty)) << "Info should be object";

    auto infoObject = std::get<std::shared_ptr<ScriptObject>>(infoProperty);
    EXPECT_EQ(infoObject->properties.size(), 1u) << "Info object should have 1 property";

    // Test array of objects
    auto arrayOfObjectsResult =
        engine_->evaluateExpression(sessionId_, "[{id: 1, name: 'first'}, {id: 2, name: 'second'}]").get();
    ASSERT_TRUE(arrayOfObjectsResult.isSuccess()) << "Array of objects should be evaluable";
    EXPECT_TRUE(arrayOfObjectsResult.isArray()) << "Result should be array";

    auto objArray = arrayOfObjectsResult.getArray();
    EXPECT_EQ(objArray->elements.size(), 2u) << "Array should have 2 objects";

    auto firstObj = objArray->elements[0];
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<ScriptObject>>(firstObj)) << "First element should be object";

    auto firstScriptObj = std::get<std::shared_ptr<ScriptObject>>(firstObj);
    EXPECT_EQ(firstScriptObj->properties.size(), 2u) << "First object should have 2 properties";

    // Test SCXML-style data model operations
    auto scxmlDataResult =
        engine_
            ->executeScript(sessionId_, "var entry_sequence = []; entry_sequence.push('parallel_entry'); "
                                        "entry_sequence.push('child1_entry'); entry_sequence")
            .get();
    ASSERT_TRUE(scxmlDataResult.isSuccess()) << "SCXML-style data model should work";
    EXPECT_TRUE(scxmlDataResult.isArray()) << "Result should be array";

    auto entryArray = scxmlDataResult.getArray();
    EXPECT_EQ(entryArray->elements.size(), 2u) << "Entry sequence should have 2 elements";

    auto firstEntry = entryArray->elements[0];
    EXPECT_TRUE(std::holds_alternative<std::string>(firstEntry)) << "First entry should be string";
    EXPECT_EQ(std::get<std::string>(firstEntry), "parallel_entry") << "First entry should be 'parallel_entry'";

    auto secondEntry = entryArray->elements[1];
    EXPECT_TRUE(std::holds_alternative<std::string>(secondEntry)) << "Second entry should be string";
    EXPECT_EQ(std::get<std::string>(secondEntry), "child1_entry") << "Second entry should be 'child1_entry'";
}

TEST_F(JSEngineBasicTest, W3C_InFunction_StateMachineIntegration_ShouldReturnCorrectStateStatus) {
    // Test that In() function can correctly check StateMachine state status

    // First, verify In() function exists and returns false when no StateMachine is registered
    expectExpressionType("In", "function");

    // Should return false for any state when no StateMachine is connected
    expectExpressionValue("In('idle')", false);

    // Create a simple SCXML for testing
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="idle">
    <state id="idle">
        <transition event="start" target="running"/>
    </state>
    <state id="running">
        <transition event="stop" target="idle"/>
    </state>
</scxml>)";

    // Create StateMachine with controlled scope for proper lifecycle management
    {
        auto sm = std::make_unique<RSM::StateMachine>();
        ASSERT_TRUE(sm->loadSCXMLFromString(scxml)) << "Failed to load SCXML";
        ASSERT_TRUE(sm->start()) << "Failed to start StateMachine";

        // All state checks must be performed while StateMachine is alive and registered
        expectExpressionValue("In('idle')", true);      // StateMachine should be in 'idle' state initially
        expectExpressionValue("In('running')", false);  // StateMachine should NOT be in 'running' state initially

        // Test state transition
        sm->processEvent("start", "");
        expectExpressionValue("In('idle')", false);    // Should no longer be in 'idle'
        expectExpressionValue("In('running')", true);  // Should now be in 'running'

        sm->stop();
        // StateMachine is still registered but stopped - In() should reflect this
    }  // StateMachine destroyed here, automatically unregistered from JSEngine

    // After StateMachine destruction, In() should return false for any state
    expectExpressionValue("In('idle')", false);
    expectExpressionValue("In('running')", false);
}

TEST_F(JSEngineBasicTest, W3C_ForeachAction_ArrayExpressionEvaluation) {
    // Test array expressions used in SCXML foreach
    // This test validates patterns used in ForeachAction's parseArrayExpression

    // 1. Basic number array expression (for ForeachAction failure analysis)
    auto numberArrayResult = engine_->evaluateExpression(sessionId_, "[1, 2, 3]").get();
    ASSERT_TRUE(numberArrayResult.isSuccess()) << "Number array expression evaluation failed";

    // Check return value type and contents
    if (std::holds_alternative<std::string>(numberArrayResult.getInternalValue())) {
        std::string resultStr = std::get<std::string>(numberArrayResult.getInternalValue());
        LOG_DEBUG("Array result (string): '{}'", resultStr);
        LOG_DEBUG("Length: {}", resultStr.length());
        if (!resultStr.empty()) {
            LOG_DEBUG("First char: '{}' (ASCII: {})", resultStr.front(), (int)resultStr.front());
            LOG_DEBUG("Last char: '{}' (ASCII: {})", resultStr.back(), (int)resultStr.back());
        }
    } else if (std::holds_alternative<long>(numberArrayResult.getInternalValue())) {
        LOG_DEBUG("Array result (integer): {}", std::get<long>(numberArrayResult.getInternalValue()));
    } else if (std::holds_alternative<double>(numberArrayResult.getInternalValue())) {
        LOG_DEBUG("Array result (double): {}", std::get<double>(numberArrayResult.getInternalValue()));
    } else if (std::holds_alternative<bool>(numberArrayResult.getInternalValue())) {
        LOG_DEBUG("Array result (boolean): {}", std::get<bool>(numberArrayResult.getInternalValue()));
    }

    // 2. String array expression
    auto stringArrayResult = engine_->evaluateExpression(sessionId_, "['first', 'second', 'third']").get();
    ASSERT_TRUE(stringArrayResult.isSuccess()) << "String array expression evaluation failed";

    // 3. Array access via variable
    auto varArraySetup = engine_->executeScript(sessionId_, "var testArray = [1, 2, 3]; testArray").get();
    ASSERT_TRUE(varArraySetup.isSuccess()) << "Array variable setup failed";

    auto varArrayResult = engine_->evaluateExpression(sessionId_, "testArray").get();
    ASSERT_TRUE(varArrayResult.isSuccess()) << "Array variable evaluation failed";

    // 4. Object.values() expression (complex array generation)
    auto objectValuesResult =
        engine_->evaluateExpression(sessionId_, "Object.values({a: 'first', b: 'second', c: 'third'})").get();
    ASSERT_TRUE(objectValuesResult.isSuccess()) << "Object.values expression evaluation failed";

    // 5. Empty array expression
    auto emptyArrayResult = engine_->evaluateExpression(sessionId_, "[]").get();
    ASSERT_TRUE(emptyArrayResult.isSuccess()) << "Empty array expression evaluation failed";

    // 6. Array length check (used in foreach to determine iteration count)
    auto lengthCheckResult = engine_->evaluateExpression(sessionId_, "[1, 2, 3].length").get();
    ASSERT_TRUE(lengthCheckResult.isSuccess()) << "Array length check failed";
    if (lengthCheckResult.isSuccess()) {
        EXPECT_EQ(lengthCheckResult.getValue<double>(), 3.0) << "Array length is not 3";
    }

    // 7. Individual array element access (used in foreach iteration)
    auto elementAccessResult1 = engine_->evaluateExpression(sessionId_, "[1, 2, 3][0]").get();
    ASSERT_TRUE(elementAccessResult1.isSuccess()) << "Array first element access failed";

    auto elementAccessResult2 = engine_->evaluateExpression(sessionId_, "[1, 2, 3][1]").get();
    ASSERT_TRUE(elementAccessResult2.isSuccess()) << "Array second element access failed";

    // 8. Array string conversion via JSON.stringify (for debugging)
    auto stringifyResult = engine_->evaluateExpression(sessionId_, "JSON.stringify([1, 2, 3])").get();
    ASSERT_TRUE(stringifyResult.isSuccess()) << "JSON.stringify conversion failed";
    if (stringifyResult.isSuccess()) {
        std::string jsonString = stringifyResult.getValue<std::string>();
        LOG_DEBUG("JSON.stringify result: '{}'", jsonString);
        EXPECT_EQ(jsonString, "[1,2,3]") << "JSON string differs from expected";
    }
}

// ===================================================================
// INTEGRATED API TESTS: JSEngine built-in result processing
// ===================================================================

TEST_F(JSEngineBasicTest, IntegratedAPI_ResultConversion) {
    // Test the new integrated result conversion API that eliminates code duplication

    // Test boolean conversion
    auto boolResult = engine_->evaluateExpression(sessionId_, "true").get();
    ASSERT_TRUE(boolResult.isSuccess()) << "Boolean evaluation failed";
    bool converted = RSM::JSEngine::resultToBool(boolResult);
    EXPECT_TRUE(converted) << "Boolean conversion failed";

    // Test string conversion with different types
    auto numberResult = engine_->evaluateExpression(sessionId_, "42").get();
    ASSERT_TRUE(numberResult.isSuccess()) << "Number evaluation failed";
    std::string numberStr = RSM::JSEngine::resultToString(numberResult);
    EXPECT_EQ(numberStr, "42") << "Number to string conversion failed";

    auto doubleResult = engine_->evaluateExpression(sessionId_, "3.14").get();
    ASSERT_TRUE(doubleResult.isSuccess()) << "Double evaluation failed";
    std::string doubleStr = RSM::JSEngine::resultToString(doubleResult);
    EXPECT_EQ(doubleStr, "3.14") << "Double to string conversion failed";

    auto boolStrResult = engine_->evaluateExpression(sessionId_, "false").get();
    ASSERT_TRUE(boolStrResult.isSuccess()) << "Boolean string evaluation failed";
    std::string boolStr = RSM::JSEngine::resultToString(boolStrResult);
    EXPECT_EQ(boolStr, "false") << "Boolean to string conversion failed";

    // Test template-based typed conversion
    auto typedNumber = RSM::JSEngine::resultToValue<double>(doubleResult);
    ASSERT_TRUE(typedNumber.has_value()) << "Typed double conversion failed";
    EXPECT_DOUBLE_EQ(typedNumber.value(), 3.14) << "Typed double value mismatch";

    auto typedBool = RSM::JSEngine::resultToValue<bool>(boolResult);
    ASSERT_TRUE(typedBool.has_value()) << "Typed boolean conversion failed";
    EXPECT_TRUE(typedBool.value()) << "Typed boolean value mismatch";
}

TEST_F(JSEngineBasicTest, IntegratedAPI_JSONStringifyFallback) {
    // Test JSON.stringify fallback for complex objects - reuses proven ActionExecutorImpl logic

    auto objResult = engine_->evaluateExpression(sessionId_, "{name: 'test', value: 123}").get();
    ASSERT_TRUE(objResult.isSuccess()) << "Object evaluation failed";

    // Test string conversion with JSON.stringify fallback
    std::string objStr = RSM::JSEngine::resultToString(objResult, sessionId_, "{name: 'test', value: 123}");
    EXPECT_FALSE(objStr.empty()) << "Object to string conversion returned empty";

    // Should contain JSON representation or fallback
    EXPECT_TRUE(objStr.find("test") != std::string::npos || objStr.find("[object]") != std::string::npos)
        << "Object conversion should contain 'test' or '[object]' fallback";
}

TEST_F(JSEngineBasicTest, IntegratedAPI_ErrorHandling) {
    // Test error handling with integrated API

    // Test with failed result
    auto failedResult = engine_->evaluateExpression(sessionId_, "nonexistent_variable").get();
    EXPECT_FALSE(RSM::JSEngine::isSuccess(failedResult)) << "Should fail for nonexistent variable";

    // Boolean conversion of failed result should return false
    bool failedBool = RSM::JSEngine::resultToBool(failedResult);
    EXPECT_FALSE(failedBool) << "Failed result should convert to false";

    // String conversion of failed result should return empty
    std::string failedStr = RSM::JSEngine::resultToString(failedResult);
    EXPECT_TRUE(failedStr.empty()) << "Failed result should convert to empty string";

    // Template conversion should return nullopt
    auto failedTyped = RSM::JSEngine::resultToValue<double>(failedResult);
    EXPECT_FALSE(failedTyped.has_value()) << "Failed result should return nullopt for typed conversion";

    // Test requireSuccess with failed result
    EXPECT_THROW(RSM::JSEngine::requireSuccess(failedResult, "test operation"), std::runtime_error)
        << "requireSuccess should throw for failed result";
}

TEST_F(JSEngineBasicTest, W3C_VariablePersistence_ExecuteScriptConsistency) {
    // Test case to verify that variables defined in executeScript() persist across multiple calls
    // This ensures SCXML W3C compliance for JavaScript variable persistence

    // Initialize variables using executeScript - similar to history test pattern
    auto initResult =
        engine_->executeScript(sessionId_, "var workflow_state = ''; var step_count = 0; step_count").get();
    ASSERT_TRUE(initResult.isSuccess()) << "Initial variable setup should succeed";
    EXPECT_EQ(initResult.getValue<int64_t>(), 0) << "Initial step_count should be 0";

    // First step: modify both string and numeric variables
    auto step1Result =
        engine_->executeScript(sessionId_, "workflow_state += '_step1'; step_count += 1; step_count").get();
    ASSERT_TRUE(step1Result.isSuccess()) << "Step 1 execution should succeed";
    EXPECT_EQ(step1Result.getValue<int64_t>(), 1) << "step_count should be 1 after first increment";

    // Verify string variable persistence using evaluateExpression
    auto stringCheckResult = engine_->evaluateExpression(sessionId_, "workflow_state").get();
    ASSERT_TRUE(stringCheckResult.isSuccess()) << "String variable check should succeed";
    EXPECT_EQ(stringCheckResult.getValue<std::string>(), "_step1") << "workflow_state should contain '_step1'";

    // Second step: continue modifying variables
    auto step2Result =
        engine_->executeScript(sessionId_, "workflow_state += '_step2'; step_count += 1; step_count").get();
    ASSERT_TRUE(step2Result.isSuccess()) << "Step 2 execution should succeed";
    EXPECT_EQ(step2Result.getValue<int64_t>(), 2) << "step_count should be 2 after second increment";

    // Third step: continue pattern
    auto step3Result =
        engine_->executeScript(sessionId_, "workflow_state += '_step3'; step_count += 1; step_count").get();
    ASSERT_TRUE(step3Result.isSuccess()) << "Step 3 execution should succeed";
    EXPECT_EQ(step3Result.getValue<int64_t>(), 3) << "step_count should be 3 after third increment";

    // Fourth step: final verification
    auto step4Result =
        engine_->executeScript(sessionId_, "workflow_state += '_step4'; step_count += 1; step_count").get();
    ASSERT_TRUE(step4Result.isSuccess()) << "Step 4 execution should succeed";
    EXPECT_EQ(step4Result.getValue<int64_t>(), 4) << "step_count should be 4 after fourth increment";

    // Final verification of both variables
    auto finalStringResult = engine_->evaluateExpression(sessionId_, "workflow_state").get();
    ASSERT_TRUE(finalStringResult.isSuccess()) << "Final string check should succeed";
    EXPECT_EQ(finalStringResult.getValue<std::string>(), "_step1_step2_step3_step4")
        << "workflow_state should contain all steps";

    auto finalCountResult = engine_->evaluateExpression(sessionId_, "step_count").get();
    ASSERT_TRUE(finalCountResult.isSuccess()) << "Final count check should succeed";
    EXPECT_EQ(finalCountResult.getValue<int64_t>(), 4) << "step_count should be 4 at the end";

    // Test variable type consistency
    auto stepTypeResult = engine_->evaluateExpression(sessionId_, "typeof step_count").get();
    ASSERT_TRUE(stepTypeResult.isSuccess()) << "Type check should succeed";
    EXPECT_EQ(stepTypeResult.getValue<std::string>(), "number") << "step_count should remain a number";

    auto stateTypeResult = engine_->evaluateExpression(sessionId_, "typeof workflow_state").get();
    ASSERT_TRUE(stateTypeResult.isSuccess()) << "String type check should succeed";
    EXPECT_EQ(stateTypeResult.getValue<std::string>(), "string") << "workflow_state should remain a string";
}

// **Regression Prevention Test**: 'in _data' check for numeric variable names
TEST_F(JSEngineBasicTest, W3C_NumericVariableNamesInDataAccess) {
    // Test 150 foreach scenario: numeric variable name generation
    auto createVar4Result = engine_->executeScript(sessionId_, "var _data = {}; _data['4'] = 'test_value';").get();
    ASSERT_TRUE(createVar4Result.isSuccess()) << "Creating numeric variable '4' should succeed";

    auto createVar123Result = engine_->executeScript(sessionId_, "_data['123'] = 42;").get();
    ASSERT_TRUE(createVar123Result.isSuccess()) << "Creating numeric variable '123' should succeed";

    // **Core Verification**: Verify 'varName' in _data syntax works correctly
    auto checkVar4Result = engine_->evaluateExpression(sessionId_, "'4' in _data").get();
    ASSERT_TRUE(checkVar4Result.isSuccess()) << "'4' in _data check should succeed";
    EXPECT_TRUE(checkVar4Result.getValue<bool>()) << "'4' should exist in _data";

    auto checkVar123Result = engine_->evaluateExpression(sessionId_, "'123' in _data").get();
    ASSERT_TRUE(checkVar123Result.isSuccess()) << "'123' in _data check should succeed";
    EXPECT_TRUE(checkVar123Result.getValue<bool>()) << "'123' should exist in _data";

    auto checkNonExistentResult = engine_->evaluateExpression(sessionId_, "'999' in _data").get();
    ASSERT_TRUE(checkNonExistentResult.isSuccess()) << "'999' in _data check should succeed";
    EXPECT_FALSE(checkNonExistentResult.getValue<bool>()) << "'999' should NOT exist in _data";

    // **Regression Prevention**: typeof numeric literal is valid, but shows why it's inappropriate as variable name
    auto typeofLiteralResult = engine_->evaluateExpression(sessionId_, "typeof 4").get();
    ASSERT_TRUE(typeofLiteralResult.isSuccess()) << "typeof 4 (literal) is valid JavaScript";
    EXPECT_EQ(typeofLiteralResult.getValue<std::string>(), "number") << "typeof 4 should return 'number'";

    // However, shows that variable name '4' cannot be accessed directly - our _data access approach is correct
    auto directAccessResult = engine_->evaluateExpression(sessionId_, "4").get();
    ASSERT_TRUE(directAccessResult.isSuccess()) << "Direct access to literal 4 should succeed";
    EXPECT_EQ(directAccessResult.getValue<int64_t>(), 4) << "Direct 4 should be number literal 4, not variable";

    // To access variable '4', must use _data['4'] approach (the correct transformation we implemented)
    EXPECT_NE(directAccessResult.getValue<int64_t>(),
              engine_->evaluateExpression(sessionId_, "_data['4']").get().getValue<std::string>().length())
        << "Direct literal access vs _data variable access should be different";
}

// **Regression Prevention Test**: foreach variable creation and existence check
TEST_F(JSEngineBasicTest, W3C_ForeachVariableCreationAndExistenceCheck) {
    // Initialize SCXML data model
    auto initResult =
        engine_
            ->executeScript(sessionId_, "var _data = {}; _data['1'] = [1,2,3]; _data['2'] = 0; _data['3'] = [1,2,3];")
            .get();
    ASSERT_TRUE(initResult.isSuccess()) << "Data model initialization should succeed";

    // **Scenario 1**: Using existing variable (foreach item="1")
    // Check typeof 1 (W3C-compliant variable creation logic)
    auto checkExisting1Result = engine_->evaluateExpression(sessionId_, "'1' in _data").get();
    ASSERT_TRUE(checkExisting1Result.isSuccess()) << "Checking existing variable '1' should succeed";
    EXPECT_TRUE(checkExisting1Result.getValue<bool>()) << "Variable '1' should already exist";

    // **Scenario 2**: Creating new variable (foreach item="4")
    auto checkNew4Result = engine_->evaluateExpression(sessionId_, "'4' in _data").get();
    ASSERT_TRUE(checkNew4Result.isSuccess()) << "Checking new variable '4' should succeed";
    EXPECT_FALSE(checkNew4Result.getValue<bool>()) << "Variable '4' should NOT exist initially";

    // Simulate foreach execution: create new variable
    auto createNew4Result = engine_->executeScript(sessionId_, "_data['4'] = _data['3'][0];").get();
    ASSERT_TRUE(createNew4Result.isSuccess()) << "Creating new foreach variable '4' should succeed";

    // **Core Verification**: Verify newly created variable exists
    auto verifyNew4Result = engine_->evaluateExpression(sessionId_, "'4' in _data").get();
    ASSERT_TRUE(verifyNew4Result.isSuccess()) << "Verifying new variable '4' should succeed";
    EXPECT_TRUE(verifyNew4Result.getValue<bool>()) << "Variable '4' should now exist after foreach";

    // **Additional Verification**: Verify variable value is correct
    auto getValue4Result = engine_->evaluateExpression(sessionId_, "_data['4']").get();
    ASSERT_TRUE(getValue4Result.isSuccess()) << "Getting value of '4' should succeed";
    EXPECT_EQ(getValue4Result.getValue<int64_t>(), 1) << "Variable '4' should contain first array element";

    // **Scenario 3**: Create index variable (foreach index="5")
    auto createIndex5Result = engine_->executeScript(sessionId_, "_data['5'] = 0;").get();
    ASSERT_TRUE(createIndex5Result.isSuccess()) << "Creating index variable '5' should succeed";

    auto verifyIndex5Result = engine_->evaluateExpression(sessionId_, "'5' in _data").get();
    ASSERT_TRUE(verifyIndex5Result.isSuccess()) << "Verifying index variable '5' should succeed";
    EXPECT_TRUE(verifyIndex5Result.getValue<bool>()) << "Index variable '5' should exist";
}

// ============================================================================
// C++ Function Binding Tests
// ============================================================================

TEST_F(JSEngineBasicTest, CppBinding_RegisterGlobalFunction_SimpleCall) {
    // Register function BEFORE creating session
    bool functionCalled = false;
    engine_->registerGlobalFunction("testFunc", [&functionCalled](const std::vector<ScriptValue> &) {
        functionCalled = true;
        return ScriptValue(42);
    });

    // Recreate session to bind the registered function
    engine_->destroySession(sessionId_);
    engine_->createSession(sessionId_, "");

    auto result = engine_->evaluateExpression(sessionId_, "testFunc()").get();

    ASSERT_TRUE(result.isSuccess()) << "Registered function should be callable from JavaScript";
    EXPECT_TRUE(functionCalled) << "C++ callback should have been invoked";
    EXPECT_EQ(result.getValue<int64_t>(), 42) << "Return value should be 42";
}

TEST_F(JSEngineBasicTest, CppBinding_RegisterGlobalFunction_WithArguments) {
    // Register function BEFORE creating session
    engine_->registerGlobalFunction("add", [](const std::vector<ScriptValue> &args) {
        if (args.size() != 2) {
            return ScriptValue(0);
        }
        int a = std::get<int64_t>(args[0]);
        int b = std::get<int64_t>(args[1]);
        return ScriptValue(static_cast<int64_t>(a + b));
    });

    // Recreate session to bind the registered function
    engine_->destroySession(sessionId_);
    engine_->createSession(sessionId_, "");

    auto result = engine_->evaluateExpression(sessionId_, "add(2, 3)").get();

    ASSERT_TRUE(result.isSuccess()) << "Function with arguments should work";
    EXPECT_EQ(result.getValue<int64_t>(), 5) << "add(2, 3) should return 5";
}

TEST_F(JSEngineBasicTest, CppBinding_RegisterGlobalFunction_StringArguments) {
    // Register function BEFORE creating session
    engine_->registerGlobalFunction("concat", [](const std::vector<ScriptValue> &args) {
        if (args.size() != 2) {
            return ScriptValue(std::string(""));
        }
        std::string a = std::get<std::string>(args[0]);
        std::string b = std::get<std::string>(args[1]);
        return ScriptValue(a + b);
    });

    // Recreate session to bind the registered function
    engine_->destroySession(sessionId_);
    engine_->createSession(sessionId_, "");

    auto result = engine_->evaluateExpression(sessionId_, "concat('Hello', 'World')").get();

    ASSERT_TRUE(result.isSuccess()) << "String function should work";
    EXPECT_EQ(result.getValue<std::string>(), "HelloWorld") << "concat should join strings";
}

TEST_F(JSEngineBasicTest, CppBinding_RegisterGlobalFunction_BooleanReturn) {
    // Register function BEFORE creating session
    engine_->registerGlobalFunction("isEven", [](const std::vector<ScriptValue> &args) {
        if (args.empty()) {
            return ScriptValue(false);
        }
        int64_t num = std::get<int64_t>(args[0]);
        return ScriptValue(num % 2 == 0);
    });

    // Recreate session to bind the registered function
    engine_->destroySession(sessionId_);
    engine_->createSession(sessionId_, "");

    auto result1 = engine_->evaluateExpression(sessionId_, "isEven(4)").get();
    ASSERT_TRUE(result1.isSuccess());
    EXPECT_TRUE(result1.getValue<bool>()) << "isEven(4) should be true";

    auto result2 = engine_->evaluateExpression(sessionId_, "isEven(3)").get();
    ASSERT_TRUE(result2.isSuccess());
    EXPECT_FALSE(result2.getValue<bool>()) << "isEven(3) should be false";
}

TEST_F(JSEngineBasicTest, CppBinding_RegisterGlobalFunction_DoubleArithmetic) {
    // Register function BEFORE creating session
    engine_->registerGlobalFunction("multiply", [](const std::vector<ScriptValue> &args) {
        if (args.size() != 2) {
            return ScriptValue(0.0);
        }
        // Handle both int64_t and double (JS whole numbers become int64_t)
        auto getDouble = [](const ScriptValue &v) -> double {
            if (std::holds_alternative<int64_t>(v)) {
                return static_cast<double>(std::get<int64_t>(v));
            }
            return std::get<double>(v);
        };
        double a = getDouble(args[0]);
        double b = getDouble(args[1]);
        return ScriptValue(a * b);
    });

    // Recreate session to bind the registered function
    engine_->destroySession(sessionId_);
    engine_->createSession(sessionId_, "");

    auto result = engine_->evaluateExpression(sessionId_, "multiply(2.5, 4.0)").get();

    ASSERT_TRUE(result.isSuccess()) << "Double arithmetic should work";
    EXPECT_DOUBLE_EQ(result.getValue<double>(), 10.0) << "2.5 * 4.0 should be 10.0";
}

TEST_F(JSEngineBasicTest, CppBinding_RegisterGlobalFunction_MultipleRegistrations) {
    // Register all functions BEFORE creating session
    engine_->registerGlobalFunction("func1", [](const std::vector<ScriptValue> &) { return ScriptValue(1); });

    engine_->registerGlobalFunction("func2", [](const std::vector<ScriptValue> &) { return ScriptValue(2); });

    engine_->registerGlobalFunction("func3", [](const std::vector<ScriptValue> &) { return ScriptValue(3); });

    // Recreate session to bind all registered functions
    engine_->destroySession(sessionId_);
    engine_->createSession(sessionId_, "");

    auto result1 = engine_->evaluateExpression(sessionId_, "func1()").get();
    auto result2 = engine_->evaluateExpression(sessionId_, "func2()").get();
    auto result3 = engine_->evaluateExpression(sessionId_, "func3()").get();

    ASSERT_TRUE(result1.isSuccess() && result2.isSuccess() && result3.isSuccess());
    EXPECT_EQ(result1.getValue<int64_t>(), 1);
    EXPECT_EQ(result2.getValue<int64_t>(), 2);
    EXPECT_EQ(result3.getValue<int64_t>(), 3);
}

TEST_F(JSEngineBasicTest, CppBinding_RegisterGlobalFunction_UsedInConditions) {
    // Register function BEFORE creating session
    engine_->registerGlobalFunction("checkTemperature", [](const std::vector<ScriptValue> &) {
        return ScriptValue(true);  // Simulate high temperature
    });

    // Recreate session to bind the registered function
    engine_->destroySession(sessionId_);
    engine_->createSession(sessionId_, "");

    // Simulate SCXML condition evaluation
    auto condResult = engine_->evaluateExpression(sessionId_, "checkTemperature() ? 'cooling' : 'idle'").get();

    ASSERT_TRUE(condResult.isSuccess());
    EXPECT_EQ(condResult.getValue<std::string>(), "cooling") << "Function should work in conditional expressions";
}
