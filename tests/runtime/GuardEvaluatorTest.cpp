#include "generator/include/runtime/GuardEvaluator.h"
#include "generator/include/core/NodeFactory.h"
#include "generator/include/core/TransitionNode.h"
#include "generator/include/events/Event.h"
#include "generator/include/model/DocumentModel.h"
#include "generator/include/parsing/DocumentParser.h"
#include "generator/include/runtime/Processor.h"
#include "generator/include/runtime/RuntimeContext.h"
#include "generator/include/runtime/StateMachineFactory.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace SCXML;

class GuardEvaluatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create parser for SCXML content
        auto nodeFactory = std::make_shared<Core::NodeFactory>();
        parser = std::make_unique<Parsing::DocumentParser>(nodeFactory, nullptr);

        // Create a proper SCXML document with ECMAScript datamodel
        createTestSCXML();

        // Parse and create runtime with proper ECMAScript engine integration
        setupECMAScriptRuntime();

        // Extract GuardEvaluator from properly configured runtime
        extractGuardEvaluator();

        // Create test transition and event
        transition = std::make_shared<Core::TransitionNode>("test.event", "targetState");
        testEvent = std::make_shared<Events::Event>("test.event");
    }

    void TearDown() override {
        processor.reset();
        context.reset();
        evaluator.reset();
        transition.reset();
        testEvent.reset();
        parser.reset();
    }

    void createTestSCXML() {
        // Create a proper SCXML document with ECMAScript datamodel and test variables
        testSCXML = R"(<?xml version="1.0" encoding="UTF-8"?>
        <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start" datamodel="ecmascript">
            <datamodel>
                <data id="counter" expr="6"/>
                <data id="step" expr="2"/>
                <data id="name" expr="'test'"/>
                <data id="active" expr="'true'"/>
                <data id="score" expr="95.5"/>
                <data id="threshold" expr="10"/>
                <data id="empty" expr="''"/>
            </datamodel>
            <state id="start">
                <transition event="test.event" target="end"/>
            </state>
            <final id="end"/>
        </scxml>)";
    }

    void setupECMAScriptRuntime() {
        // Parse the SCXML document
        auto model = parser->parseContent(testSCXML);
        ASSERT_TRUE(model != nullptr) << "Failed to parse test SCXML";
        ASSERT_FALSE(parser->hasErrors()) << "Parser errors occurred";

        // Create StateMachine with ECMAScript support (same pattern as ActionSystemIntegrationTest)
        auto options = Runtime::StateMachineFactory::getDefaultOptions();
        options.name = "GuardEvaluatorTest";
        options.enableLogging = true;
        options.enableEventTracing = true;
        options.validateModel = true;

        // StateMachineFactory automatically enables QuickJS for ECMAScript datamodel
        auto result = Runtime::StateMachineFactory::create(model, options);
        ASSERT_TRUE(result.isValid()) << "Failed to create StateMachine runtime";

        processor = result.runtime;
        ASSERT_TRUE(processor != nullptr) << "Runtime processor is null";

        // Initialize the processor to ensure all components are properly set up
        ASSERT_TRUE(processor->start()) << "Failed to start processor";
    }

    void extractGuardEvaluator() {
        // Get the runtime context from the properly configured processor
        context = processor->getContext();
        ASSERT_TRUE(context != nullptr) << "Runtime context is null";

        // Verify ECMAScript engine is properly configured
        auto dataEngine = context->getDataModelEngine();
        ASSERT_TRUE(dataEngine != nullptr) << "DataModel engine should be configured";

        // Create a new GuardEvaluator and configure it with the ECMAScript engine
        evaluator = std::make_unique<GuardEvaluator>();
        evaluator->setDataModelEngine(dataEngine);

        // Also set up the expression evaluator
        auto expressionEvaluator = std::make_shared<Runtime::ExpressionEvaluator>();
        evaluator->setExpressionEvaluator(expressionEvaluator);
    }

    GuardEvaluator::GuardResult evaluateGuard(const std::string &condition) {
        transition->setGuard(condition);

        GuardEvaluator::GuardContext guardContext;
        guardContext.currentEvent = testEvent;
        guardContext.sourceState = "sourceState";
        guardContext.targetState = "targetState";
        guardContext.runtimeContext = context.get();

        return evaluator->evaluateTransitionGuard(transition, guardContext);
    }

