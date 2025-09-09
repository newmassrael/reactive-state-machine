/**
 * @file Test185SendDelay.h
 * @brief W3C Test 185 - Send delay validation
 */

#pragma once

#include "W3CTestCase.h"

namespace W3CCompliance {

/**
 * @brief Test 185: Validates <send> element delay processing
 *
 * This test verifies that SCXML <send> elements with delay attributes
 * properly interpret delay specifications and deliver events in correct order.
 * The test sends event2 with 1-second delay and event1 immediately,
 * expecting event1 to arrive first.
 */
class Test185SendDelay : public W3CTestCase {
public:
    TestResult execute(const W3CTestMetadata &metadata, const std::string &scxmlContent) override {
        // Execute with enhanced monitoring for event timing
        auto result = executeBasicSCXML(metadata, scxmlContent);

        // Additional validation specific to send delay
        if (result.success) {
            result.success = validateEventTiming();
            if (!result.success && result.errorMessage.empty()) {
                result.errorMessage = "Send delay validation failed - events arrived in wrong order";
            }
        }

        return result;
    }

    bool validateResult(const TestResult &result) override {
        // Test 185 should demonstrate proper send delay behavior
        if (!result.success) {
            return false;
        }

        // Should reach pass state if delay timing worked correctly
        if (!checkFinalState("pass")) {
            return false;
        }

        // Verify that events were processed in correct order
        // event1 (immediate) should arrive before event2 (delayed)
        if (!validateSendEventOrdering()) {
            return false;
        }

        return true;
    }

    std::string getDescription() const override {
        return "W3C Test 185 - Send delay validation (Spec 6.2)";
    }

private:
    /**
     * @brief Validate event timing patterns specific to Test 185
     */
    bool validateEventTiming() const {
        // Test 185 specifically tests send delay behavior

        // Should have entered initial state s0
        if (!checkStateEntered("s0")) {
            return false;
        }

        // Should have transitioned to s1 (indicates event1 arrived first)
        if (!checkStateEntered("s1")) {
            return false;
        }

        // The test structure expects:
        // 1. Send event2 with delay
        // 2. Send event1 immediately
        // 3. event1 triggers s0->s1 transition
        // 4. event2 triggers s1->pass transition

        return true;
    }

    /**
     * @brief Validate send event ordering for delay compliance
     */
    bool validateSendEventOrdering() const {
        // Test 185 uses send delay to test event ordering
        // The fact that we reached 'pass' state indicates proper timing

        // Expected sequence:
        // - event1 (immediate) processed first -> s0 to s1
        // - event2 (delayed 1s) processed second -> s1 to pass

        // Check minimum transition count (should have at least 2 transitions)
        int transitionCount = getTransitionCount();
        if (transitionCount < 2) {
            return false;  // Should have s0->s1 and s1->pass transitions
        }

        return true;  // Basic validation - reaching pass state indicates success
    }
};

}  // namespace W3CCompliance