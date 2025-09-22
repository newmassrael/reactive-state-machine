#include "runtime/StateMachine.h"
#include "common/Logger.h"
#include "runtime/StateMachineFactory.h"
#include <fstream>
#include <gtest/gtest.h>

using namespace RSM;

class StateMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
        // Logger::setLevel(Logger::Level::DEBUG); // Logger API 확인 필요
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper to create a simple SCXML document
    std::string createSimpleSCXML() {
        return R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="idle">
    <state id="idle">
        <transition event="start" target="running"/>
    </state>
    
    <state id="running">
        <transition event="stop" target="idle"/>
        <transition event="finish" target="done"/>
    </state>
    
    <final id="done"/>
</scxml>)";
    }

    // Helper to create SCXML with JavaScript guards/actions
    std::string createSCXMLWithJS() {
        return R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="waiting">
    <datamodel>
        <data id="counter" expr="0"/>
    </datamodel>
    
    <state id="waiting">
        <onentry>
            <script>counter = 0;</script>
        </onentry>
        <transition event="increment" cond="counter &lt; 5" target="counting">
            <script>counter = counter + 1;</script>
        </transition>
        <transition event="increment" cond="counter >= 5" target="finished"/>
    </state>
    
    <state id="counting">
        <transition event="increment" cond="counter &lt; 5" target="counting">
            <script>counter = counter + 1;</script>
        </transition>
        <transition event="increment" cond="counter >= 5" target="finished"/>
        <transition event="reset" target="waiting"/>
    </state>
    
    <final id="finished"/>
</scxml>)";
    }

    // Helper to create SCXML for C++ binding test
    std::string createSCXMLWithCppBinding() {
        return R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="init">
    <state id="init">
        <transition event="check_temp" target="normal"/>
    </state>
    
    <state id="normal">
        <onentry>
            <script>console.log("Entered normal state");</script>
        </onentry>
        <transition event="overheat" target="cooling"/>
    </state>
    
    <state id="cooling">
        <onentry>
            <script>console.log("Entered cooling state");</script>
        </onentry>
        <transition event="cooled" target="normal"/>
    </state>
</scxml>)";
    }
};

// Mock hardware class for C++ binding tests
class MockHardware {
public:
    bool isTemperatureHigh() const {
        return temperature_ > 30.0;
    }

    void setTemperature(double temp) {
        temperature_ = temp;
    }

    void startCooling() {
        cooling_ = true;
        status_ = "Cooling active";
    }

    void setStatus(const std::string &status) {
        status_ = status;
    }

    double getTemperature() const {
        return temperature_;
    }

    bool isCooling() const {
        return cooling_;
    }

    std::string getStatus() const {
        return status_;
    }

private:
    double temperature_ = 25.0;
    bool cooling_ = false;
    std::string status_ = "Unknown";
};

// Basic functionality tests
TEST_F(StateMachineTest, Constructor) {
    // 기본 생성자는 안전하게 성공
    StateMachine sm;
    EXPECT_FALSE(sm.isRunning());
    // SCXML이 로드되지 않은 상태에서는 getCurrentState() 호출하면 안됨
    // 이는 SCXML 표준 준수를 위한 올바른 동작
}

TEST_F(StateMachineTest, FactoryPattern_CreateForTesting) {
    // Factory 패턴으로 Mock 기반 테스트 가능
    auto result = StateMachineFactory::createForTesting();

    // Factory는 Mock 엔진 사용하므로 성공할 수 있음
    if (result.has_value()) {
        EXPECT_FALSE(result.value->isRunning());
    } else {
        // 실패해도 오류 메시지가 있어야 함
        EXPECT_FALSE(result.error.empty());
    }
}

TEST_F(StateMachineTest, LoadSimpleSCXML) {
    StateMachine sm;
    std::string scxml = createSimpleSCXML();

    EXPECT_TRUE(sm.loadSCXMLFromString(scxml));
}

TEST_F(StateMachineTest, StartStateMachine) {
    StateMachine sm;
    std::string scxml = createSimpleSCXML();

    ASSERT_TRUE(sm.loadSCXMLFromString(scxml));
    EXPECT_TRUE(sm.start());
    EXPECT_TRUE(sm.isRunning());
    EXPECT_EQ(sm.getCurrentState(), "idle");
    EXPECT_TRUE(sm.isStateActive("idle"));
}

TEST_F(StateMachineTest, BasicTransition) {
    StateMachine sm;
    std::string scxml = createSimpleSCXML();

    ASSERT_TRUE(sm.loadSCXMLFromString(scxml));
    ASSERT_TRUE(sm.start());

    // Test transition from idle to running
    auto result = sm.processEvent("start");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.fromState, "idle");
    EXPECT_EQ(result.toState, "running");
    EXPECT_EQ(result.eventName, "start");
    EXPECT_EQ(sm.getCurrentState(), "running");
}

TEST_F(StateMachineTest, InvalidEvent) {
    StateMachine sm;
    std::string scxml = createSimpleSCXML();

    ASSERT_TRUE(sm.loadSCXMLFromString(scxml));
    ASSERT_TRUE(sm.start());

    // Test invalid event
    auto result = sm.processEvent("invalid_event");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
    EXPECT_EQ(sm.getCurrentState(), "idle");  // Should stay in same state
}

