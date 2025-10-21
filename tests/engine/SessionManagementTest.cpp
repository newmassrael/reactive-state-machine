#include "scripting/JSEngine.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "events/EventDispatcherImpl.h"
#include "events/EventSchedulerImpl.h"
#include "events/EventTargetFactoryImpl.h"
#include "mocks/MockEventRaiser.h"
#include "runtime/ActionExecutorImpl.h"

class SessionManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &RSM::JSEngine::instance();
        // Ensure test isolation with JSEngine reset
        engine_->reset();
    }

    void TearDown() override {
        if (engine_) {
            engine_->shutdown();
        }
    }

    RSM::JSEngine *engine_;
};

// Test session creation and validation
TEST_F(SessionManagementTest, CreateSession) {
    bool result = engine_->createSession("test_session", "");
    EXPECT_TRUE(result) << "Failed to create session";

    // Test session exists by trying to evaluate something
    auto evalResult = engine_->evaluateExpression("test_session", "1 + 1").get();
    EXPECT_TRUE(evalResult.isSuccess()) << "Session doesn't seem to exist";

    // Cleanup
    engine_->destroySession("test_session");
}

// Test session creation with initial script
TEST_F(SessionManagementTest, CreateSessionWithScript) {
    bool result = engine_->createSession("script_session", "");
    EXPECT_TRUE(result) << "Failed to create session with script";

    // Set initial variable
    auto initResult = engine_->executeScript("script_session", "var x = 42;").get();
    EXPECT_TRUE(initResult.isSuccess());

    // Test that the initial script was executed
    auto evalResult = engine_->evaluateExpression("script_session", "x").get();
    EXPECT_TRUE(evalResult.isSuccess());
    EXPECT_EQ(evalResult.getValue<double>(), 42.0);

    // Cleanup
    engine_->destroySession("script_session");
}

// Test duplicate session creation fails
TEST_F(SessionManagementTest, CreateDuplicateSession) {
    bool result1 = engine_->createSession("duplicate_session", "");
    EXPECT_TRUE(result1);

    bool result2 = engine_->createSession("duplicate_session", "");
    EXPECT_FALSE(result2) << "Duplicate session creation should fail";

    // Cleanup
    engine_->destroySession("duplicate_session");
}

// Test session destruction
TEST_F(SessionManagementTest, DestroySession) {
    bool createResult = engine_->createSession("temp_session", "");
    EXPECT_TRUE(createResult);

    // Session should work before destruction
    auto evalResult1 = engine_->evaluateExpression("temp_session", "1 + 1").get();
    EXPECT_TRUE(evalResult1.isSuccess());

    // Destroy session
    bool destroyResult = engine_->destroySession("temp_session");
    EXPECT_TRUE(destroyResult);

    // Session should not work after destruction
    auto evalResult2 = engine_->evaluateExpression("temp_session", "1 + 1").get();
    EXPECT_FALSE(evalResult2.isSuccess()) << "Session should not exist after destruction";
}

// Test destroying non-existent session
TEST_F(SessionManagementTest, DestroyNonExistentSession) {
    bool result = engine_->destroySession("non_existent_session");
    EXPECT_FALSE(result) << "Destroying non-existent session should fail";
}

// Test session variable isolation
TEST_F(SessionManagementTest, SessionVariableIsolation) {
    // Create two sessions
    bool result1 = engine_->createSession("session1", "");
    bool result2 = engine_->createSession("session2", "");

    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);

    // Set different values in each session
    auto set1 = engine_->executeScript("session1", "var value = 100;").get();
    auto set2 = engine_->executeScript("session2", "var value = 200;").get();

    EXPECT_TRUE(set1.isSuccess());
    EXPECT_TRUE(set2.isSuccess());

    // Check that variables are isolated
    auto eval1 = engine_->evaluateExpression("session1", "value").get();
    auto eval2 = engine_->evaluateExpression("session2", "value").get();

    EXPECT_TRUE(eval1.isSuccess());
    EXPECT_TRUE(eval2.isSuccess());
    EXPECT_EQ(eval1.getValue<double>(), 100.0);
    EXPECT_EQ(eval2.getValue<double>(), 200.0);

    // Modify variable in one session
    auto setResult = engine_->executeScript("session1", "value = 999;").get();
    EXPECT_TRUE(setResult.isSuccess());

    // Check isolation is maintained
    auto eval1b = engine_->evaluateExpression("session1", "value").get();
    auto eval2b = engine_->evaluateExpression("session2", "value").get();

    EXPECT_TRUE(eval1b.isSuccess());
    EXPECT_TRUE(eval2b.isSuccess());
    EXPECT_EQ(eval1b.getValue<double>(), 999.0);
    EXPECT_EQ(eval2b.getValue<double>(), 200.0);  // Should remain unchanged

    // Cleanup
    engine_->destroySession("session1");
    engine_->destroySession("session2");
}

