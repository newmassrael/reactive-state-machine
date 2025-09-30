#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

#include "common/Logger.h"
#include "runtime/EventRaiserImpl.h"
#include "runtime/StateMachine.h"
#include "runtime/StateMachineBuilder.h"
#include "runtime/StateMachineContext.h"
#include "scripting/JSEngine.h"

using namespace RSM;

class ActionIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // JSEngine 리셋으로 테스트 간 격리 보장
        RSM::JSEngine::instance().reset();

        // Build StateMachine with dependency injection, then wrap in RAII context
        auto eventRaiser = std::make_shared<EventRaiserImpl>();

        StateMachineBuilder builder;
        auto stateMachine = builder.withEventRaiser(eventRaiser).build();

        // Wrap in StateMachineContext for RAII cleanup
        smContext_ = std::make_unique<StateMachineContext>(std::move(stateMachine));
        stateMachine_ = smContext_->get();
    }

    void TearDown() override {
        // RAII cleanup: StateMachineContext destructor handles StateMachine stop and cleanup
        smContext_.reset();
        // Clean shutdown with minimal delay
        RSM::JSEngine::instance().shutdown();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void createTestSCXMLFile(const std::string &filename, const std::string &content) {
        std::ofstream file(filename);
        file << content;
        file.close();
    }

    void removeTestFile(const std::string &filename) {
        std::remove(filename.c_str());
    }

    std::unique_ptr<StateMachineContext> smContext_;
    StateMachine *stateMachine_;  // Non-owning pointer for easy access
};

TEST_F(ActionIntegrationTest, ScriptActionInOnEntryOnExit) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="init">
    <state id="init">
        <onentry>
            <script>
                var entryExecuted = true;
                var initCounter = 42;
            </script>
        </onentry>
        <onexit>
            <script>
                var exitExecuted = true;
                initCounter = initCounter + 10;
            </script>
        </onexit>
        <transition event="next" target="final"/>
    </state>

    <final id="final"/>
</scxml>)";

    std::string filename = "test_script_actions.scxml";
    createTestSCXMLFile(filename, scxmlContent);

    // Test SCXML loading and starting
    ASSERT_TRUE(stateMachine_->loadSCXML(filename));
    ASSERT_TRUE(stateMachine_->start());
    EXPECT_TRUE(stateMachine_->isRunning());
    EXPECT_EQ(stateMachine_->getCurrentState(), "init");

    // Give time for entry actions to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Test transition (should execute exit actions)
    auto result = stateMachine_->processEvent("next");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.fromState, "init");
    EXPECT_EQ(result.toState, "final");

    // Give time for exit actions to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    removeTestFile(filename);
}

TEST_F(ActionIntegrationTest, AssignActionInOnEntryOnExit) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="setup">
    <state id="setup">
        <onentry>
            <assign location="counter" expr="0"/>
            <assign location="status" expr="'initializing'"/>
        </onentry>
        <onexit>
            <assign location="counter" expr="counter + 1"/>
            <assign location="status" expr="'ready'"/>
        </onexit>
        <transition event="ready" target="active"/>
    </state>

    <state id="active">
        <onentry>
            <assign location="counter" expr="counter + 5"/>
            <assign location="status" expr="'active'"/>
        </onentry>
        <transition event="done" target="final"/>
    </state>

    <final id="final"/>
</scxml>)";

    std::string filename = "test_assign_actions.scxml";
    createTestSCXMLFile(filename, scxmlContent);

    ASSERT_TRUE(stateMachine_->loadSCXML(filename));
    ASSERT_TRUE(stateMachine_->start());
    EXPECT_EQ(stateMachine_->getCurrentState(), "setup");

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Test first transition
    auto result1 = stateMachine_->processEvent("ready");
    EXPECT_TRUE(result1.success);
    EXPECT_EQ(result1.toState, "active");

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Test final transition
    auto result2 = stateMachine_->processEvent("done");
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(result2.toState, "final");

    removeTestFile(filename);
}

TEST_F(ActionIntegrationTest, MixedScriptAndAssignActions) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="mixed">
    <state id="mixed">
        <onentry>
            <script>var step = 1;</script>
            <assign location="firstStep" expr="step"/>
            <script>step = step + 1;</script>
            <assign location="secondStep" expr="step"/>
            <script>var mixedComplete = true;</script>
        </onentry>
        <onexit>
            <assign location="exitStep" expr="step * 2"/>
            <script>var exitComplete = true;</script>
        </onexit>
        <transition event="finish" target="done"/>
    </state>

    <final id="done"/>
