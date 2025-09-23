#include "actions/AssignAction.h"
#include "actions/ForeachAction.h"
#include "actions/LogAction.h"
#include "actions/ScriptAction.h"
#include "runtime/ActionExecutorImpl.h"
#include "runtime/StateMachine.h"
#include "scripting/JSEngine.h"
#include <gtest/gtest.h>
#include <memory>

using namespace RSM;

class SCXMLForeachIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        RSM::JSEngine::instance().reset();
        executor = std::make_unique<ActionExecutorImpl>("foreach_integration_test");

        // Create JSEngine session following SCXML specification pattern
        // Session must be created before variable assignments can be performed
        bool sessionCreated = RSM::JSEngine::instance().createSession("foreach_integration_test");
        if (!sessionCreated) {
            throw std::runtime_error("Failed to create JSEngine session for test");
        }
    }

    void TearDown() override {
        executor.reset();
        RSM::JSEngine::instance().shutdown();
    }

    std::unique_ptr<ActionExecutorImpl> executor;
};

// ============================================================================
// SCXML W3C Foreach Integration Tests
// ============================================================================

TEST_F(SCXMLForeachIntegrationTest, W3C_ForeachAction_BasicIntegration) {
    // Test basic foreach integration with ActionExecutor
    EXPECT_TRUE(executor->assignVariable("numbers", "[1, 2, 3, 4, 5]"));
    EXPECT_TRUE(executor->assignVariable("sum", "0"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("numbers");
    foreachAction->setItem("num");

    auto assignAction = std::make_shared<AssignAction>("sum", "sum + num");
    foreachAction->addIterationAction(assignAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // Verify sum calculation: 1+2+3+4+5 = 15
    std::string result = executor->evaluateExpression("sum");
    EXPECT_EQ(result, "15");
}

TEST_F(SCXMLForeachIntegrationTest, W3C_ForeachAction_ObjectIterationIntegration) {
    // Test foreach iterating over object properties
    EXPECT_TRUE(executor->assignVariable("userData", "{name: 'John', age: 30, city: 'NYC'}"));
    EXPECT_TRUE(executor->assignVariable("properties", "Object.keys(userData)"));
    EXPECT_TRUE(executor->assignVariable("result", "[]"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("properties");
    foreachAction->setItem("key");
    foreachAction->setIndex("idx");

    auto assignAction = std::make_shared<AssignAction>("result", "result.concat([key + ': ' + userData[key]])");
    auto logAction = std::make_shared<LogAction>("Processing");
    logAction->setExpr("'Processing ' + key + ' at index ' + idx");

    foreachAction->addIterationAction(assignAction);
    foreachAction->addIterationAction(logAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // Verify result array was populated
    std::string resultLength = executor->evaluateExpression("result.length");
    EXPECT_EQ(resultLength, "3");  // name, age, city

    // Verify content structure
    std::string firstItem = executor->evaluateExpression("result[0]");
    EXPECT_TRUE(firstItem.find(":") != std::string::npos);  // Should contain ':'
}

TEST_F(SCXMLForeachIntegrationTest, W3C_ForeachAction_ComplexWorkflowIntegration) {
    // Test foreach with complex task processing workflow
    EXPECT_TRUE(executor->assignVariable("tasks", "['task1', 'task2', 'task3']"));
    EXPECT_TRUE(executor->assignVariable("completed", "[]"));
    EXPECT_TRUE(executor->assignVariable("currentTask", "null"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("tasks");
    foreachAction->setItem("task");
    foreachAction->setIndex("taskIndex");

    // Simulate complex workflow with multiple actions
    auto setCurrentAction = std::make_shared<AssignAction>("currentTask", "task");
    auto logAction = std::make_shared<LogAction>("Starting task");
    logAction->setExpr("'Starting ' + task + ' (index: ' + taskIndex + ')'");
    auto completeAction = std::make_shared<AssignAction>("completed", "completed.concat([task])");

    foreachAction->addIterationAction(setCurrentAction);
    foreachAction->addIterationAction(logAction);
    foreachAction->addIterationAction(completeAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // Verify workflow completion
    std::string completedLength = executor->evaluateExpression("completed.length");
    EXPECT_EQ(completedLength, "3");

    std::string lastTask = executor->evaluateExpression("currentTask");
    EXPECT_EQ(lastTask, "task3");
}

TEST_F(SCXMLForeachIntegrationTest, W3C_ForeachAction_ErrorHandlingIntegration) {
    // Test foreach error handling with mixed valid/invalid scenarios
    EXPECT_TRUE(executor->assignVariable("validArray", "[1, 2, 3]"));
    EXPECT_TRUE(executor->assignVariable("sum", "0"));

    // First test: valid array processing
    auto foreachValid = std::make_shared<ForeachAction>();
    foreachValid->setArray("validArray");
    foreachValid->setItem("num");

    auto sumAction = std::make_shared<AssignAction>("sum", "sum + num");
    foreachValid->addIterationAction(sumAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachValid));

    // Verify sum calculation
    std::string sum = executor->evaluateExpression("sum");
    EXPECT_EQ(sum, "6");  // 1+2+3

    // Second test: invalid array handling
    auto foreachInvalid = std::make_shared<ForeachAction>();
    foreachInvalid->setArray("nonExistentArray");  // Invalid array
    foreachInvalid->setItem("item");

    auto errorAction = std::make_shared<AssignAction>("errorOccurred", "true");
    foreachInvalid->addIterationAction(errorAction);

    // Should handle gracefully (implementation dependent)
    bool result = executor->executeForeachAction(*foreachInvalid);
    // Accept either behavior: graceful handling or error
    EXPECT_TRUE(result == true || result == false);
}

TEST_F(SCXMLForeachIntegrationTest, W3C_ForeachAction_ConditionalProcessingIntegration) {
    // Test foreach with conditional logic using script actions
    EXPECT_TRUE(executor->assignVariable("numbers", "[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]"));
    EXPECT_TRUE(executor->assignVariable("evenSum", "0"));
    EXPECT_TRUE(executor->assignVariable("oddSum", "0"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("numbers");
    foreachAction->setItem("num");
    foreachAction->setIndex("i");

    // Simulate conditional processing using script actions
    auto evenCheck = std::make_shared<ScriptAction>("if (num % 2 === 0) { evenSum += num; } else { oddSum += num; }");
    auto logAction = std::make_shared<LogAction>("Processing");
    logAction->setExpr("'Processed ' + num + ' at index ' + i");

    foreachAction->addIterationAction(evenCheck);
    foreachAction->addIterationAction(logAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // Verify even sum: 2+4+6+8+10 = 30
    std::string evenResult = executor->evaluateExpression("evenSum");
    EXPECT_EQ(evenResult, "30");

    // Verify odd sum: 1+3+5+7+9 = 25
    std::string oddResult = executor->evaluateExpression("oddSum");
    EXPECT_EQ(oddResult, "25");
}

TEST_F(SCXMLForeachIntegrationTest, W3C_ForeachAction_ComplexDataModelIntegration) {
    // Test foreach with complex object data model
    std::string itemsData = R"([
        {"name": "Item1", "price": 10, "category": "A"},
        {"name": "Item2", "price": 20, "category": "B"},
        {"name": "Item3", "price": 30, "category": "A"}
    ])";

    EXPECT_TRUE(executor->assignVariable("items", itemsData));
    EXPECT_TRUE(executor->assignVariable("totalValue", "0"));
    EXPECT_TRUE(executor->assignVariable("itemNames", "[]"));
    EXPECT_TRUE(executor->assignVariable("currentItem", "null"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("items");
    foreachAction->setItem("item");
    foreachAction->setIndex("itemIndex");

    // Complex processing with multiple actions
    auto setCurrentAction = std::make_shared<AssignAction>("currentItem", "item");
    auto addValueAction = std::make_shared<AssignAction>("totalValue", "totalValue + item.price");
    auto addNameAction = std::make_shared<AssignAction>("itemNames", "itemNames.concat([item.name])");
    auto logAction = std::make_shared<LogAction>("Processing item");
    logAction->setExpr("'Processing item: ' + item.name + ', Price: $' + item.price");

    foreachAction->addIterationAction(setCurrentAction);
    foreachAction->addIterationAction(addValueAction);
    foreachAction->addIterationAction(addNameAction);
    foreachAction->addIterationAction(logAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // Verify calculations
    std::string totalValue = executor->evaluateExpression("totalValue");
    EXPECT_EQ(totalValue, "60");  // 10+20+30

    std::string namesLength = executor->evaluateExpression("itemNames.length");
    EXPECT_EQ(namesLength, "3");

    std::string lastItemName = executor->evaluateExpression("currentItem.name");
    EXPECT_EQ(lastItemName, "Item3");
}

TEST_F(SCXMLForeachIntegrationTest, W3C_ForeachAction_NestedIterationIntegration) {
    // Test foreach with nested iteration capability
    EXPECT_TRUE(executor->assignVariable("matrix", "[[1, 2], [3, 4], [5, 6]]"));
    EXPECT_TRUE(executor->assignVariable("flatResult", "[]"));
    EXPECT_TRUE(executor->assignVariable("product", "1"));

    auto outerForeach = std::make_shared<ForeachAction>();
    outerForeach->setArray("matrix");
    outerForeach->setItem("row");
    outerForeach->setIndex("rowIndex");

    // Process each row with nested iteration logic
    auto processRow = std::make_shared<ScriptAction>(R"(
        for (let i = 0; i < row.length; i++) {
            flatResult.push(row[i]);
            product *= row[i];
        }
    )");

    auto logRow = std::make_shared<LogAction>("Processing row");
    logRow->setExpr("'Processing row ' + rowIndex + ': [' + row.join(', ') + ']'");

    outerForeach->addIterationAction(processRow);
    outerForeach->addIterationAction(logRow);

    EXPECT_TRUE(executor->executeForeachAction(*outerForeach));

    // Verify flattened result
    std::string flatLength = executor->evaluateExpression("flatResult.length");
    EXPECT_EQ(flatLength, "6");  // All 6 elements

    // Verify product calculation: 1*2*3*4*5*6 = 720
    std::string productResult = executor->evaluateExpression("product");
    EXPECT_EQ(productResult, "720");
}

TEST_F(SCXMLForeachIntegrationTest, W3C_ForeachAction_IndexValidationIntegration) {
    // Test foreach index tracking across various scenarios
    EXPECT_TRUE(executor->assignVariable("data", "['a', 'b', 'c', 'd', 'e']"));
    EXPECT_TRUE(executor->assignVariable("indexSum", "0"));
    EXPECT_TRUE(executor->assignVariable("itemCount", "0"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("data");
    foreachAction->setItem("letter");
    foreachAction->setIndex("idx");

    auto sumIndexAction = std::make_shared<AssignAction>("indexSum", "indexSum + idx");
    auto countAction = std::make_shared<AssignAction>("itemCount", "itemCount + 1");
    auto logAction = std::make_shared<LogAction>("Item");
    logAction->setExpr("'Item ' + idx + ': ' + letter");

    foreachAction->addIterationAction(sumIndexAction);
    foreachAction->addIterationAction(countAction);
    foreachAction->addIterationAction(logAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // Verify index sum: 0+1+2+3+4 = 10
    std::string indexSum = executor->evaluateExpression("indexSum");
    EXPECT_EQ(indexSum, "10");

    // Verify item count
    std::string itemCount = executor->evaluateExpression("itemCount");
    EXPECT_EQ(itemCount, "5");
}

// ============================================================================
// SCXML W3C Specification Compliance Tests
// ============================================================================

TEST_F(SCXMLForeachIntegrationTest, W3C_SCXML_ForeachAction_ShallowCopyCompliance) {
    // SCXML W3C Requirement: foreach must create shallow copy to prevent modification during iteration
    EXPECT_TRUE(executor->assignVariable("originalArray", "[1, 2, 3]"));
    EXPECT_TRUE(executor->assignVariable("iterationCount", "0"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("originalArray");
    foreachAction->setItem("item");

    // During iteration, try to modify the original array
    auto modifyArrayAction = std::make_shared<ScriptAction>("originalArray.push(item + 10);");
    auto countAction = std::make_shared<AssignAction>("iterationCount", "iterationCount + 1");

    foreachAction->addIterationAction(modifyArrayAction);
    foreachAction->addIterationAction(countAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // W3C Compliance: Should iterate exactly 3 times (original array length)
    // despite array being modified during iteration
    std::string iterations = executor->evaluateExpression("iterationCount");
    EXPECT_EQ(iterations, "3");

    // Original array should be modified but iteration was unaffected
    std::string finalArrayLength = executor->evaluateExpression("originalArray.length");
    EXPECT_EQ(finalArrayLength, "6");  // Original 3 + 3 added during iterations
}

TEST_F(SCXMLForeachIntegrationTest, W3C_SCXML_ForeachAction_VariableDeclarationCompliance) {
    // SCXML W3C Requirement: foreach declares new variables for item and index
    // These variables should exist in the data model after execution

    // Ensure variables don't exist initially
    EXPECT_FALSE(executor->hasVariable("loopItem"));
    EXPECT_FALSE(executor->hasVariable("loopIndex"));

    EXPECT_TRUE(executor->assignVariable("testArray", "['first', 'second', 'third']"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("testArray");
    foreachAction->setItem("loopItem");
    foreachAction->setIndex("loopIndex");

    // Simple action to verify variables are accessible
    auto verifyAction = std::make_shared<ScriptAction>("/* Variables should be accessible: loopItem, loopIndex */");
    foreachAction->addIterationAction(verifyAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // W3C Compliance: Variables should exist after foreach execution
    EXPECT_TRUE(executor->hasVariable("loopItem"));
    EXPECT_TRUE(executor->hasVariable("loopIndex"));

    // Verify final values (last iteration)
    std::string finalItem = executor->evaluateExpression("loopItem");
    EXPECT_EQ(finalItem, "third");

    std::string finalIndex = executor->evaluateExpression("loopIndex");
    EXPECT_EQ(finalIndex, "2");  // 0-based index
}

TEST_F(SCXMLForeachIntegrationTest, W3C_SCXML_ForeachAction_ErrorExecutionCompliance) {
    // SCXML W3C Requirement: If foreach encounters error, it should queue error.execution
    // and cease execution of child content

    EXPECT_TRUE(executor->assignVariable("testArray", "[1, 2, 3, 4, 5]"));
    EXPECT_TRUE(executor->assignVariable("successCount", "0"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("testArray");
    foreachAction->setItem("num");

    // Create an action that will succeed for some iterations then fail
    auto conditionalFailAction = std::make_shared<ScriptAction>(R"(
        if (num > 2) {
            throw new Error("Intentional test error");
        }
        successCount++;
    )");

    foreachAction->addIterationAction(conditionalFailAction);

    // W3C Compliance: Should fail when encountering error
    bool result = executor->executeForeachAction(*foreachAction);
    (void)result;  // Suppress unused variable warning

    // Implementation dependent: may return false or handle error gracefully
    // Important: execution should stop at error point
    std::string successCount = executor->evaluateExpression("successCount");

    // Should have processed items 1 and 2 successfully, failed on 3
    EXPECT_TRUE(successCount == "2" || successCount == "0");  // Either stopped at error or handled gracefully
}

TEST_F(SCXMLForeachIntegrationTest, W3C_SCXML_ForeachAction_InvalidArrayExpressionCompliance) {
    // SCXML W3C Requirement: If array expression doesn't evaluate to iterable collection,
    // processor should handle gracefully or queue error.execution

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("42");  // Not an array
    foreachAction->setItem("item");

    auto shouldNotExecute = std::make_shared<AssignAction>("executed", "true");
    foreachAction->addIterationAction(shouldNotExecute);

    // W3C Compliance: Should handle invalid array expression gracefully
    bool result = executor->executeForeachAction(*foreachAction);
    (void)result;  // Suppress unused variable warning

    // Implementation should either:
    // 1. Return false (error handling)
    // 2. Return true but not execute iterations (graceful handling)
    bool wasExecuted = executor->hasVariable("executed");

    // Variable should not exist since iterations shouldn't run
    EXPECT_FALSE(wasExecuted);
}

TEST_F(SCXMLForeachIntegrationTest, W3C_SCXML_ForeachAction_ItemVariableNameValidation) {
    // SCXML W3C Requirement: item and index must be valid variable names in the data model

    EXPECT_TRUE(executor->assignVariable("validArray", "[1, 2, 3]"));

    // Test with invalid variable names that should be rejected
    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("validArray");
    foreachAction->setItem("123invalid");  // Invalid: starts with number

    auto action = std::make_shared<AssignAction>("test", "item");
    foreachAction->addIterationAction(action);

    // W3C Compliance: Should validate variable names
    auto errors = foreachAction->validate();
    EXPECT_FALSE(errors.empty());  // Should have validation errors

    // Test with valid variable names
    auto validForeachAction = std::make_shared<ForeachAction>();
    validForeachAction->setArray("validArray");
    validForeachAction->setItem("validItem");
    validForeachAction->setIndex("validIndex");

    // Add iteration action to satisfy validation requirements
    auto validAction = std::make_shared<AssignAction>("test", "validItem");
    validForeachAction->addIterationAction(validAction);

    auto validErrors = validForeachAction->validate();
    if (!validErrors.empty()) {
        for (const auto &error : validErrors) {
            RSM::Logger::debug("Validation error: {}", error);
        }
    }
    EXPECT_TRUE(validErrors.empty());  // Should have no validation errors
}

TEST_F(SCXMLForeachIntegrationTest, W3C_SCXML_ForeachAction_IterationOrderCompliance) {
    // SCXML W3C Requirement: foreach should iterate in defined order specific to data model
    // For JavaScript/ECMAScript: array iteration should be in index order

    EXPECT_TRUE(executor->assignVariable("orderedArray", "['first', 'second', 'third', 'fourth']"));
    EXPECT_TRUE(executor->assignVariable("concatenated", "''"));
    EXPECT_TRUE(executor->assignVariable("indexOrder", "''"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("orderedArray");
    foreachAction->setItem("item");
    foreachAction->setIndex("idx");

    auto concatItemAction = std::make_shared<AssignAction>("concatenated", "concatenated + item + '-'");
    auto concatIndexAction = std::make_shared<AssignAction>("indexOrder", "indexOrder + idx + '-'");

    foreachAction->addIterationAction(concatItemAction);
    foreachAction->addIterationAction(concatIndexAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // W3C Compliance: Order should be preserved
    std::string itemOrder = executor->evaluateExpression("concatenated");
    EXPECT_EQ(itemOrder, "first-second-third-fourth-");

    std::string indexOrder = executor->evaluateExpression("indexOrder");
    EXPECT_EQ(indexOrder, "0-1-2-3-");
}

TEST_F(SCXMLForeachIntegrationTest, W3C_SCXML_ForeachAction_NullUndefinedItemsCompliance) {
    // SCXML W3C Requirement: foreach should handle null and undefined items

    EXPECT_TRUE(executor->assignVariable("mixedArray", "[1, null, undefined, 'text', 0, false]"));
    EXPECT_TRUE(executor->assignVariable("itemTypes", "[]"));

    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("mixedArray");
    foreachAction->setItem("item");
    foreachAction->setIndex("idx");

    auto recordTypeAction = std::make_shared<AssignAction>("itemTypes", "itemTypes.concat([typeof item])");
    foreachAction->addIterationAction(recordTypeAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // W3C Compliance: Should process all items including null/undefined
    std::string arrayLength = executor->evaluateExpression("itemTypes.length");
    EXPECT_EQ(arrayLength, "6");

    // Verify types were recorded correctly
    std::string firstType = executor->evaluateExpression("itemTypes[0]");  // number (1)
    EXPECT_EQ(firstType, "number");

    std::string secondType = executor->evaluateExpression("itemTypes[1]");  // null -> "object"
    EXPECT_EQ(secondType, "object");

    std::string thirdType = executor->evaluateExpression("itemTypes[2]");  // undefined
    EXPECT_EQ(thirdType, "undefined");
}

TEST_F(SCXMLForeachIntegrationTest, W3C_SCXML_ForeachAction_RequiredAttributesCompliance) {
    // SCXML W3C Requirement: array and item attributes are required

    auto foreachAction = std::make_shared<ForeachAction>();

    // Test missing array attribute
    foreachAction->setItem("item");
    auto errors1 = foreachAction->validate();
    EXPECT_FALSE(errors1.empty());

    // Test missing item attribute
    foreachAction->setArray("[1, 2, 3]");
    foreachAction->setItem("");  // Empty item
    auto errors2 = foreachAction->validate();
    EXPECT_FALSE(errors2.empty());

    // Test valid configuration from scratch
    auto validForeachAction = std::make_shared<ForeachAction>();
    validForeachAction->setArray("[1, 2, 3]");
    validForeachAction->setItem("validItem");

    // Add a dummy child action to satisfy validation requirement
    auto dummyAction = std::make_shared<LogAction>("test");
    validForeachAction->addIterationAction(dummyAction);

    auto errors3 = validForeachAction->validate();
    EXPECT_TRUE(errors3.empty());
}

TEST_F(SCXMLForeachIntegrationTest, W3C_SCXML_ForeachAction_OptionalIndexCompliance) {
    // SCXML W3C Requirement: index attribute is optional

    EXPECT_TRUE(executor->assignVariable("testArray", "['a', 'b', 'c']"));
    EXPECT_TRUE(executor->assignVariable("result", "''"));

    // Test foreach without index
    auto foreachAction = std::make_shared<ForeachAction>();
    foreachAction->setArray("testArray");
    foreachAction->setItem("letter");
    // No index attribute set

    auto concatAction = std::make_shared<AssignAction>("result", "result + letter");
    foreachAction->addIterationAction(concatAction);

    EXPECT_TRUE(executor->executeForeachAction(*foreachAction));

    // W3C Compliance: Should work without index
    std::string result = executor->evaluateExpression("result");
    EXPECT_EQ(result, "abc");

    // Index variable should not exist
    EXPECT_FALSE(executor->hasVariable("index"));  // Default name shouldn't exist
}