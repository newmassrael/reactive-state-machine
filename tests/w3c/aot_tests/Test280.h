#pragma once

#include "SimpleAotTest.h"
#include "test280_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.3: Late binding variable initialization
 *
 * Tests that variables with late binding are assigned values only when the state
 * containing them is entered, not at state machine initialization time.
 * Variable Var2 defined in state s1's datamodel should be initialized when s1 is entered,
 * not accessible from state s0.
 */
struct Test280 : public SimpleAotTest<Test280, 280> {
    static constexpr const char *DESCRIPTION = "Late binding variable initialization (W3C 5.3 AOT)";
    using SM = RSM::Generated::test280::test280;
};

// Auto-register
inline static AotTestRegistrar<Test280> registrar_Test280;

}  // namespace RSM::W3C::AotTests