</scxml>)";

    std::string filename = "test_mixed_actions.scxml";
    createTestSCXMLFile(filename, scxmlContent);

    ASSERT_TRUE(stateMachine_->loadSCXML(filename));
    ASSERT_TRUE(stateMachine_->start());
    EXPECT_EQ(stateMachine_->getCurrentState(), "mixed");

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    auto result = stateMachine_->processEvent("finish");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.toState, "done");

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    removeTestFile(filename);
}

TEST_F(ActionIntegrationTest, ErrorHandlingWithInvalidActions) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="error_test">
    <state id="error_test">
        <onentry>
            <assign location="validVar" expr="123"/>
            <script>invalid JavaScript syntax here;</script>
            <assign location="anotherVar" expr="456"/>
        </onentry>
        <transition event="continue" target="recovery"/>
    </state>

    <state id="recovery">
        <onentry>
            <assign location="recovered" expr="true"/>
        </onentry>
        <transition event="done" target="final"/>
    </state>

    <final id="final"/>
</scxml>)";

    std::string filename = "test_error_handling.scxml";
    createTestSCXMLFile(filename, scxmlContent);

    // Should load and start successfully even with invalid actions
    ASSERT_TRUE(stateMachine_->loadSCXML(filename));
    ASSERT_TRUE(stateMachine_->start());
    EXPECT_TRUE(stateMachine_->isRunning());
    EXPECT_EQ(stateMachine_->getCurrentState(), "error_test");

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Should still be able to transition despite action errors
    auto result = stateMachine_->processEvent("continue");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.toState, "recovery");

    removeTestFile(filename);
}

TEST_F(ActionIntegrationTest, EmptyActionsHandling) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="empty">
    <state id="empty">
        <onentry>
        </onentry>
        <onexit>
        </onexit>
        <transition event="next" target="also_empty"/>
    </state>

    <state id="also_empty">
        <onentry>
            <assign location="emptyHandled" expr="true"/>
        </onentry>
        <transition event="done" target="final"/>
    </state>

    <final id="final"/>
</scxml>)";

    std::string filename = "test_empty_actions.scxml";
    createTestSCXMLFile(filename, scxmlContent);

    ASSERT_TRUE(stateMachine_->loadSCXML(filename));
    ASSERT_TRUE(stateMachine_->start());
    EXPECT_EQ(stateMachine_->getCurrentState(), "empty");

    auto result1 = stateMachine_->processEvent("next");
    EXPECT_TRUE(result1.success);
    EXPECT_EQ(result1.toState, "also_empty");

    auto result2 = stateMachine_->processEvent("done");
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(result2.toState, "final");

    removeTestFile(filename);
}

TEST_F(ActionIntegrationTest, CompoundStateWithActions) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="parent">
    <state id="parent" initial="child1">
        <onentry>
            <assign location="parentEntered" expr="true"/>
        </onentry>
        <onexit>
            <assign location="parentExited" expr="true"/>
        </onexit>

        <state id="child1">
            <onentry>
                <script>var childActive = 1;</script>
            </onentry>
            <onexit>
                <script>childActive = 0;</script>
            </onexit>
            <transition event="switch" target="child2"/>
        </state>

        <state id="child2">
            <onentry>
                <assign location="secondChild" expr="true"/>
            </onentry>
            <transition event="exit" target="final"/>
        </state>

        <transition event="emergency" target="final"/>
    </state>

    <final id="final"/>
</scxml>)";

    std::string filename = "test_compound_actions.scxml";
    createTestSCXMLFile(filename, scxmlContent);

    ASSERT_TRUE(stateMachine_->loadSCXML(filename));
    ASSERT_TRUE(stateMachine_->start());

    // Should start in child1 state
    EXPECT_EQ(stateMachine_->getCurrentState(), "child1");

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Test internal transition
    auto result1 = stateMachine_->processEvent("switch");
    EXPECT_TRUE(result1.success);
    EXPECT_EQ(result1.toState, "child2");

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Test exit from compound state
    auto result2 = stateMachine_->processEvent("exit");
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(result2.toState, "final");

    removeTestFile(filename);
}

TEST_F(ActionIntegrationTest, BackwardCompatibilityWithLegacyActions) {
    // This test verifies that both old string-based actions and new IActionNode actions work together
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="compatibility">
    <state id="compatibility">
        <onentry>
            <assign location="newSystem" expr="true"/>
            <script>var legacyVar = 'legacy_and_new_working';</script>
        </onentry>
        <onexit>
            <script>var exitMessage = 'Both systems executed';</script>
            <assign location="exitFlag" expr="true"/>
        </onexit>
        <transition event="test" target="final"/>
    </state>

    <final id="final"/>
</scxml>)";

    std::string filename = "test_compatibility.scxml";
    createTestSCXMLFile(filename, scxmlContent);

    ASSERT_TRUE(stateMachine_->loadSCXML(filename));
    ASSERT_TRUE(stateMachine_->start());
    EXPECT_EQ(stateMachine_->getCurrentState(), "compatibility");

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    auto result = stateMachine_->processEvent("test");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.toState, "final");

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    removeTestFile(filename);
}

