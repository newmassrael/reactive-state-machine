#pragma once
#include "SimpleAotTest.h"
#include "test448_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML B.2: ECMAScript single global scope requirement
 *
 * Tests that all ECMAScript objects are placed in a single global scope,
 * ensuring variables defined in any state's <datamodel> are accessible
 * from any other state in the state machine.
 *
 * W3C SCXML B.2: The ECMAScript data model provides a single global scope
 * for all variables. Variables defined in a child state must be accessible
 * from parent states, and variables in parallel sibling states must be
 * accessible from each other.
 *
 * Test validates:
 * - Parent state (s0) can access variable (var1) defined in child state (s01)
 * - Parallel sibling state (s01p1) can access variable (var2) defined in
 *   another parallel sibling (s01p2)
 */
struct Test448 : public SimpleAotTest<Test448, 448> {
    static constexpr const char *DESCRIPTION = "ECMAScript single global scope (W3C B.2 AOT)";
    using SM = RSM::Generated::test448::test448;
};

// Auto-register
inline static AotTestRegistrar<Test448> registrar_Test448;

}  // namespace RSM::W3C::AotTests
