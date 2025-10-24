#pragma once
#include "SimpleAotTest.h"
#include "test416_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.3.2: done.state.id event generation for compound states
 *
 * Tests that when a compound state's final child state is entered,
 * the platform automatically generates a done.state.id event.
 *
 * Test structure:
 * - State s1 (compound) has child state s11 (compound)
 * - State s11 has final state s11final
 * - When s11final is entered, done.state.s11 event is generated
 * - Transition on done.state.s11 moves to pass state
 *
 * W3C SCXML 3.3.2: "When the state machine enters the final child of a
 * compound state, the platform must generate a done.state.id event where
 * id is the id of the compound state."
 */
struct Test416 : public SimpleAotTest<Test416, 416> {
    static constexpr const char *DESCRIPTION = "done.state.id event generation (W3C 3.3.2 AOT)";
    using SM = RSM::Generated::test416::test416;
};

// Auto-register
inline static AotTestRegistrar<Test416> registrar_Test416;

}  // namespace RSM::W3C::AotTests
