#pragma once

#include "AotTestBase.h"
#include "AotTestRegistry.h"
#include "test178_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Send with duplicate param names (AOT)
 */
struct Test178 : public AotTestBase {
    static constexpr int TEST_ID = 178;
    static constexpr const char *DESCRIPTION = "Send with duplicate param names (AOT)";

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
inline static AotTestRegistrar<Test178> registrar_Test178;

}  // namespace RSM::W3C::AotTests
