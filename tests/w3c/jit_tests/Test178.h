#pragma once

#include "JitTestBase.h"
#include "JitTestRegistry.h"
#include "test178_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Send with duplicate param names (JIT)
 */
struct Test178 : public JitTestBase {
    static constexpr int TEST_ID = 178;
    static constexpr const char *DESCRIPTION = "Send with duplicate param names (JIT)";

    bool run() override {
        RSM::Generated::test178::test178 sm;
        sm.initialize();
        return sm.isInFinalState() && sm.getCurrentState() == RSM::Generated::test178::State::Final;
    }

    int getTestId() const override {
        return TEST_ID;
    }

    const char *getDescription() const override {
        return DESCRIPTION;
    }
};

// Auto-register
inline static JitTestRegistrar<Test178> registrar_Test178;

}  // namespace RSM::W3C::JitTests
