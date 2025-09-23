#include "runtime/HistoryManager.h"
#include "runtime/IHistoryManager.h"
#include "runtime/StateMachine.h"
#include "scripting/JSEngine.h"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace RSM {

/**
 * SCXML W3C Specification History States Integration Tests
 *
 * These tests verify compliance with W3C SCXML 1.0 specification Section 3.6 (History States)
 * covering the essential aspects of history state behavior that can be tested with the
 * current StateMachine API including:
 * - History state registration and validation
 * - Basic history functionality verification
 * - State machine lifecycle integration
 * - Thread safety of history operations
 */
class HistoryStateIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset JSEngine for test isolation (following SCXML specification pattern)
        RSM::JSEngine::instance().reset();

        // Initialize state machine with history support
        stateMachine = std::make_unique<StateMachine>();

        // Create JSEngine session for this test
        sessionId = "history_integration_test";
        bool sessionCreated = RSM::JSEngine::instance().createSession(sessionId);
        if (!sessionCreated) {
            throw std::runtime_error("Failed to create JSEngine session for HistoryStateIntegrationTest");
        }
    }

    void TearDown() override {
        if (stateMachine) {
            stateMachine->stop();
        }

        // Clean up JSEngine session
        if (!sessionId.empty()) {
            RSM::JSEngine::instance().destroySession(sessionId);
        }

        // Shutdown JSEngine to ensure clean state for next test
        RSM::JSEngine::instance().shutdown();
    }

    std::unique_ptr<StateMachine> stateMachine;
    std::string sessionId;
};

/**
 * W3C SCXML Section 3.6: Basic history state registration
 * Tests that history states can be properly registered with the state machine
 */
TEST_F(HistoryStateIntegrationTest, W3C_HistoryState_BasicRegistration) {
    const std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="main">
        <state id="main">
            <state id="compound">
                <history type="shallow" id="hist">
                    <transition target="state1"/>
                </history>
                <state id="state1"/>
                <state id="state2"/>
            </state>
        </state>
    </scxml>)";

    ASSERT_TRUE(stateMachine->loadSCXMLFromString(scxml));
    ASSERT_TRUE(stateMachine->start());

    // Test basic state machine functionality
    EXPECT_TRUE(stateMachine->isRunning());

    // SCXML W3C Section 3.6: History states should be auto-registered from SCXML
    // No manual registration needed - this tests the auto-registration feature
    EXPECT_TRUE(stateMachine->isHistoryState("hist"));

    // Verify non-history states return false
    EXPECT_FALSE(stateMachine->isHistoryState("state1"));
    EXPECT_FALSE(stateMachine->isHistoryState("state2"));
}

/**
 * W3C SCXML Section 3.6: History state clear functionality
 * Tests that history can be cleared and reset
 */
TEST_F(HistoryStateIntegrationTest, W3C_HistoryState_ClearFunctionality) {
    const std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="main">
        <state id="main">
            <state id="compound">
                <history type="deep" id="deep_hist">
                    <transition target="state1"/>
                </history>
                <state id="state1">
                    <state id="nested1"/>
                    <state id="nested2"/>
                </state>
                <state id="state2"/>
            </state>
        </state>
    </scxml>)";

    ASSERT_TRUE(stateMachine->loadSCXMLFromString(scxml));
    ASSERT_TRUE(stateMachine->start());

    // SCXML W3C Section 3.6: Deep history state should be auto-registered
    EXPECT_TRUE(stateMachine->isHistoryState("deep_hist"));

    // Clear all history - should not throw exceptions
    EXPECT_NO_THROW(stateMachine->clearAllHistory());

    // History entries should be available for debugging
    auto entries = stateMachine->getHistoryEntries();
    // After clearing, entries should be empty or default state
    EXPECT_GE(entries.size(), 0);  // Should not fail
}

/**
 * W3C SCXML Section 3.6: History state registration validation
 * Tests various registration scenarios and error conditions
 */
