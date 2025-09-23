#include "common/Logger.h"
#include "runtime/StateMachine.h"
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
    EXPECT_TRUE(std::holds_alternative<double>(valueProperty)) << "Value should be number";
    EXPECT_EQ(std::get<double>(valueProperty), 42.0) << "Value should be 42";

    // Test array creation and evaluation
    auto arrayResult = engine_->evaluateExpression(sessionId_, "[1, 'hello', true]").get();
    ASSERT_TRUE(arrayResult.isSuccess()) << "Array literal should be evaluable";
    EXPECT_TRUE(arrayResult.isArray()) << "Result should be recognized as array";

    auto arr = arrayResult.getArray();
    ASSERT_NE(arr, nullptr) << "Array should not be null";
    EXPECT_EQ(arr->elements.size(), 3u) << "Array should have 3 elements";

    auto firstElement = arrayResult.getArrayElement(0);
    EXPECT_TRUE(std::holds_alternative<double>(firstElement)) << "First element should be number";
    EXPECT_EQ(std::get<double>(firstElement), 1.0) << "First element should be 1";

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
    auto inTypeResult = engine_->evaluateExpression(sessionId_, "typeof In").get();
    ASSERT_TRUE(inTypeResult.isSuccess());
    EXPECT_EQ(inTypeResult.getValue<std::string>(), "function");

    // Should return false for any state when no StateMachine is connected
    auto noStateMachineResult = engine_->evaluateExpression(sessionId_, "In('idle')").get();
    ASSERT_TRUE(noStateMachineResult.isSuccess());
    EXPECT_FALSE(noStateMachineResult.getValue<bool>())
        << "In() should return false when no StateMachine is registered";

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

    // Create and start StateMachine
    RSM::StateMachine sm;
    ASSERT_TRUE(sm.loadSCXMLFromString(scxml)) << "Failed to load SCXML";
    ASSERT_TRUE(sm.start()) << "Failed to start StateMachine";

    // Now In() should correctly reflect the StateMachine state
    auto idleCheckResult = engine_->evaluateExpression(sessionId_, "In('idle')").get();
    ASSERT_TRUE(idleCheckResult.isSuccess()) << "In('idle') evaluation failed";
    EXPECT_TRUE(idleCheckResult.getValue<bool>()) << "StateMachine should be in 'idle' state initially";

    auto runningCheckResult = engine_->evaluateExpression(sessionId_, "In('running')").get();
    ASSERT_TRUE(runningCheckResult.isSuccess()) << "In('running') evaluation failed";
    EXPECT_FALSE(runningCheckResult.getValue<bool>()) << "StateMachine should NOT be in 'running' state initially";

    // Trigger state transition
    auto transitionResult = sm.processEvent("start");
    ASSERT_TRUE(transitionResult.success) << "Failed to process 'start' event: " << transitionResult.errorMessage;
    EXPECT_EQ(transitionResult.toState, "running") << "Should transition to 'running' state";

    // Verify In() function reflects the new state
    auto idleAfterTransition = engine_->evaluateExpression(sessionId_, "In('idle')").get();
    ASSERT_TRUE(idleAfterTransition.isSuccess()) << "In('idle') evaluation failed after transition";
    EXPECT_FALSE(idleAfterTransition.getValue<bool>()) << "StateMachine should NOT be in 'idle' state after transition";

    auto runningAfterTransition = engine_->evaluateExpression(sessionId_, "In('running')").get();
    ASSERT_TRUE(runningAfterTransition.isSuccess()) << "In('running') evaluation failed after transition";
    EXPECT_TRUE(runningAfterTransition.getValue<bool>())
        << "StateMachine should be in 'running' state after transition";

    // Test with non-existent state
    auto nonExistentResult = engine_->evaluateExpression(sessionId_, "In('nonexistent')").get();
    ASSERT_TRUE(nonExistentResult.isSuccess()) << "In('nonexistent') evaluation failed";
    EXPECT_FALSE(nonExistentResult.getValue<bool>()) << "In() should return false for non-existent states";

    // Stop StateMachine and verify cleanup
    sm.stop();

    // After stopping, In() should return false again (StateMachine unregistered)
    auto afterStopResult = engine_->evaluateExpression(sessionId_, "In('idle')").get();
    ASSERT_TRUE(afterStopResult.isSuccess()) << "In('idle') evaluation failed after stop";
    EXPECT_FALSE(afterStopResult.getValue<bool>()) << "In() should return false after StateMachine is stopped";
}

