#pragma once
#include "SimpleAotTest.h"
#include "test388_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.11: History states work correctly (deep/shallow)
 *
 * Tests that history states work correctly. The counter Var1 counts how many times
 * we have entered s0. The initial state is s012. We then transition to s1, which
 * transitions to s0's deep history state. entering.s012 should be raised. Then we
 * transition to s02, which transitions to s0's shallow history state. That should
 * have value s01, and its initial state s011 should be entered.
 */
struct Test388 : public SimpleAotTest<Test388, 388> {
    static constexpr const char *DESCRIPTION = "History states work correctly (W3C 3.11 AOT)";
    using SM = RSM::Generated::test388::test388;
};

// Auto-register
inline static AotTestRegistrar<Test388> registrar_Test388;

}  // namespace RSM::W3C::AotTests
