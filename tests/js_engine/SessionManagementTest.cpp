#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <future>
#include <vector>
#include <algorithm>
#include "../../scxml/include/SCXMLEngine.h"

using namespace SCXML;
using SCXML::SessionInfo;

class SessionManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = createSCXMLEngine();
        ASSERT_NE(engine, nullptr);
        ASSERT_TRUE(engine->initialize());
    }

    void TearDown() override {
        if (engine) {
            engine->shutdown();
        }
        engine.reset();
    }

    std::unique_ptr<SCXMLEngine> engine;
};

// Test session creation and validation
TEST_F(SessionManagementTest, CreateSession) {
    EXPECT_TRUE(engine->createSession("test_session"));
    EXPECT_TRUE(engine->hasSession("test_session"));
}

// Test session creation with parent session
TEST_F(SessionManagementTest, CreateSessionWithParent) {
    EXPECT_TRUE(engine->createSession("parent_session"));
    EXPECT_TRUE(engine->createSession("child_session", "parent_session"));
    
    EXPECT_TRUE(engine->hasSession("parent_session"));
    EXPECT_TRUE(engine->hasSession("child_session"));
}

// Test duplicate session creation fails
TEST_F(SessionManagementTest, CreateDuplicateSession) {
    EXPECT_TRUE(engine->createSession("duplicate_session"));
    EXPECT_FALSE(engine->createSession("duplicate_session"));
}

// Test session destruction
TEST_F(SessionManagementTest, DestroySession) {
    EXPECT_TRUE(engine->createSession("temp_session"));
    EXPECT_TRUE(engine->hasSession("temp_session"));
    
    EXPECT_TRUE(engine->destroySession("temp_session"));
    EXPECT_FALSE(engine->hasSession("temp_session"));
}

// Test destroying non-existent session
TEST_F(SessionManagementTest, DestroyNonExistentSession) {
    EXPECT_FALSE(engine->destroySession("non_existent"));
}

// Test session isolation - variables don't leak between sessions
TEST_F(SessionManagementTest, SessionVariableIsolation) {
    EXPECT_TRUE(engine->createSession("session1"));
    EXPECT_TRUE(engine->createSession("session2"));
    
    // Set variable in session1
    auto result1 = engine->setVariable("session1", "testVar", std::string("value1")).get();
    EXPECT_TRUE(result1.success);
    
    // Set different value in session2
    auto result2 = engine->setVariable("session2", "testVar", std::string("value2")).get();
    EXPECT_TRUE(result2.success);
    
    // Check isolation
    auto get1 = engine->getVariable("session1", "testVar").get();
    auto get2 = engine->getVariable("session2", "testVar").get();
    
    EXPECT_TRUE(get1.success);
    EXPECT_TRUE(get2.success);
    EXPECT_EQ(std::get<std::string>(get1.value), "value1");
    EXPECT_EQ(std::get<std::string>(get2.value), "value2");
}

// Test concurrent session operations
TEST_F(SessionManagementTest, ConcurrentSessionOperations) {
    const int num_sessions = 10;
    std::vector<std::future<bool>> futures;
    
    // Create sessions concurrently
    for (int i = 0; i < num_sessions; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            return engine->createSession("session_" + std::to_string(i));
        }));
    }
    
    // Verify all sessions were created
    for (int i = 0; i < num_sessions; ++i) {
        EXPECT_TRUE(futures[i].get());
    }
    
    // Verify all sessions exist
    for (int i = 0; i < num_sessions; ++i) {
        EXPECT_TRUE(engine->hasSession("session_" + std::to_string(i)));
    }
}

// Test concurrent script execution in different sessions
TEST_F(SessionManagementTest, ConcurrentScriptExecution) {
    EXPECT_TRUE(engine->createSession("session_a"));
    EXPECT_TRUE(engine->createSession("session_b"));
    
    // Execute scripts concurrently
    auto future_a = std::async(std::launch::async, [this]() {
        return engine->executeScript("session_a", "var result = 'from_a'; result;").get();
    });
    
    auto future_b = std::async(std::launch::async, [this]() {
        return engine->executeScript("session_b", "var result = 'from_b'; result;").get();
    });
    
    auto result_a = future_a.get();
    auto result_b = future_b.get();
    
    EXPECT_TRUE(result_a.success);
    EXPECT_TRUE(result_b.success);
    EXPECT_EQ(std::get<std::string>(result_a.value), "from_a");
    EXPECT_EQ(std::get<std::string>(result_b.value), "from_b");
}

// Test session cleanup on engine shutdown
TEST_F(SessionManagementTest, SessionCleanupOnShutdown) {
    EXPECT_TRUE(engine->createSession("cleanup_test"));
    
    // Get active sessions
    auto sessions = engine->getActiveSessions();
    EXPECT_FALSE(sessions.empty());
    EXPECT_TRUE(std::find_if(sessions.begin(), sessions.end(), 
        [](const SessionInfo& info) { return info.sessionId == "cleanup_test"; }) != sessions.end());
    
    // Engine will be destroyed in TearDown(), testing cleanup
}

// Test maximum number of sessions
TEST_F(SessionManagementTest, MaxSessionsStressTest) {
    const int max_sessions = 100;
    std::vector<std::string> session_ids;
    
    for (int i = 0; i < max_sessions; ++i) {
        std::string session_id = "stress_session_" + std::to_string(i);
        session_ids.push_back(session_id);
        
        bool created = engine->createSession(session_id);
        EXPECT_TRUE(created) << "Failed to create session " << i;
        
        if (!created) break;
    }
    
    // Verify all sessions are active
    auto active_sessions = engine->getActiveSessions();
    EXPECT_GE(active_sessions.size(), session_ids.size());
    
    // Clean up
    for (const auto& session_id : session_ids) {
        engine->destroySession(session_id);
    }
}

// Test operations on invalid session
TEST_F(SessionManagementTest, InvalidSessionOperations) {
    const std::string invalid_session = "invalid_session";
    
    // Script execution should fail
    auto script_result = engine->executeScript(invalid_session, "1 + 1").get();
    EXPECT_FALSE(script_result.success);
    EXPECT_FALSE(script_result.errorMessage.empty());
    
    // Variable operations should fail
    auto set_result = engine->setVariable(invalid_session, "test", 42).get();
    EXPECT_FALSE(set_result.success);
    
    auto get_result = engine->getVariable(invalid_session, "test").get();
    EXPECT_FALSE(get_result.success);
}