protected:
    std::unique_ptr<Parsing::DocumentParser> parser;
    std::shared_ptr<Processor> processor;
    std::shared_ptr<Runtime::RuntimeContext> context;
    std::unique_ptr<GuardEvaluator> evaluator;
    std::shared_ptr<Core::TransitionNode> transition;
    std::shared_ptr<Events::Event> testEvent;
    std::string testSCXML;
};

// ========== Basic Equality Comparison Tests ==========

TEST_F(GuardEvaluatorTest, NumericEqualityComparison) {
    auto result = evaluateGuard("counter == 6");
    EXPECT_TRUE(result.satisfied) << "counter == 6 should be true";
    EXPECT_TRUE(result.hasGuard) << "Should detect guard condition";

    result = evaluateGuard("counter == 5");
    EXPECT_FALSE(result.satisfied) << "counter == 5 should be false";

    result = evaluateGuard("step == 2");
    EXPECT_TRUE(result.satisfied) << "step == 2 should be true";
}

TEST_F(GuardEvaluatorTest, StringEqualityComparison) {
    auto result = evaluateGuard("name == 'test'");
    EXPECT_TRUE(result.satisfied) << "name == 'test' should be true";

    result = evaluateGuard("name == \"test\"");
    EXPECT_TRUE(result.satisfied) << "name == \"test\" should be true";

    result = evaluateGuard("name == 'wrong'");
    EXPECT_FALSE(result.satisfied) << "name == 'wrong' should be false";
}

TEST_F(GuardEvaluatorTest, BooleanEqualityComparison) {
    auto result = evaluateGuard("active == 'true'");
    EXPECT_TRUE(result.satisfied) << "active == 'true' should be true";

    result = evaluateGuard("active == 'false'");
    EXPECT_FALSE(result.satisfied) << "active == 'false' should be false";
}

// ========== Extended Comparison Operators Tests ==========
// Note: These tests verify current behavior and will help identify
// when extended operators are implemented

TEST_F(GuardEvaluatorTest, ExtendedOperatorsCurrentBehavior) {
    // Test what happens with operators not currently supported
    // This helps document current limitations and provides regression tests

    auto result = evaluateGuard("counter > 5");
    // Current implementation may not support this - test documents behavior
    EXPECT_TRUE(result.hasGuard) << "Should still detect as guard condition";

    result = evaluateGuard("step < 5");
    EXPECT_TRUE(result.hasGuard) << "Should still detect as guard condition";

    result = evaluateGuard("counter != 5");
    EXPECT_TRUE(result.hasGuard) << "Should still detect as guard condition";
}

// ========== Complex Expressions Tests ==========

TEST_F(GuardEvaluatorTest, ComplexExpressionsCurrentBehavior) {
    // Test how current implementation handles complex expressions
    auto result = evaluateGuard("counter == 6 && step == 2");
    EXPECT_TRUE(result.hasGuard) << "Should detect complex expression as guard";

    result = evaluateGuard("counter == 6 || step == 3");
    EXPECT_TRUE(result.hasGuard) << "Should detect OR expression as guard";

    result = evaluateGuard("!(counter == 5)");
    EXPECT_TRUE(result.hasGuard) << "Should detect NOT expression as guard";
}

// ========== Error Handling Tests ==========

TEST_F(GuardEvaluatorTest, InvalidExpressionHandling) {
    auto result = evaluateGuard("invalid..syntax((");
    // Guard evaluator should handle gracefully
    EXPECT_TRUE(result.hasGuard) << "Should still detect as guard condition";

    result = evaluateGuard("");
    EXPECT_FALSE(result.hasGuard) << "Empty expression should not be detected as guard";

    result = evaluateGuard("   ");
    EXPECT_FALSE(result.hasGuard) << "Whitespace-only expression should not be guard";
}

TEST_F(GuardEvaluatorTest, UndefinedVariableHandling) {
    auto result = evaluateGuard("undefinedVar == 'test'");
    EXPECT_TRUE(result.hasGuard) << "Should detect as guard even with undefined variable";

    result = evaluateGuard("undefinedVar == undefinedVar");
    EXPECT_TRUE(result.hasGuard) << "Should detect comparison of undefined variables as guard";
}

// ========== Data Type Handling Tests ==========

TEST_F(GuardEvaluatorTest, FloatingPointComparison) {
    auto result = evaluateGuard("score == 95.5");
    EXPECT_TRUE(result.satisfied) << "Floating point equality should work";
    EXPECT_TRUE(result.hasGuard) << "Should detect guard condition";
}

