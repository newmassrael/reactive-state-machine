#pragma once
#include "SimpleAotTest.h"
#include "test407_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.8: onexit handlers with datamodel variable updates
 *
 * Tests that onexit handlers execute properly and can update datamodel variables.
 * Validates that when exiting a state, the onexit handler executes and increments
 * a variable (Var1), and the updated value is visible in subsequent conditional
 * transitions. Ensures event order: s0 exit (Var1=0→1) → condition check (Var1==1) → pass.
 *
 * This test also validates the critical fix for lastTransitionSourceState_ tracking
 * (2025-10-23): AOT engine now correctly tracks transition source states for all
 * transitions (not just those with actions), ensuring hierarchical exit/entry uses
 * the actual transitioning state instead of defaulting to incorrect states. Without
 * this fix, onexit handlers would not execute because the wrong state was being exited.
 *
 * W3C SCXML 3.4: Hierarchical state transitions require accurate source state tracking
 * for proper entry/exit action execution in compound and parallel state machines.
 */
struct Test407 : public SimpleAotTest<Test407, 407> {
    static constexpr const char *DESCRIPTION = "onexit handlers (W3C 3.8 AOT)";
    using SM = RSM::Generated::test407::test407;
};

// Auto-register
inline static AotTestRegistrar<Test407> registrar_Test407;

}  // namespace RSM::W3C::AotTests
