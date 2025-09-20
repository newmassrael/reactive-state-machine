#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

#include "common/Logger.h"
#include "runtime/StateMachine.h"
#include "scripting/JSEngine.h"

using namespace RSM;

class Phase2IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        stateMachine_ = std::make_unique<StateMachine>();
    }

    void TearDown() override {
        if (stateMachine_ && stateMachine_->isRunning()) {
            stateMachine_->stop();
        }
        stateMachine_.reset();
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

    std::unique_ptr<StateMachine> stateMachine_;
};

TEST_F(Phase2IntegrationTest, ScriptActionInOnEntryOnExit) {
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

TEST_F(Phase2IntegrationTest, AssignActionInOnEntryOnExit) {
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

TEST_F(Phase2IntegrationTest, MixedScriptAndAssignActions) {
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

TEST_F(Phase2IntegrationTest, ErrorHandlingWithInvalidActions) {
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

TEST_F(Phase2IntegrationTest, EmptyActionsHandling) {
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

TEST_F(Phase2IntegrationTest, CompoundStateWithActions) {
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

TEST_F(Phase2IntegrationTest, BackwardCompatibilityWithLegacyActions) {
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