// Test concurrent session operations
TEST_F(SessionManagementTest, ConcurrentSessionOperations) {
    const int numSessions = 5;
    std::vector<std::future<void>> futures;

    // Create sessions concurrently
    for (int i = 0; i < numSessions; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            std::string sessionId = "concurrent_session_" + std::to_string(i);

            bool result = engine_->createSession(sessionId, "");
            EXPECT_TRUE(result) << "Failed to create session " << i;

            // Set session-specific variable
            std::string script = "var sessionNum = " + std::to_string(i) + ";";
            auto initResult = engine_->executeScript(sessionId, script).get();
            EXPECT_TRUE(initResult.isSuccess());

            // Test the session works
            auto evalResult = engine_->evaluateExpression(sessionId, "sessionNum").get();
            EXPECT_TRUE(evalResult.isSuccess());
            EXPECT_EQ(evalResult.getValue<double>(), (double)i);
        }));
    }

    // Wait for all operations to complete
    for (auto &future : futures) {
        future.wait();
    }

    // Cleanup
    for (int i = 0; i < numSessions; ++i) {
        std::string sessionId = "concurrent_session_" + std::to_string(i);
        engine_->destroySession(sessionId);
    }
}

// Test concurrent script execution within sessions
TEST_F(SessionManagementTest, ConcurrentScriptExecution) {
    bool createResult = engine_->createSession("concurrent_exec_session", "");
    EXPECT_TRUE(createResult);

    // Initialize counter
    auto initResult = engine_->executeScript("concurrent_exec_session", "var counter = 0;").get();
    EXPECT_TRUE(initResult.isSuccess());

    const int numOperations = 10;
    std::vector<std::future<void>> futures;

    // Execute scripts concurrently in the same session
    for (int i = 0; i < numOperations; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            std::string script = "counter += " + std::to_string(i + 1) + ";";
            auto result = engine_->executeScript("concurrent_exec_session", script).get();
            EXPECT_TRUE(result.isSuccess()) << "Failed to execute script " << i;
        }));
    }

    // Wait for all operations to complete
    for (auto &future : futures) {
        future.wait();
    }

    // Check final counter value (should be sum of 1+2+...+10 = 55)
    auto evalResult = engine_->evaluateExpression("concurrent_exec_session", "counter").get();
    EXPECT_TRUE(evalResult.isSuccess());
    EXPECT_EQ(evalResult.getValue<double>(), 55.0);

    // Cleanup
    engine_->destroySession("concurrent_exec_session");
}

// Test session cleanup on shutdown
TEST_F(SessionManagementTest, SessionCleanupOnShutdown) {
    // Create a few sessions
    bool result1 = engine_->createSession("cleanup_session1", "");
    bool result2 = engine_->createSession("cleanup_session2", "");

    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);

    // Sessions should work
    auto eval1 = engine_->evaluateExpression("cleanup_session1", "1 + 1").get();
    auto eval2 = engine_->evaluateExpression("cleanup_session2", "2 + 2").get();

    EXPECT_TRUE(eval1.isSuccess());
    EXPECT_TRUE(eval2.isSuccess());

    // Shutdown should clean up sessions automatically
    engine_->shutdown();

    // Re-initialize for TearDown
    // JSEngine is automatically initialized in constructor (RAII)
}