TEST_F(JSEngineBasicTest, W3C_ForeachAction_ArrayExpressionEvaluation) {
    // SCXML foreach에서 사용하는 배열 표현식들을 테스트
    // 이 테스트는 ForeachAction의 parseArrayExpression에서 사용되는 패턴들을 검증

    // 1. 기본 숫자 배열 표현식 (ForeachAction 실패 원인 분석용)
    auto numberArrayResult = engine_->evaluateExpression(sessionId_, "[1, 2, 3]").get();
    ASSERT_TRUE(numberArrayResult.isSuccess()) << "숫자 배열 표현식 평가 실패";

    // 반환값의 타입과 내용 확인
    if (std::holds_alternative<std::string>(numberArrayResult.getInternalValue())) {
        std::string resultStr = std::get<std::string>(numberArrayResult.getInternalValue());
        RSM::Logger::debug("Array result (string): '{}'", resultStr);
        RSM::Logger::debug("Length: {}", resultStr.length());
        if (!resultStr.empty()) {
            RSM::Logger::debug("First char: '{}' (ASCII: {})", resultStr.front(), (int)resultStr.front());
            RSM::Logger::debug("Last char: '{}' (ASCII: {})", resultStr.back(), (int)resultStr.back());
        }
    } else if (std::holds_alternative<long>(numberArrayResult.getInternalValue())) {
        RSM::Logger::debug("Array result (integer): {}", std::get<long>(numberArrayResult.getInternalValue()));
    } else if (std::holds_alternative<double>(numberArrayResult.getInternalValue())) {
        RSM::Logger::debug("Array result (double): {}", std::get<double>(numberArrayResult.getInternalValue()));
    } else if (std::holds_alternative<bool>(numberArrayResult.getInternalValue())) {
        RSM::Logger::debug("Array result (boolean): {}", std::get<bool>(numberArrayResult.getInternalValue()));
    }

    // 2. 문자열 배열 표현식
    auto stringArrayResult = engine_->evaluateExpression(sessionId_, "['first', 'second', 'third']").get();
    ASSERT_TRUE(stringArrayResult.isSuccess()) << "문자열 배열 표현식 평가 실패";

    // 3. 변수를 통한 배열 접근
    auto varArraySetup = engine_->executeScript(sessionId_, "var testArray = [1, 2, 3]; testArray").get();
    ASSERT_TRUE(varArraySetup.isSuccess()) << "배열 변수 설정 실패";

    auto varArrayResult = engine_->evaluateExpression(sessionId_, "testArray").get();
    ASSERT_TRUE(varArrayResult.isSuccess()) << "배열 변수 평가 실패";

    // 4. Object.values() 표현식 (복잡한 배열 생성)
    auto objectValuesResult =
        engine_->evaluateExpression(sessionId_, "Object.values({a: 'first', b: 'second', c: 'third'})").get();
    ASSERT_TRUE(objectValuesResult.isSuccess()) << "Object.values 표현식 평가 실패";

    // 5. 빈 배열 표현식
    auto emptyArrayResult = engine_->evaluateExpression(sessionId_, "[]").get();
    ASSERT_TRUE(emptyArrayResult.isSuccess()) << "빈 배열 표현식 평가 실패";

    // 6. 배열 길이 확인 (foreach에서 반복 횟수 결정에 사용)
    auto lengthCheckResult = engine_->evaluateExpression(sessionId_, "[1, 2, 3].length").get();
    ASSERT_TRUE(lengthCheckResult.isSuccess()) << "배열 길이 확인 실패";
    if (lengthCheckResult.isSuccess()) {
        EXPECT_EQ(lengthCheckResult.getValue<double>(), 3.0) << "배열 길이가 3이 아님";
    }

    // 7. 배열 요소 개별 접근 (foreach 반복에서 사용)
    auto elementAccessResult1 = engine_->evaluateExpression(sessionId_, "[1, 2, 3][0]").get();
    ASSERT_TRUE(elementAccessResult1.isSuccess()) << "배열 첫 번째 요소 접근 실패";

    auto elementAccessResult2 = engine_->evaluateExpression(sessionId_, "[1, 2, 3][1]").get();
    ASSERT_TRUE(elementAccessResult2.isSuccess()) << "배열 두 번째 요소 접근 실패";

    // 8. JSON.stringify를 통한 배열 문자열 변환 (디버깅용)
    auto stringifyResult = engine_->evaluateExpression(sessionId_, "JSON.stringify([1, 2, 3])").get();
    ASSERT_TRUE(stringifyResult.isSuccess()) << "JSON.stringify 변환 실패";
    if (stringifyResult.isSuccess()) {
        std::string jsonString = stringifyResult.getValue<std::string>();
        RSM::Logger::debug("JSON.stringify result: '{}'", jsonString);
        EXPECT_EQ(jsonString, "[1,2,3]") << "JSON 문자열이 예상과 다름";
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