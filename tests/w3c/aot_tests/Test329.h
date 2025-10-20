#pragma once
#include "AotTestBase.h"
#include "AotTestRegistry.h"
#include "test329_sm.h"
#include <spdlog/spdlog.h>

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10: System variables immutability
 *
 * Tests that system variables (_sessionid, _event, _name, _ioprocessors)
 * are immutable and cannot be modified via assign operations.
 *
 * Expected behavior:
 * - s0: Attempt to assign to _sessionid → error.execution → s1
 * - s1: Attempt to assign to _event → error.execution → s2
 * - s2: Attempt to assign to _name → error.execution → s3
 * - s3: Attempt to assign to _ioprocessors → error.execution → pass
 * - All assignment attempts must fail and trigger error.execution events
 */
struct Test329 : public AotTestBase {
    static constexpr int TEST_ID = 329;
    static constexpr const char *DESCRIPTION = "System variables immutability (W3C 5.10 AOT)";
    using SM = RSM::Generated::test329::test329;

    bool run() override {
        SM sm;

        LOG_DEBUG("Test329 Debug: Before initialize");
        sm.initialize();
        LOG_DEBUG("Test329 Debug: After initialize");

        bool isInFinal = sm.isInFinalState();
        auto currentState = sm.getCurrentState();
        bool isPass = (currentState == SM::State::Pass);

        LOG_DEBUG("Test329 Debug: isInFinalState={}, currentState={}, Pass={}, Fail={}", isInFinal,
                  static_cast<int>(currentState), static_cast<int>(SM::State::Pass), static_cast<int>(SM::State::Fail));

        if (currentState == SM::State::Fail) {
            LOG_ERROR("Test329: Reached FAIL state instead of PASS!");
        }

        return isInFinal && isPass;
    }

    int getTestId() const override {
        return TEST_ID;
    }

    const char *getDescription() const override {
        return DESCRIPTION;
    }
};

// Auto-register
inline static AotTestRegistrar<Test329> registrar_Test329;

}  // namespace RSM::W3C::AotTests
