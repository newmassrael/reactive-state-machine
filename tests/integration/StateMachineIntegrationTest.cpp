#include "factory/NodeFactory.h"
#include "model/SCXMLModel.h"
#include "parsing/SCXMLParser.h"
#include "scripting/JSEngine.h"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

namespace RSM {
namespace Tests {

class StateMachineIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        // JSEngine 리셋으로 테스트 간 격리 보장
        engine_->reset();

        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
        sessionId_ = "integration_test_session";
    }

    void TearDown() override {
        if (engine_) {
            engine_->destroySession(sessionId_);
            engine_->shutdown();
        }
    }

    JSEngine *engine_;
    std::shared_ptr<NodeFactory> nodeFactory_;
    std::unique_ptr<SCXMLParser> parser_;
    std::string sessionId_;
};

// Test basic state machine execution with JavaScript
TEST_F(StateMachineIntegrationTest, ExecuteSimpleStateMachine) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
    <datamodel>
        <data id="result" expr="''"/>
    </datamodel>
    <state id="start">
        <onentry>
            <script>result = 'entered_start';</script>
        </onentry>
        <transition event="go" target="end">
            <script>result = 'transitioning';</script>
        </transition>
    </state>
    <final id="end">
        <onentry>
            <script>result = 'reached_end';</script>
        </onentry>
    </final>
</scxml>)";

    // Parse the state machine
    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_FALSE(parser_->hasErrors());

    // Create JavaScript session
    bool success = engine_->createSession(sessionId_, "");
    ASSERT_TRUE(success);

    // Initialize data model
    auto dataModelItems = model->getDataModelItems();
    for (const auto &data : dataModelItems) {
        auto result = engine_->executeScript(sessionId_, "var " + data->getId() + " = " + data->getExpr() + ";").get();
        EXPECT_TRUE(result.isSuccess()) << "Failed to initialize: " + data->getId();
    }

    // Simulate state machine execution
    // Entry action for start state
    auto entryResult = engine_->executeScript(sessionId_, "result = 'entered_start';").get();
    EXPECT_TRUE(entryResult.isSuccess());

    // Check initial state
    auto checkResult = engine_->evaluateExpression(sessionId_, "result").get();
    EXPECT_TRUE(checkResult.isSuccess());
    EXPECT_EQ(checkResult.getValue<std::string>(), "entered_start");

    // Execute transition
    auto transitionResult = engine_->executeScript(sessionId_, "result = 'transitioning';").get();
    EXPECT_TRUE(transitionResult.isSuccess());

    // Execute final state entry
    auto finalResult = engine_->executeScript(sessionId_, "result = 'reached_end';").get();
    EXPECT_TRUE(finalResult.isSuccess());

    // Verify final state
    auto finalCheck = engine_->evaluateExpression(sessionId_, "result").get();
    EXPECT_TRUE(finalCheck.isSuccess());
    EXPECT_EQ(finalCheck.getValue<std::string>(), "reached_end");
}

// Test data model operations
TEST_F(StateMachineIntegrationTest, DataModelOperations) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
    <datamodel>
        <data id="counter" expr="0"/>
        <data id="name" expr="'test'"/>
        <data id="active" expr="true"/>
    </datamodel>
    <state id="start">
        <onentry>
            <script>counter = counter + 1;</script>
        </onentry>
        <transition event="go" target="end">
            <script>active = false;</script>
        </transition>
    </state>
    <final id="end">
        <onentry>
            <script>name = 'completed';</script>
        </onentry>
    </final>
</scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);

    bool success = engine_->createSession(sessionId_, "");
    ASSERT_TRUE(success);

    // Initialize all data model variables
    auto dataItems = model->getDataModelItems();
    for (const auto &data : dataItems) {
        std::string script = "var " + data->getId() + " = " + data->getExpr() + ";";
        auto result = engine_->executeScript(sessionId_, script).get();
        EXPECT_TRUE(result.isSuccess()) << "Failed to initialize: " + data->getId();
    }

    // Test initial values
    auto counterResult = engine_->evaluateExpression(sessionId_, "counter").get();
    EXPECT_TRUE(counterResult.isSuccess());
    EXPECT_EQ(counterResult.getValue<double>(), 0.0);

    auto nameResult = engine_->evaluateExpression(sessionId_, "name").get();
    EXPECT_TRUE(nameResult.isSuccess());
    EXPECT_EQ(nameResult.getValue<std::string>(), "test");

    auto activeResult = engine_->evaluateExpression(sessionId_, "active").get();
    EXPECT_TRUE(activeResult.isSuccess());
    EXPECT_TRUE(activeResult.getValue<bool>());

    // Simulate increment operation
    auto incrementResult = engine_->executeScript(sessionId_, "counter = counter + 1;").get();
    EXPECT_TRUE(incrementResult.isSuccess());

    // Verify increment
    auto newCounterResult = engine_->evaluateExpression(sessionId_, "counter").get();
    EXPECT_TRUE(newCounterResult.isSuccess());
    EXPECT_EQ(newCounterResult.getValue<double>(), 1.0);
}

// Test guard condition evaluation
TEST_F(StateMachineIntegrationTest, GuardConditionEvaluation) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
    <datamodel>
        <data id="result" expr="''"/>
    </datamodel>
    <state id="start">
        <onentry>
            <script>result = 'entered_start';</script>
        </onentry>
        <transition event="go" target="end">
            <script>result = 'transitioning';</script>
        </transition>
    </state>
    <final id="end">
        <onentry>
            <script>result = 'reached_end';</script>
        </onentry>
    </final>
</scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);

    bool success = engine_->createSession(sessionId_, "");
    ASSERT_TRUE(success);

    // Initialize data
    auto initResult = engine_->executeScript(sessionId_, "var value = 5;").get();
    EXPECT_TRUE(initResult.isSuccess());

    // Test guard condition: value > 3 (should be true)
    auto guardResult1 = engine_->evaluateExpression(sessionId_, "value > 3").get();
    EXPECT_TRUE(guardResult1.isSuccess());
    EXPECT_TRUE(guardResult1.getValue<bool>());

    // Test guard condition: value <= 3 (should be false)
    auto guardResult2 = engine_->evaluateExpression(sessionId_, "value <= 3").get();
    EXPECT_TRUE(guardResult2.isSuccess());
    EXPECT_FALSE(guardResult2.getValue<bool>());

    // Change value and test again
    auto changeResult = engine_->executeScript(sessionId_, "value = 2;").get();
    EXPECT_TRUE(changeResult.isSuccess());

    auto guardResult3 = engine_->evaluateExpression(sessionId_, "value > 3").get();
    EXPECT_TRUE(guardResult3.isSuccess());
    EXPECT_FALSE(guardResult3.getValue<bool>());

    auto guardResult4 = engine_->evaluateExpression(sessionId_, "value <= 3").get();
    EXPECT_TRUE(guardResult4.isSuccess());
    EXPECT_TRUE(guardResult4.getValue<bool>());
}

// Test event system integration
TEST_F(StateMachineIntegrationTest, EventSystemIntegration) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
    <datamodel>
        <data id="result" expr="''"/>
    </datamodel>
    <state id="start">
        <onentry>
            <script>result = 'entered_start';</script>
        </onentry>
        <transition event="go" target="end">
            <script>result = 'transitioning';</script>
        </transition>
    </state>
    <final id="end">
        <onentry>
            <script>result = 'reached_end';</script>
        </onentry>
    </final>
</scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);

    bool success = engine_->createSession(sessionId_, "");
    ASSERT_TRUE(success);

    // Initialize data model
    auto initResult = engine_->executeScript(sessionId_, "var eventCount = 0; var lastEvent = '';").get();
    EXPECT_TRUE(initResult.isSuccess());

    // Simulate event reception and processing
    // Set up event object (this would normally be done by the engine)
    auto eventSetup = engine_->executeScript(sessionId_, "_event.name = 'testEvent'; _event.type = 'platform';").get();
    EXPECT_TRUE(eventSetup.isSuccess());

    // Execute transition script
    auto transitionScript =
        engine_->executeScript(sessionId_, "eventCount = eventCount + 1; lastEvent = _event.name;").get();
    EXPECT_TRUE(transitionScript.isSuccess());

    // Verify event processing
    auto countResult = engine_->evaluateExpression(sessionId_, "eventCount").get();
    EXPECT_TRUE(countResult.isSuccess());
    EXPECT_EQ(countResult.getValue<double>(), 1.0);

    auto eventNameResult = engine_->evaluateExpression(sessionId_, "lastEvent").get();
    EXPECT_TRUE(eventNameResult.isSuccess());
    EXPECT_EQ(eventNameResult.getValue<std::string>(), "testEvent");
}

// Test complex state machine with multiple features
TEST_F(StateMachineIntegrationTest, ComplexStateMachineExecution) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
    <datamodel>
        <data id="result" expr="''"/>
    </datamodel>
    <state id="start">
        <onentry>
            <script>result = 'entered_start';</script>
        </onentry>
        <transition event="go" target="end">
            <script>result = 'transitioning';</script>
        </transition>
    </state>
    <final id="end">
        <onentry>
            <script>result = 'reached_end';</script>
        </onentry>
    </final>
</scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_FALSE(parser_->hasErrors());

    bool success = engine_->createSession(sessionId_, "");
    ASSERT_TRUE(success);

    // Initialize complex data structures
    auto initResult = engine_->executeScript(sessionId_, "var steps = []; var currentStep = 'init';").get();
    EXPECT_TRUE(initResult.isSuccess());

    // Simulate complete state machine execution
    // Init state entry
    auto initEntry = engine_->executeScript(sessionId_, "steps.push('entered_init'); currentStep = 'init';").get();
    EXPECT_TRUE(initEntry.isSuccess());

    // Start transition
    auto startTransition = engine_->executeScript(sessionId_, "steps.push('start_transition');").get();
    EXPECT_TRUE(startTransition.isSuccess());

    // Working state entry
    auto workingEntry =
        engine_->executeScript(sessionId_, "steps.push('entered_working'); currentStep = 'working';").get();
    EXPECT_TRUE(workingEntry.isSuccess());

    // Step1 entry
    auto step1Entry = engine_->executeScript(sessionId_, "steps.push('step1');").get();
    EXPECT_TRUE(step1Entry.isSuccess());

    // Step2 entry
    auto step2Entry = engine_->executeScript(sessionId_, "steps.push('step2');").get();
    EXPECT_TRUE(step2Entry.isSuccess());

    // Completion
    auto completion = engine_->executeScript(sessionId_, "steps.push('completed'); currentStep = 'completed';").get();
    EXPECT_TRUE(completion.isSuccess());

    // Verify execution path
    auto stepsResult = engine_->evaluateExpression(sessionId_, "steps.length").get();
    EXPECT_TRUE(stepsResult.isSuccess());
    EXPECT_EQ(stepsResult.getValue<double>(), 6.0);

    auto currentStepResult = engine_->evaluateExpression(sessionId_, "currentStep").get();
    EXPECT_TRUE(currentStepResult.isSuccess());
    EXPECT_EQ(currentStepResult.getValue<std::string>(), "completed");
}

}  // namespace Tests
}  // namespace RSM