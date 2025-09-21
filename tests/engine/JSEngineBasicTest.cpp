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
    ASSERT_TRUE(result.isSuccess()) << "Failed to evaluate expression: " << result.errorMessage;
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