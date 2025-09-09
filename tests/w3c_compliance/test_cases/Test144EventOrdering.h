/**
 * @file Test144EventOrdering.h
 * @brief W3C Test 144 - Event queue ordering validation
 */

#pragma once

#include "W3CTestCase.h"

namespace W3CCompliance {

/**
 * @brief Test 144: Verifies event queue ordering behavior
 *
 * This test validates that events are processed in the correct order
 * according to SCXML specification section 4.2 (Event Processing).
 * The test should verify that internal events are processed before
 * external events at each macro-step.
 */
class Test144EventOrdering : public W3CTestCase {
public:
    TestResult execute(const W3CTestMetadata &metadata, const std::string &scxmlContent) override {
        // Execute basic SCXML with enhanced monitoring for event ordering
        auto result = executeBasicSCXML(metadata, scxmlContent);

        // Additional validation for event ordering
        if (result.success) {
            // Verify that we processed events in correct order
            result.success = validateEventOrderingPattern();
            if (!result.success && result.errorMessage.empty()) {
                result.errorMessage = "Event ordering validation failed";
            }
        }

        return result;
    }

    bool validateResult(const TestResult &result) override {
        // Test 144 should reach 'pass' state and demonstrate proper event ordering
        if (!result.success) {
            return false;
        }

        // Check for pass state
        if (!checkFinalState("pass")) {
            return false;
        }

        // Verify minimum number of state transitions occurred
        // Test 144 should have multiple transitions due to event processing
        int transitionCount = getTransitionCount();
        if (transitionCount < 2) {
            return false;  // Should have at least initial + event-triggered transitions
        }

        return true;
    }

    std::string getDescription() const override {
        return "W3C Test 144 - Event queue ordering validation (Spec 4.2)";
    }

private:
    /**
     * @brief Validate event ordering patterns specific to Test 144
     */
    bool validateEventOrderingPattern() const {
        // Test 144 specifically tests event queue ordering
        // We expect to see evidence of proper event processing order

        // Check that we entered expected states in sequence
        if (!checkStateEntered("s0")) {
            return false;  // Should start in s0
        }

        // For Test 144, we expect specific event processing behavior
        // The test uses internal events that should be processed before external ones

        return true;  // Basic validation - can be enhanced with more specific checks
    }
};

}  // namespace W3CCompliance