TEST_F(ActionIntegrationTest, OnEntryForeachExecution) {
    // Test to verify that onentry actions (specifically foreach) are properly executed
    // This addresses the issue found in W3C test 150 where onentry actions were not running
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s0" datamodel="ecmascript">
    <datamodel>
        <data id="testArray">[1,2,3]</data>
    </datamodel>

    <state id="s0">
        <onentry>
            <!-- This foreach should create newVar even though it doesn't exist -->
            <foreach item="newVar" index="newIndex" array="testArray"/>
            <raise event="continue"/>
        </onentry>
        <transition event="error" target="fail"/>
        <transition event="continue" target="s1"/>
    </state>

    <state id="s1">
        <onentry>
            <!-- Set a flag to indicate we reached this state -->
            <script>reachedS1 = true;</script>
        </onentry>
        <!-- Check if newVar was created by foreach -->
        <transition cond="typeof newVar !== 'undefined'" target="pass"/>
        <transition target="fail"/>
    </state>

    <final id="pass"/>
    <final id="fail"/>
</scxml>)";

    std::string filename = "test_onentry_foreach.scxml";
    createTestSCXMLFile(filename, scxmlContent);

    ASSERT_TRUE(stateMachine_->loadSCXML(filename));
    ASSERT_TRUE(stateMachine_->start());

    // According to SCXML W3C specification, start() should complete the entire macrostep
    // including onentry actions and automatic transitions, ending in a stable configuration
    std::string currentState = stateMachine_->getCurrentState();

    // The test passes if we reach 'pass' state, indicating:
    // 1. onentry foreach action executed successfully
    // 2. newVar was created by foreach
    // 3. Automatic transitions worked correctly per SCXML specification
    EXPECT_EQ(currentState, "pass")
        << "OnEntry foreach action should create newVar and reach pass state per SCXML W3C specification";

    removeTestFile(filename);
}

TEST_F(ActionIntegrationTest, OnEntryActionExecutionOrder) {
    // Test to verify that onentry actions are executed in document order
    // W3C requirement: "execute the <onentry> handlers of a state in document order"
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="orderTest" datamodel="ecmascript">
    <datamodel>
        <data id="executionOrder">""</data>
    </datamodel>

    <state id="orderTest">
        <onentry>
            <!-- These should execute in document order: 1, 2, 3 -->
            <script>executionOrder += "1";</script>
            <assign location="tempVar" expr="'step2'"/>
            <script>executionOrder += "2";</script>
            <script>executionOrder += "3";</script>
            <raise event="checkOrder"/>
        </onentry>
        <transition event="checkOrder" target="validate"/>
    </state>

    <state id="validate">
        <!-- Check if execution order was 1-2-3 -->
        <transition cond="executionOrder === '123'" target="pass"/>
        <transition target="fail"/>
    </state>

    <final id="pass"/>
    <final id="fail"/>
</scxml>)";

    std::string filename = "test_onentry_order.scxml";
    createTestSCXMLFile(filename, scxmlContent);

    ASSERT_TRUE(stateMachine_->loadSCXML(filename));
    ASSERT_TRUE(stateMachine_->start());

    // Give time for state transitions and action execution
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    std::string currentState = stateMachine_->getCurrentState();

    if (currentState == "pass") {
        SUCCEED() << "OnEntry actions executed in correct document order (1-2-3)";
    } else if (currentState == "fail") {
        FAIL() << "OnEntry actions did not execute in document order";
    } else if (currentState == "orderTest") {
        FAIL() << "OnEntry actions were not executed at all";
    } else {
        FAIL() << "Unexpected state: " << currentState;
    }

    removeTestFile(filename);
}

