// Integration test for static code generation
// Tests the complete workflow: SCXML -> Generated C++ -> Compilation -> Execution

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "Thermostat_sm.h"

using namespace RSM::Generated;

// User implementation using CRTP pattern
class ThermostatLogic : public ThermostatBase<ThermostatLogic> {
public:
    // Track action calls for testing
    std::vector<std::string> actionLog;
    bool coolDecision = true;

    // Guard method
    bool shouldCool() {
        actionLog.push_back("shouldCool()");
        return coolDecision;
    }

    // Action methods
    void onEnterIdle() {
        actionLog.push_back("onEnterIdle()");
    }

    void onEnterCooling() {
        actionLog.push_back("onEnterCooling()");
    }

    void onExitCooling() {
        actionLog.push_back("onExitCooling()");
    }

    void startCooling() {
        actionLog.push_back("startCooling()");
    }

    void stopCooling() {
        actionLog.push_back("stopCooling()");
    }

    // Friend declaration for CRTP access
    friend class ThermostatBase<ThermostatLogic>;
};

class StaticCodegenIntegrationTest : public ::testing::Test {
protected:
    ThermostatLogic thermostat;

    void SetUp() override {
        thermostat.actionLog.clear();
    }
};

TEST_F(StaticCodegenIntegrationTest, InitializeCallsInitialStateOnentry) {
    // Act
    thermostat.initialize();

    // Assert
    ASSERT_EQ(thermostat.actionLog.size(), 1);
    EXPECT_EQ(thermostat.actionLog[0], "onEnterIdle()");
    EXPECT_EQ(thermostat.getCurrentState(), State::Idle);
}

TEST_F(StaticCodegenIntegrationTest, TransitionWithGuardTrue) {
    // Arrange
    thermostat.initialize();
    thermostat.actionLog.clear();
    thermostat.coolDecision = true;

    // Act: Trigger transition with guard
    thermostat.processEvent(Event::Temp_high);

    // Assert: Guard checked, transition actions executed, state changed
    ASSERT_EQ(thermostat.actionLog.size(), 3);
    EXPECT_EQ(thermostat.actionLog[0], "shouldCool()");      // Guard check
    EXPECT_EQ(thermostat.actionLog[1], "startCooling()");    // Transition action
    EXPECT_EQ(thermostat.actionLog[2], "onEnterCooling()");  // Target state onentry
    EXPECT_EQ(thermostat.getCurrentState(), State::Cooling);
}

TEST_F(StaticCodegenIntegrationTest, TransitionWithGuardFalse) {
    // Arrange
    thermostat.initialize();
    thermostat.actionLog.clear();
    thermostat.coolDecision = false;

    // Act: Trigger transition with guard that fails
    thermostat.processEvent(Event::Temp_high);

    // Assert: Guard checked, but no transition
    ASSERT_EQ(thermostat.actionLog.size(), 1);
    EXPECT_EQ(thermostat.actionLog[0], "shouldCool()");    // Guard check only
    EXPECT_EQ(thermostat.getCurrentState(), State::Idle);  // Still in idle
}

TEST_F(StaticCodegenIntegrationTest, TransitionWithExitAndEntryActions) {
    // Arrange: Get to cooling state
    thermostat.initialize();
    thermostat.coolDecision = true;
    thermostat.processEvent(Event::Temp_high);
    thermostat.actionLog.clear();

    // Act: Trigger transition back to idle
    thermostat.processEvent(Event::Temp_normal);

    // Assert: Correct action execution order
    ASSERT_EQ(thermostat.actionLog.size(), 3);
    EXPECT_EQ(thermostat.actionLog[0], "onExitCooling()");  // Source state onexit
    EXPECT_EQ(thermostat.actionLog[1], "stopCooling()");    // Transition action
    EXPECT_EQ(thermostat.actionLog[2], "onEnterIdle()");    // Target state onentry
    EXPECT_EQ(thermostat.getCurrentState(), State::Idle);
}

TEST_F(StaticCodegenIntegrationTest, CompleteStateMachineScenario) {
    // Scenario: Idle -> Cooling -> Idle cycle

    // Step 1: Initialize
    thermostat.initialize();
    EXPECT_EQ(thermostat.getCurrentState(), State::Idle);
    thermostat.actionLog.clear();

    // Step 2: Temperature goes high, should start cooling
    thermostat.coolDecision = true;
    thermostat.processEvent(Event::Temp_high);
    EXPECT_EQ(thermostat.getCurrentState(), State::Cooling);
    EXPECT_EQ(thermostat.actionLog[0], "shouldCool()");
    EXPECT_EQ(thermostat.actionLog[1], "startCooling()");
    EXPECT_EQ(thermostat.actionLog[2], "onEnterCooling()");
    thermostat.actionLog.clear();

    // Step 3: Temperature normalizes, should stop cooling
    thermostat.processEvent(Event::Temp_normal);
    EXPECT_EQ(thermostat.getCurrentState(), State::Idle);
    EXPECT_EQ(thermostat.actionLog[0], "onExitCooling()");
    EXPECT_EQ(thermostat.actionLog[1], "stopCooling()");
    EXPECT_EQ(thermostat.actionLog[2], "onEnterIdle()");
}

TEST_F(StaticCodegenIntegrationTest, IgnoresIrrelevantEvents) {
    // Arrange
    thermostat.initialize();
    thermostat.actionLog.clear();

    // Act: Send event that has no transition from idle
    thermostat.processEvent(Event::Temp_normal);

    // Assert: No action taken, still in idle
    EXPECT_EQ(thermostat.actionLog.size(), 0);
    EXPECT_EQ(thermostat.getCurrentState(), State::Idle);
}

// Test that demonstrates zero-overhead: no virtual functions, all inline
TEST_F(StaticCodegenIntegrationTest, VerifyCRTPPatternCompiles) {
    // This test exists to verify that:
    // 1. CRTP pattern compiles without errors
    // 2. No virtual function overhead (can be verified with sizeof)
    // 3. Friend declaration works correctly

    thermostat.initialize();
    EXPECT_TRUE(true);  // If we got here, CRTP pattern works

    // Verify base class has no vtable (size check)
    // Size should be just the state variable (1 byte for enum)
    // Plus padding, typically 4 bytes total
    EXPECT_LE(sizeof(ThermostatBase<ThermostatLogic>), 8);
}