TEST_F(StateMachineTest, MultipleTransitions) {
    StateMachine sm;
    std::string scxml = createSimpleSCXML();

    ASSERT_TRUE(sm.loadSCXMLFromString(scxml));
    ASSERT_TRUE(sm.start());

    // idle -> running
    auto result1 = sm.processEvent("start");
    EXPECT_TRUE(result1.success);
    EXPECT_EQ(sm.getCurrentState(), "running");

    // running -> idle
    auto result2 = sm.processEvent("stop");
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(sm.getCurrentState(), "idle");

    // idle -> running -> done
    sm.processEvent("start");
    auto result3 = sm.processEvent("finish");
    EXPECT_TRUE(result3.success);
    EXPECT_EQ(sm.getCurrentState(), "done");
}

TEST_F(StateMachineTest, StopStateMachine) {
    StateMachine sm;
    std::string scxml = createSimpleSCXML();

    ASSERT_TRUE(sm.loadSCXMLFromString(scxml));
    ASSERT_TRUE(sm.start());

    sm.stop();
    EXPECT_FALSE(sm.isRunning());
    EXPECT_TRUE(sm.getCurrentState().empty());
    EXPECT_TRUE(sm.getActiveStates().empty());
}

TEST_F(StateMachineTest, Statistics) {
    StateMachine sm;
    std::string scxml = createSimpleSCXML();

    ASSERT_TRUE(sm.loadSCXMLFromString(scxml));
    ASSERT_TRUE(sm.start());

    auto stats1 = sm.getStatistics();
    EXPECT_EQ(stats1.totalTransitions, 0);
    EXPECT_EQ(stats1.totalEvents, 0);
    EXPECT_TRUE(stats1.isRunning);

    // Make some transitions
    sm.processEvent("start");
    sm.processEvent("stop");
    sm.processEvent("invalid");  // This should fail

    auto stats2 = sm.getStatistics();
    EXPECT_EQ(stats2.totalTransitions, 2);
    EXPECT_EQ(stats2.failedTransitions, 1);
    EXPECT_EQ(stats2.totalEvents, 3);
}

// JavaScript integration tests
TEST_F(StateMachineTest, JavaScriptGuards) {
    StateMachine sm;
    std::string scxml = createSCXMLWithJS();

    ASSERT_TRUE(sm.loadSCXMLFromString(scxml));
    ASSERT_TRUE(sm.start());
    EXPECT_EQ(sm.getCurrentState(), "waiting");

    // First few increments should go to counting
    for (int i = 0; i < 5; i++) {
        auto result = sm.processEvent("increment");
        EXPECT_TRUE(result.success);
        // Should be in counting state after first increment
        if (i == 0) {
            EXPECT_EQ(sm.getCurrentState(), "counting");
        }
    }

    // 6th increment should go to finished (counter will be 5, so counter >= 5 condition triggers)
    auto result = sm.processEvent("increment");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(sm.getCurrentState(), "finished");
}

// C++ binding tests (these will need the binding implementation)
TEST_F(StateMachineTest, DISABLED_CppObjectBinding) {
    StateMachine sm;
    MockHardware hardware;

    // This test is disabled until we implement the C++ binding
    std::string scxml = createSCXMLWithCppBinding();

    ASSERT_TRUE(sm.loadSCXMLFromString(scxml));

    // Bind C++ object
    sm.bindObject("hardware", &hardware);

    ASSERT_TRUE(sm.start());
    EXPECT_EQ(sm.getCurrentState(), "init");

    // Test with low temperature
    hardware.setTemperature(25.0);
    auto result1 = sm.processEvent("check_temp");
    EXPECT_TRUE(result1.success);
    EXPECT_EQ(sm.getCurrentState(), "normal");
    EXPECT_EQ(hardware.getStatus(), "Normal operation");

    // Test with high temperature
    hardware.setTemperature(35.0);
    auto result2 = sm.processEvent("check_temp");
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(sm.getCurrentState(), "cooling");
    EXPECT_TRUE(hardware.isCooling());
}

// Integration with existing JSEngine tests
TEST_F(StateMachineTest, JSEngineIntegration) {
    StateMachine sm;

    // Create simple SCXML with script
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
    <state id="start">
        <onentry>
            <script>var testVar = "Hello from SCXML";</script>
        </onentry>
        <transition event="next" target="end"/>
    </state>
    <final id="end"/>
</scxml>)";

    ASSERT_TRUE(sm.loadSCXMLFromString(scxml));
    ASSERT_TRUE(sm.start());

    // The script should have executed in onentry
    // (We can't easily test this without exposing the JS session,
    //  but it tests that the integration doesn't crash)

    auto result = sm.processEvent("next");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(sm.getCurrentState(), "end");
}

// Error handling tests
TEST_F(StateMachineTest, InvalidSCXML) {
    StateMachine sm;

    std::string invalidScxml = "<?xml version='1.0'?><invalid>not scxml</invalid>";
    EXPECT_FALSE(sm.loadSCXMLFromString(invalidScxml));
}

TEST_F(StateMachineTest, EmptySCXML) {
    StateMachine sm;

    EXPECT_FALSE(sm.loadSCXMLFromString(""));
}

TEST_F(StateMachineTest, StartWithoutLoading) {
    StateMachine sm;

    EXPECT_FALSE(sm.start());
    EXPECT_FALSE(sm.isRunning());
}

TEST_F(StateMachineTest, ProcessEventWithoutStarting) {
    StateMachine sm;
    std::string scxml = createSimpleSCXML();

    ASSERT_TRUE(sm.loadSCXMLFromString(scxml));
    // Don't start the state machine

    auto result = sm.processEvent("start");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}