TEST_F(ActionIntegrationTest, ForeachErrorHandling) {
    // Test W3C requirement: foreach with invalid array should generate error.execution
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="errorTest" datamodel="ecmascript">
    <datamodel>
        <data id="invalidArray">null</data>
        <data id="validArray">[1,2,3]</data>
    </datamodel>

    <state id="errorTest">
        <onentry>
            <!-- This foreach should generate error.execution due to null array -->
            <foreach item="testItem" array="invalidArray"/>
            <!-- This should not execute if error occurred -->
            <script>shouldNotExecute = true;</script>
        </onentry>
        <transition event="error.execution" target="errorHandled"/>
        <transition event="*" target="fail"/>
    </state>

    <state id="errorHandled">
        <onentry>
            <!-- Test with valid array after error -->
            <foreach item="validItem" array="validArray"/>
        </onentry>
        <!-- Check if valid foreach worked after error handling -->
        <transition cond="typeof validItem !== 'undefined'" target="pass"/>
        <transition target="fail"/>
    </state>

    <final id="pass"/>
    <final id="fail"/>
</scxml>)";

    std::string filename = "test_foreach_error.scxml";
    createTestSCXMLFile(filename, scxmlContent);

    ASSERT_TRUE(stateMachine_->loadSCXML(filename));
    ASSERT_TRUE(stateMachine_->start());

    // Give time for error handling and state transitions
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    std::string currentState = stateMachine_->getCurrentState();

    if (currentState == "pass") {
        SUCCEED() << "Foreach error handling works correctly - error.execution generated and valid foreach executed";
    } else if (currentState == "fail") {
        FAIL() << "Foreach error handling failed";
    } else if (currentState == "errorTest") {
        FAIL() << "OnEntry actions were not executed or error.execution not generated";
    } else if (currentState == "errorHandled") {
        FAIL() << "Error was handled but valid foreach did not create variable";
    } else {
        FAIL() << "Unexpected state: " << currentState;
    }

    removeTestFile(filename);
}

TEST_F(ActionIntegrationTest, IfElseIfElseExecution) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s0" datamodel="ecmascript">
    <datamodel>
        <data id="counter" expr="0"/>
        <data id="result" expr="''"/>
    </datamodel>

    <state id="s0">
        <onentry>
            <if cond="false">
                <assign location="result" expr="'if_branch'"/>
                <assign location="counter" expr="counter + 10"/>
            <elseif cond="true"/>
                <assign location="result" expr="'elseif_branch'"/>
                <assign location="counter" expr="counter + 1"/>
            <else/>
                <assign location="result" expr="'else_branch'"/>
                <assign location="counter" expr="counter + 100"/>
            </if>
            <raise event="continue"/>
        </onentry>
        <transition event="continue" cond="counter == 1 &amp;&amp; result == 'elseif_branch'" target="pass"/>
        <transition event="continue" target="fail"/>
    </state>

    <final id="pass"/>
    <final id="fail"/>
</scxml>)";

    std::string filename = "test_if_elseif_else.scxml";
    createTestSCXMLFile(filename, scxmlContent);

    ASSERT_TRUE(stateMachine_->loadSCXML(filename));
    ASSERT_TRUE(stateMachine_->start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string currentState = stateMachine_->getCurrentState();

    if (currentState == "pass") {
        SUCCEED() << "If-ElseIf-Else executed correctly - elseif branch taken, counter=1, result='elseif_branch'";
    } else if (currentState == "fail") {
        FAIL() << "If-ElseIf-Else failed - wrong branch executed or incorrect variable values";
    } else {
        FAIL() << "Unexpected state: " << currentState;
    }

    removeTestFile(filename);
}

TEST_F(ActionIntegrationTest, IfElseIfElseElseBranchExecution) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s0" datamodel="ecmascript">
    <datamodel>
        <data id="counter" expr="0"/>
        <data id="result" expr="''"/>
    </datamodel>

    <state id="s0">
        <onentry>
            <if cond="false">
                <assign location="result" expr="'if_branch'"/>
                <assign location="counter" expr="counter + 10"/>
            <elseif cond="false"/>
                <assign location="result" expr="'elseif_branch'"/>
                <assign location="counter" expr="counter + 1"/>
            <else/>
                <assign location="result" expr="'else_branch'"/>
                <assign location="counter" expr="counter + 100"/>
            </if>
            <raise event="continue"/>
        </onentry>
        <transition event="continue" cond="counter == 100 &amp;&amp; result == 'else_branch'" target="pass"/>
        <transition event="continue" target="fail"/>
    </state>

    <final id="pass"/>
    <final id="fail"/>
</scxml>)";

    std::string filename = "test_if_elseif_else_branch.scxml";
    createTestSCXMLFile(filename, scxmlContent);

    ASSERT_TRUE(stateMachine_->loadSCXML(filename));
    ASSERT_TRUE(stateMachine_->start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string currentState = stateMachine_->getCurrentState();

    if (currentState == "pass") {
        SUCCEED() << "If-ElseIf-Else executed correctly - else branch taken, counter=100, result='else_branch'";
    } else if (currentState == "fail") {
        FAIL() << "If-ElseIf-Else failed - wrong branch executed or incorrect variable values";
    } else {
        FAIL() << "Unexpected state: " << currentState;
    }

    removeTestFile(filename);
}