TEST_F(GuardEvaluatorTest, EmptyStringHandling) {
    auto result = evaluateGuard("empty == ''");
    EXPECT_TRUE(result.satisfied) << "Empty string comparison should work";

    result = evaluateGuard("empty == \"\"");
    EXPECT_TRUE(result.satisfied) << "Empty string with double quotes should work";
}

// ========== Edge Cases Tests ==========

TEST_F(GuardEvaluatorTest, WhitespaceHandling) {
    auto result = evaluateGuard("  counter   ==   6  ");
    EXPECT_TRUE(result.satisfied) << "Whitespace around expression should be handled";

    result = evaluateGuard("counter==6");
    EXPECT_TRUE(result.satisfied) << "No whitespace should work";

    result = evaluateGuard("counter == 6   ");
    EXPECT_TRUE(result.satisfied) << "Trailing whitespace should work";
}

TEST_F(GuardEvaluatorTest, TypeMismatchHandling) {
    // Test comparing number with string (type coercion)
    auto result = evaluateGuard("counter == '6'");
    EXPECT_TRUE(result.satisfied) << "Number to string comparison should work";

    result = evaluateGuard("'6' == counter");
    // This will test current string comparison logic
    EXPECT_TRUE(result.hasGuard) << "Should detect as guard condition";
}

// ========== Integration Tests ==========

TEST_F(GuardEvaluatorTest, ContextIntegration) {
    // Test that guard evaluator properly uses runtime context
    auto &dataManager = context->getDataContextManager();
    dataManager.setDataValue("dynamicVar", "testValue");

    auto result = evaluateGuard("dynamicVar == 'testValue'");
    EXPECT_TRUE(result.satisfied) << "Dynamic variable from context should work";

    // Update value and test again
    dataManager.setDataValue("dynamicVar", "newValue");
    result = evaluateGuard("dynamicVar == 'newValue'");
    EXPECT_TRUE(result.satisfied) << "Updated variable value should work";
}

TEST_F(GuardEvaluatorTest, MultipleEvaluations) {
    // Test evaluating multiple guards in sequence
    auto result1 = evaluateGuard("counter == 6");
    auto result2 = evaluateGuard("step == 2");
    auto result3 = evaluateGuard("name == 'test'");

    EXPECT_TRUE(result1.satisfied) << "First guard should pass";
    EXPECT_TRUE(result2.satisfied) << "Second guard should pass";
    EXPECT_TRUE(result3.satisfied) << "Third guard should pass";

    EXPECT_TRUE(result1.hasGuard) << "First should be detected as guard";
    EXPECT_TRUE(result2.hasGuard) << "Second should be detected as guard";
    EXPECT_TRUE(result3.hasGuard) << "Third should be detected as guard";
}

// ========== Guard Detection Tests ==========

TEST_F(GuardEvaluatorTest, GuardDetection) {
    // Test basic guard detection functionality
    auto result = evaluateGuard("counter == 6");
    EXPECT_TRUE(result.hasGuard) << "Should detect simple equality as guard";
    EXPECT_FALSE(result.guardExpression.empty()) << "Should capture guard expression";

    // Test no guard case
    transition->setGuard("");
    GuardEvaluator::GuardContext guardContext;
    guardContext.runtimeContext = context.get();
    result = evaluator->evaluateTransitionGuard(transition, guardContext);
    EXPECT_FALSE(result.hasGuard) << "Should not detect empty string as guard";
    EXPECT_TRUE(result.satisfied) << "No guard should default to satisfied";
}

// ========== Performance Tests ==========

TEST_F(GuardEvaluatorTest, RepeatedEvaluations) {
    // Test that repeated evaluations work correctly
    const int iterations = 100;
    int successCount = 0;

    for (int i = 0; i < iterations; i++) {
        auto result = evaluateGuard("counter == 6");
        if (result.satisfied) {
            successCount++;
        }
    }

    EXPECT_EQ(successCount, iterations) << "All iterations should succeed";
}

TEST_F(GuardEvaluatorTest, DifferentExpressions) {
    // Test various expression patterns to ensure robustness
    std::vector<std::string> expressions = {"counter == 6",     "step == 2",     "name == 'test'",
                                            "active == 'true'", "score == 95.5", "empty == ''"};

    for (const auto &expr : expressions) {
        auto result = evaluateGuard(expr);
        EXPECT_TRUE(result.hasGuard) << "Expression '" << expr << "' should be detected as guard";
        EXPECT_TRUE(result.satisfied) << "Expression '" << expr << "' should be satisfied";
    }
}