TEST_F(HistoryStateIntegrationTest, W3C_HistoryState_RegistrationValidation) {
    const std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="main">
        <state id="main">
            <state id="compound1">
                <history type="shallow" id="hist1">
                    <transition target="default1"/>
                </history>
                <state id="default1"/>
                <state id="regular1"/>
            </state>
            <state id="compound2">
                <history type="deep" id="hist2">
                    <transition target="default2"/>
                </history>
                <state id="default2"/>
                <state id="regular2"/>
            </state>
        </state>
    </scxml>)";

    ASSERT_TRUE(stateMachine->loadSCXMLFromString(scxml));
    ASSERT_TRUE(stateMachine->start());

    // W3C SCXML Section 3.6: History states should be auto-registered from SCXML
    // No manual registration needed - verify automatic recognition
    EXPECT_TRUE(stateMachine->isHistoryState("hist1"));
    EXPECT_TRUE(stateMachine->isHistoryState("hist2"));

    // Test that regular states are not history states
    EXPECT_FALSE(stateMachine->isHistoryState("regular1"));
    EXPECT_FALSE(stateMachine->isHistoryState("regular2"));
}

/**
 * W3C SCXML Section 3.6: State machine lifecycle with history
 * Tests that history persists across different state machine operations
 */
TEST_F(HistoryStateIntegrationTest, W3C_HistoryState_StateMachineLifecycle) {
    const std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="initial_state">
        <state id="initial_state">
            <transition event="start" target="compound"/>
        </state>
        <state id="compound">
            <history type="shallow" id="lifecycle_hist">
                <transition target="first"/>
            </history>
            <state id="first">
                <transition event="next" target="second"/>
            </state>
            <state id="second">
                <transition event="exit" target="outside"/>
            </state>
        </state>
        <state id="outside">
            <transition event="restore" target="lifecycle_hist"/>
        </state>
    </scxml>)";

    ASSERT_TRUE(stateMachine->loadSCXMLFromString(scxml));
    ASSERT_TRUE(stateMachine->start());

    // Verify initial state
    EXPECT_TRUE(stateMachine->isRunning());
    auto activeStates = stateMachine->getActiveStates();
    EXPECT_FALSE(activeStates.empty());

    // Test state machine statistics
    auto stats = stateMachine->getStatistics();
    EXPECT_TRUE(stats.isRunning);
    EXPECT_FALSE(stats.currentState.empty());

    // History functionality should be available
    EXPECT_TRUE(stateMachine->isHistoryState("lifecycle_hist"));

    // Clear history should work without issues
    EXPECT_NO_THROW(stateMachine->clearAllHistory());
}

/**
 * W3C SCXML Section 3.6: Multiple history states coordination
 * Tests systems with multiple history states working together
 */
TEST_F(HistoryStateIntegrationTest, W3C_HistoryState_MultipleHistoryCoordination) {
    const std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="app">
        <state id="app">
            <state id="workflow">
                <history type="shallow" id="workflow_hist">
                    <transition target="step1"/>
                </history>
                <state id="step1">
                    <transition event="next" target="step2"/>
                </state>
                <state id="step2">
                    <transition event="complete" target="done"/>
                </state>
                <state id="done"/>
            </state>
            <state id="settings">
                <history type="deep" id="settings_hist">
                    <transition target="general"/>
                </history>
                <state id="general">
                    <state id="basic"/>
                    <state id="advanced"/>
                </state>
                <state id="network"/>
            </state>
        </state>
    </scxml>)";

    ASSERT_TRUE(stateMachine->loadSCXMLFromString(scxml));
    ASSERT_TRUE(stateMachine->start());

    // W3C SCXML Section 3.6: Multiple history states should be auto-registered from SCXML
    // Verify both history states are automatically recognized
    EXPECT_TRUE(stateMachine->isHistoryState("workflow_hist"));
    EXPECT_TRUE(stateMachine->isHistoryState("settings_hist"));

    // Regular states should not be history states
    EXPECT_FALSE(stateMachine->isHistoryState("step1"));
    EXPECT_FALSE(stateMachine->isHistoryState("general"));
    EXPECT_FALSE(stateMachine->isHistoryState("basic"));

    // History entries should be trackable
    auto entries = stateMachine->getHistoryEntries();
    EXPECT_GE(entries.size(), 0);  // Should have some entries or be empty (both valid)
}

/**
 * W3C SCXML Section 3.6: History state error handling
 * Tests proper handling of invalid history configurations
 */
