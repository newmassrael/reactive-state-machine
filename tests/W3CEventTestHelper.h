#pragma once

#include "SCXMLTypes.h"
#include "scripting/JSEngine.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace RSM {
namespace Tests {

/**
 * W3C SCXML 5.10 Test Helper
 *
 * Provides reusable test utilities for validating W3C SCXML 5.10 compliance:
 * "_event must NOT be bound at initialization time until the first event is processed"
 *
 * Usage:
 *   class MyTest : public ::testing::Test {
 *   protected:
 *       W3CEventTestHelper helper_;
 *
 *       void SetUp() override {
 *           engine_ = &JSEngine::instance();
 *           engine_->reset();
 *           sessionId_ = "test_session";
 *           engine_->createSession(sessionId_, "");
 *           helper_.initialize(engine_, sessionId_);
 *       }
 *   };
 */
class W3CEventTestHelper {
public:
    // Test constants
    static constexpr const char *TEST_EVENT_NAME = "test.event";
    static constexpr const char *EVENT_TYPE_INTERNAL = "internal";
    static constexpr const char *TYPEOF_EVENT_EXPR = "typeof _event";

    W3CEventTestHelper() = default;

    /**
     * Initialize helper with JSEngine and session ID
     * Must be called in SetUp() after session is created
     */
    void initialize(RSM::JSEngine *engine, const std::string &sessionId) {
        engine_ = engine;
        sessionId_ = sessionId;
    }

    /**
     * Trigger an event to initialize _event object
     *
     * @param name Event name (defaults to TEST_EVENT_NAME)
     * @param type Event type (defaults to EVENT_TYPE_INTERNAL)
     */
    void triggerEvent(const std::string &name = TEST_EVENT_NAME, const std::string &type = EVENT_TYPE_INTERNAL) {
        ASSERT_NE(engine_, nullptr) << "Helper not initialized - call initialize() first";

        auto event = std::make_shared<RSM::Event>(name, type);
        auto result = engine_->setCurrentEvent(sessionId_, event).get();
        ASSERT_TRUE(result.isSuccess()) << "Failed to trigger event '" << name << "' (type: " << type << ")";
    }

    /**
     * Assert that _event is undefined (not bound)
     * W3C SCXML 5.10: Should be true before first event
     */
    void assertEventUndefined() {
        ASSERT_NE(engine_, nullptr) << "Helper not initialized - call initialize() first";

        auto result = engine_->evaluateExpression(sessionId_, TYPEOF_EVENT_EXPR).get();
        ASSERT_TRUE(result.isSuccess()) << "Failed to evaluate '" << TYPEOF_EVENT_EXPR << "'";
        EXPECT_EQ(result.getValue<std::string>(), "undefined")
            << "_event should NOT be bound before first event (W3C SCXML 5.10)";
    }

    /**
     * Assert that _event is an object (bound)
     * W3C SCXML 5.10: Should be true after first event
     */
    void assertEventObject() {
        ASSERT_NE(engine_, nullptr) << "Helper not initialized - call initialize() first";

        auto result = engine_->evaluateExpression(sessionId_, TYPEOF_EVENT_EXPR).get();
        ASSERT_TRUE(result.isSuccess()) << "Failed to evaluate '" << TYPEOF_EVENT_EXPR << "'";
        EXPECT_EQ(result.getValue<std::string>(), "object")
            << "_event should be bound after first event (W3C SCXML 5.10)";
    }

    /**
     * Verify that a specific _event property is read-only
     * W3C SCXML: Event object properties must be immutable
     *
     * @param prop Property name to verify
     */
    void verifyPropertyReadOnly(const std::string &prop) {
        ASSERT_NE(engine_, nullptr) << "Helper not initialized - call initialize() first";

        // Try to modify property - should throw error
        std::string modifyScript = "_event." + prop + " = 'modified_value'; _event." + prop;
        auto modifyResult = engine_->executeScript(sessionId_, modifyScript).get();

        // SCXML W3C compliant: modification should fail
        EXPECT_FALSE(modifyResult.isSuccess())
            << "Modification of _event." << prop << " should fail (W3C SCXML requires read-only properties)";

        // Verify property remains unchanged
        std::string checkScript = "_event." + prop;
        auto checkResult = engine_->evaluateExpression(sessionId_, checkScript).get();
        ASSERT_TRUE(checkResult.isSuccess()) << "Failed to evaluate _event." << prop << " after modification attempt";

        // Properties should still have their default values
        if (prop == "data") {
            auto dataCheck = engine_->evaluateExpression(sessionId_, "_event.data === undefined").get();
            ASSERT_TRUE(dataCheck.isSuccess()) << "Failed to check if _event.data is undefined";
            EXPECT_TRUE(dataCheck.getValue<bool>()) << "_event.data should remain undefined after modification attempt";
        } else {
            EXPECT_EQ(checkResult.getValue<std::string>(), "")
                << "_event." << prop << " should remain empty string after modification attempt";
        }
    }

    /**
     * Get the JSEngine instance
     */
    RSM::JSEngine *getEngine() const {
        return engine_;
    }

    /**
     * Get the session ID
     */
    const std::string &getSessionId() const {
        return sessionId_;
    }

private:
    RSM::JSEngine *engine_ = nullptr;
    std::string sessionId_;
};

}  // namespace Tests
}  // namespace RSM
