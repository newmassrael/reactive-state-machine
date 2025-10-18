#pragma once

#include "JitTestBase.h"
#include "JitTestRegistry.h"
#include <thread>

namespace RSM::W3C::JitTests {

/**
 * @brief CRTP template for simple JIT tests
 *
 * Simplifies test creation for most common pattern:
 * 1. Create state machine
 * 2. Initialize
 * 3. Check final state is Pass
 *
 * Usage:
 * @code
 * #include "test144_sm.h"
 * struct Test144 : public SimpleJitTest<Test144, 144> {
 *     static constexpr const char* DESCRIPTION = "Event queue ordering";
 *     using SM = RSM::Generated::test144::test144;
 * };
 * REGISTER_JIT_TEST(Test144);
 * @endcode
 */
template <typename Derived, int TestNum> class SimpleJitTest : public JitTestBase {
public:
    static constexpr int TEST_ID = TestNum;

    bool run() override {
        using SM = typename Derived::SM;
        SM sm;
        sm.initialize();
        auto finalState = sm.getCurrentState();
        bool isFinished = sm.isInFinalState();
        bool isPass = (finalState == SM::State::Pass);
        LOG_DEBUG("JIT Test {}: isInFinalState={}, currentState={}, isPass={}", TEST_ID, isFinished,
                  static_cast<int>(finalState), isPass);
        return isFinished && isPass;
    }

    int getTestId() const override {
        return TEST_ID;
    }

    const char *getDescription() const override {
        return Derived::DESCRIPTION;
    }
};

/**
 * @brief CRTP template for JIT tests requiring event scheduler polling
 *
 * Used for tests with delayed send or invoke that need tick() polling.
 *
 * Usage:
 * @code
 * #include "test175_sm.h"
 * struct Test175 : public ScheduledJitTest<Test175, 175> {
 *     static constexpr const char* DESCRIPTION = "Send delayexpr";
 *     using SM = RSM::Generated::test175::test175;
 * };
 * REGISTER_JIT_TEST(Test175);
 * @endcode
 */
template <typename Derived, int TestNum> class ScheduledJitTest : public JitTestBase {
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
                return false;
            }

            // Sleep briefly to allow scheduled events to become ready
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // W3C SCXML 6.2: Poll scheduler with Event::NONE
            sm.tick();
        }

        return sm.getCurrentState() == SM::State::Pass;
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
};

}  // namespace RSM::W3C::JitTests
