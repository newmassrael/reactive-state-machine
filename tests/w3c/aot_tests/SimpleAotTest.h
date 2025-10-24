#pragma once

#include "AotTestBase.h"
#include "AotTestRegistry.h"
#include <thread>

namespace RSM::W3C::AotTests {

/**
 * @brief CRTP template for simple AOT tests
 *
 * Simplifies test creation for most common pattern:
 * 1. Create state machine
 * 2. Initialize
 * 3. Check final state is Pass
 *
 * Usage:
 * @code
 * #include "test144_sm.h"
 * struct Test144 : public SimpleAotTest<Test144, 144> {
 *     static constexpr const char* DESCRIPTION = "Event queue ordering";
 *     using SM = RSM::Generated::test144::test144;
 * };
 * REGISTER_AOT_TEST(Test144);
 * @endcode
 */
template <typename Derived, int TestNum> class SimpleAotTest : public AotTestBase {
public:
    static constexpr int TEST_ID = TestNum;

    bool run() override {
        using SM = typename Derived::SM;
        SM sm;
        sm.initialize();
        auto finalState = sm.getCurrentState();
        bool isFinished = sm.isInFinalState();

        // W3C SCXML: Check success state (default: Pass, override with PASS_STATE)
        // Policy-based design: Derived class can define PASS_STATE for custom success states
        bool isPass;
        if constexpr (requires { Derived::PASS_STATE; }) {
            // Manual test or custom success state
            isPass = (finalState == Derived::PASS_STATE);
        } else {
            // Standard test: success = Pass state
            isPass = (finalState == SM::State::Pass);
        }

        LOG_DEBUG("AOT Test {}: isInFinalState={}, currentState={}, isPass={}", TEST_ID, isFinished,
                  static_cast<int>(finalState), isPass);
        return isFinished && isPass;
    }

    int getTestId() const override {
        return TEST_ID;
    }

    const char *getDescription() const override {
        return Derived::DESCRIPTION;
    }

    /**
     * @brief Get test type: pure_static or static_hybrid
     *
     * Uses Policy::NEEDS_JSENGINE to determine if test uses JSEngine
     * for ECMAScript expression evaluation (In(), typeof, _event, etc.)
     */
    const char *getTestType() const {
        using SM = typename Derived::SM;
        using Policy = typename SM::PolicyType;
        return Policy::NEEDS_JSENGINE ? "static_hybrid" : "pure_static";
    }
};

/**
 * @brief CRTP template for AOT tests requiring event scheduler polling
 *
 * Used for tests with delayed send or invoke that need tick() polling.
 *
 * Usage:
 * @code
 * #include "test175_sm.h"
 * struct Test175 : public ScheduledAotTest<Test175, 175> {
 *     static constexpr const char* DESCRIPTION = "Send delayexpr";
 *     using SM = RSM::Generated::test175::test175;
 * };
 * REGISTER_AOT_TEST(Test175);
 * @endcode
 */
template <typename Derived, int TestNum> class ScheduledAotTest : public AotTestBase {
public:
    static constexpr int TEST_ID = TestNum;

    bool run() override {
        using SM = typename Derived::SM;
        SM sm;
        sm.initialize();

        // W3C SCXML 6.2: Process scheduled events until completion or timeout
        auto startTime = std::chrono::steady_clock::now();
        const auto timeout = getTimeout();

        while (!sm.isInFinalState()) {
            // Check for timeout
            if (std::chrono::steady_clock::now() - startTime > timeout) {
                // Cleanup JSEngine session before returning to prevent stack-use-after-return
                sm.getPolicy().ensureJSEngineSessionDestroyed();
                return false;
            }

            // Sleep briefly to allow scheduled events to become ready
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // W3C SCXML 6.2: Poll scheduler with Event::NONE
            sm.tick();
        }

        bool result = sm.getCurrentState() == SM::State::Pass;

        // W3C SCXML: Cleanup JSEngine session before stack unwinding
        // This prevents stack-use-after-return when JSEngine background thread
        // tries to call In() predicate callbacks after sm is destroyed
        sm.getPolicy().ensureJSEngineSessionDestroyed();

        return result;
    }

    int getTestId() const override {
        return TEST_ID;
    }

    const char *getDescription() const override {
        return Derived::DESCRIPTION;
    }

    bool needsSchedulerPolling() const override {
        return true;
    }

    /**
     * @brief Get test type: pure_static or static_hybrid
     *
     * Uses Policy::NEEDS_JSENGINE to determine if test uses JSEngine
     * for ECMAScript expression evaluation (In(), typeof, _event, etc.)
     */
    const char *getTestType() const {
        using SM = typename Derived::SM;
        using Policy = typename SM::PolicyType;
        return Policy::NEEDS_JSENGINE ? "static_hybrid" : "pure_static";
    }
};

}  // namespace RSM::W3C::AotTests