// Test max sessions stress test
TEST_F(SessionManagementTest, MaxSessionsStressTest) {
    const int maxSessions = 20;  // Reasonable limit for testing
    std::vector<std::string> sessionIds;

    // Create many sessions
    for (int i = 0; i < maxSessions; ++i) {
        std::string sessionId = "stress_session_" + std::to_string(i);
        sessionIds.push_back(sessionId);

        bool result = engine_->createSession(sessionId, "");
        EXPECT_TRUE(result) << "Failed to create session " << i;

        // Set session-specific variable
        std::string script = "var id = " + std::to_string(i) + ";";
        auto initResult = engine_->executeScript(sessionId, script).get();
        EXPECT_TRUE(initResult.isSuccess());

        // Quick validation
        auto evalResult = engine_->evaluateExpression(sessionId, "id").get();
        EXPECT_TRUE(evalResult.isSuccess());
        EXPECT_EQ(evalResult.getValue<double>(), (double)i);
    }

    // Cleanup all sessions
    for (const auto &sessionId : sessionIds) {
        bool result = engine_->destroySession(sessionId);
        EXPECT_TRUE(result) << "Failed to destroy session " << sessionId;
    }
}

// Test invalid session operations
TEST_F(SessionManagementTest, InvalidSessionOperations) {
    // Try to use non-existent session
    auto evalResult = engine_->evaluateExpression("non_existent", "1 + 1").get();
    EXPECT_FALSE(evalResult.isSuccess()) << "Should fail for non-existent session";

    auto execResult = engine_->executeScript("non_existent", "var x = 1;").get();
    EXPECT_FALSE(execResult.isSuccess()) << "Should fail for non-existent session";

    // Try to create session with empty ID
    bool createResult = engine_->createSession("", "");
    EXPECT_FALSE(createResult) << "Should fail for empty session ID";
}

// Test event scheduling specific scenario: rapid sequential session creation like in EventSchedulingTest
TEST_F(SessionManagementTest, EventSchedulingPatternTest) {
    // This test reproduces the exact pattern from EventSchedulingTest.SetUp()
    // to verify if rapid sequential session creation causes deadlock

    // Step 1: Create first session (like test_session in event scheduling)
    bool result1 = engine_->createSession("test_session", "");
    EXPECT_TRUE(result1) << "Failed to create first session";

    // Step 2: Immediately create second session (like temp_session in event scheduling)
    bool result2 = engine_->createSession("temp_session", "");
    EXPECT_TRUE(result2) << "Failed to create second session";

    // Step 3: Verify both sessions work
    auto eval1 = engine_->evaluateExpression("test_session", "1 + 1").get();
    EXPECT_TRUE(eval1.isSuccess()) << "First session should work";

    auto eval2 = engine_->evaluateExpression("temp_session", "2 + 2").get();
    EXPECT_TRUE(eval2.isSuccess()) << "Second session should work";

    // Step 4: Cleanup
    engine_->destroySession("test_session");
    engine_->destroySession("temp_session");
}

// Test ActionExecutor pattern from event scheduling: create session then ActionExecutor
TEST_F(SessionManagementTest, ActionExecutorCreationPatternTest) {
    // This test reproduces the ActionExecutor creation pattern

    // Step 1: Create session first (like JSEngine createSession in event scheduling)
    bool sessionResult = engine_->createSession("test_session", "");
    EXPECT_TRUE(sessionResult) << "Failed to create session";

    // Step 2: Simulate ActionExecutorImpl creation (this would internally check JSEngine)
    // In real ActionExecutorImpl, this calls isSessionReady() which calls hasSession()
    // We simulate this by checking if session exists
    auto checkResult = engine_->evaluateExpression("test_session", "typeof undefined").get();
    EXPECT_TRUE(checkResult.isSuccess()) << "Session should be accessible";

    // Step 3: Try to create another ActionExecutor with different session
    // This simulates the temp_session creation in EventTargetFactoryImpl
    bool tempSessionResult = engine_->createSession("temp_session", "");
    EXPECT_TRUE(tempSessionResult) << "Failed to create temp session";

    // Step 4: Verify original session still works
    auto originalCheck = engine_->evaluateExpression("test_session", "1 + 1").get();
    EXPECT_TRUE(originalCheck.isSuccess()) << "Original session should still work";

    // Step 5: Cleanup
    engine_->destroySession("test_session");
    engine_->destroySession("temp_session");
}