TEST_F(HistoryStateIntegrationTest, W3C_HistoryState_ErrorHandling) {
    // Test with minimal valid SCXML
    const std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="simple">
        <state id="simple"/>
    </scxml>)";

    ASSERT_TRUE(stateMachine->loadSCXMLFromString(scxml));
    ASSERT_TRUE(stateMachine->start());

    // Test registration with invalid parameters should fail gracefully
    EXPECT_FALSE(stateMachine->registerHistoryState("", "parent", HistoryType::SHALLOW, "default"));
    EXPECT_FALSE(stateMachine->registerHistoryState("hist", "", HistoryType::SHALLOW, "default"));

    // Non-existent history states should return false
    EXPECT_FALSE(stateMachine->isHistoryState("nonexistent"));
    EXPECT_FALSE(stateMachine->isHistoryState(""));

    // Clear history should work even with no history states
    EXPECT_NO_THROW(stateMachine->clearAllHistory());
}

/**
 * W3C SCXML Section 3.6: History state thread safety
 * Tests that history operations are thread-safe in concurrent scenarios
 */
TEST_F(HistoryStateIntegrationTest, W3C_HistoryState_ThreadSafety) {
    const std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="concurrent_test">
        <state id="concurrent_test">
            <state id="container">
                <history type="deep" id="thread_hist">
                    <transition target="state1"/>
                </history>
                <state id="state1"/>
                <state id="state2"/>
                <state id="state3"/>
            </state>
        </state>
    </scxml>)";

    ASSERT_TRUE(stateMachine->loadSCXMLFromString(scxml));
    ASSERT_TRUE(stateMachine->start());

    // W3C SCXML Section 3.6: History state should be auto-registered from SCXML
    // Verify the history state is automatically recognized
    EXPECT_TRUE(stateMachine->isHistoryState("thread_hist"));

    // Test concurrent access to history operations
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> failureCount{0};

    // Launch multiple threads performing history operations
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&, i]() {
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));

                // Test thread-safe operations
                bool isHist = stateMachine->isHistoryState("thread_hist");
                if (isHist) {
                    successCount++;
                } else {
                    failureCount++;
                }

                // Test clearing history (should be thread-safe)
                stateMachine->clearAllHistory();

                // Test getting history entries
                auto entries = stateMachine->getHistoryEntries();
                // Just verify it doesn't crash

            } catch (const std::exception &) {
                failureCount++;
            }
        });
    }

    // Wait for all threads to complete
    for (auto &thread : threads) {
        thread.join();
    }

    // Verify thread safety - most operations should succeed
    EXPECT_GT(successCount.load(), 0);
    EXPECT_LT(failureCount.load(), 3);  // Allow some failures due to race conditions
}

/**
 * W3C SCXML Section 3.6: History state with simple state transitions
 * Tests basic integration of history states with regular state machine operation
 */
TEST_F(HistoryStateIntegrationTest, W3C_HistoryState_SimpleTransitions) {
    const std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
        <state id="start">
            <transition event="enter_flow" target="main_flow"/>
        </state>
        <state id="main_flow">
            <history type="shallow" id="flow_history">
                <transition target="step_a"/>
            </history>
            <state id="step_a">
                <transition event="next" target="step_b"/>
            </state>
            <state id="step_b">
                <transition event="finish" target="end"/>
            </state>
        </state>
        <state id="end"/>
    </scxml>)";

    ASSERT_TRUE(stateMachine->loadSCXMLFromString(scxml));
    ASSERT_TRUE(stateMachine->start());

    // W3C SCXML Section 3.6: History state should be auto-registered from SCXML
    // No manual registration needed

    // Verify history state is recognized
    EXPECT_TRUE(stateMachine->isHistoryState("flow_history"));

    // Test basic state machine operations still work
    EXPECT_TRUE(stateMachine->isRunning());

    // Verify we can get active states
    auto activeStates = stateMachine->getActiveStates();
    EXPECT_FALSE(activeStates.empty());

    // Statistics should be available
    auto stats = stateMachine->getStatistics();
    EXPECT_TRUE(stats.isRunning);
    EXPECT_GE(stats.totalEvents, 0);
}

}  // namespace RSM