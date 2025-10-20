#pragma once

#include "SimpleAotTest.h"
#include "test279_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.2.2: Early binding variable initialization
 *
 * Tests that variables with early binding are assigned values at initialization time,
 * before the state containing them is visited.
 * Variable Var1 defined in state s1's datamodel should be initialized before s0 checks it.
 */
struct Test279 : public SimpleAotTest<Test279, 279> {
    static constexpr const char *DESCRIPTION = "Early binding variable initialization (W3C 5.2.2 AOT)";
    using SM = RSM::Generated::test279::test279;
};

// Auto-register
inline static AotTestRegistrar<Test279> registrar_Test279;

}  // namespace RSM::W3C::AotTests