// Test event scheduling component creation step by step to isolate hanging issue
TEST_F(SessionManagementTest, EventSchedulingComponentCreationStepByStepTest) {
    // This test reproduces event scheduling component creation step by step

    // Step 1: Create JSEngine session (this works)
    bool sessionResult = engine_->createSession("test_session", "");
    EXPECT_TRUE(sessionResult) << "Failed to create JSEngine session";

    // Step 2: Create ActionExecutor (potential hang point?)
    try {
        auto actionExecutor = std::make_shared<RSM::ActionExecutorImpl>("test_session");
        EXPECT_TRUE(actionExecutor != nullptr) << "ActionExecutor creation failed";

        // Step 3: Create EventTargetFactory with MockEventRaiser (potential hang point?)
        auto mockEventRaiser = std::make_shared<RSM::Test::MockEventRaiser>(
            [](const std::string &, const std::string &) -> bool { return true; });
        actionExecutor->setEventRaiser(mockEventRaiser);
        auto targetFactory = std::make_shared<RSM::EventTargetFactoryImpl>(mockEventRaiser);
        EXPECT_TRUE(targetFactory != nullptr) << "EventTargetFactory creation failed";

        // If we get here, the problem is NOT in basic component creation
        SUCCEED() << "All basic components created successfully - issue must be elsewhere";

    } catch (const std::exception &e) {
        FAIL() << "Exception during component creation: " << e.what();
    }

    // Cleanup
    engine_->destroySession("test_session");
}

// Test EventScheduler and EventDispatcher creation
TEST_F(SessionManagementTest, EventSchedulerCreationTest) {
    // Step 1: Basic setup (we know this works)
    bool sessionResult = engine_->createSession("test_session", "");
    EXPECT_TRUE(sessionResult);

    auto actionExecutor = std::make_shared<RSM::ActionExecutorImpl>("test_session");
    auto mockEventRaiser = std::make_shared<RSM::Test::MockEventRaiser>(
        [](const std::string &, const std::string &) -> bool { return true; });
    actionExecutor->setEventRaiser(mockEventRaiser);
    auto targetFactory = std::make_shared<RSM::EventTargetFactoryImpl>(mockEventRaiser);

    try {
        // Step 2: Create EventExecutionCallback (potential hang point?)
        RSM::EventExecutionCallback callback = [](const RSM::EventDescriptor &event,
                                                  std::shared_ptr<RSM::IEventTarget> target,
                                                  const std::string &sendId) -> bool {
            (void)event;
            (void)target;
            (void)sendId;  // Suppress unused parameter warnings
            return true;
        };

        // Step 3: Create EventSchedulerImpl (major potential hang point!)
        auto scheduler = std::make_shared<RSM::EventSchedulerImpl>(callback);
        EXPECT_TRUE(scheduler != nullptr) << "EventSchedulerImpl creation failed";

        // Step 4: Create EventDispatcherImpl (potential hang point?)
        auto dispatcher = std::make_shared<RSM::EventDispatcherImpl>(scheduler, targetFactory);
        EXPECT_TRUE(dispatcher != nullptr) << "EventDispatcherImpl creation failed";

        // Step 5: Test if we can create ActionExecutor with dispatcher
        auto actionExecutorWithDispatcher = std::make_shared<RSM::ActionExecutorImpl>("test_session", dispatcher);
        EXPECT_TRUE(actionExecutorWithDispatcher != nullptr) << "ActionExecutor with dispatcher failed";

        SUCCEED() << "All event scheduling components created successfully!";

        // Cleanup scheduler and dispatcher properly
        scheduler->shutdown();
        dispatcher->shutdown();

    } catch (const std::exception &e) {
        FAIL() << "Exception during event scheduling component creation: " << e.what();
    }

    // Cleanup
    engine_->destroySession("test_session");
}
