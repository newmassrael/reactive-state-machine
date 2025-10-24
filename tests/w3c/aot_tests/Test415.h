#pragma once
#include "SimpleAotTest.h"
#include "test415_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.7.1: Top-level final state halts execution
 *
 * This manual test verifies that entering a top-level final state
 * immediately halts the state machine before processing any raised events.
 *
 * Test structure:
 * - Initial state: final (top-level final state)
 * - Entry action: raise "event1"
 * - Expected: State machine halts at Final, event1 is NOT processed
 *
 * W3C SCXML 3.7.1: "When a state machine enters a top-level final state,
 * it must halt execution and may not process any further events."
 */
struct Test415 : public SimpleAotTest<Test415, 415> {
    static constexpr const char *DESCRIPTION = "Top-level final state halts execution (W3C 3.7.1 AOT)";
    using SM = RSM::Generated::test415::test415;

    // Policy-based design: Override success state for manual test
    // This test has no Pass state, only Final state
    static constexpr auto PASS_STATE = SM::State::Final;
};

// Auto-register
inline static AotTestRegistrar<Test415> registrar_Test415;

}  // namespace RSM::W3C::AotTests
