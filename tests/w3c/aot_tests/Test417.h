#pragma once
#include "SimpleAotTest.h"
#include "test417_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.7.1: done.state Event for Parallel States
 *
 * Tests that done.state.id event is generated when all children of a
 * parallel element enter final states. The parallel state s1p1 has two
 * child regions (s1p11, s1p12), each with a nested state leading to a
 * final state. When both regions complete, done.state.s1p1 should trigger
 * transition to pass state.
 *
 * Test structure:
 * - State s1 (compound) has parallel child s1p1
 * - Parallel s1p1 has two regions: s1p11 and s1p12
 * - Region s1p11: s1p111 -> s1p11final (final)
 * - Region s1p12: s1p121 -> s1p12final (final)
 * - When both regions enter final states, done.state.s1p1 event is generated
 * - Transition on done.state.s1p1 moves from s1 to pass state
 *
 * W3C SCXML Requirements:
 * - 3.4: Parallel state with multiple child regions
 * - 3.7.1: Automatic done.state event generation for parallel completion
 * - 3.8.1: Final state handling within parallel regions
 */
struct Test417 : public SimpleAotTest<Test417, 417> {
    static constexpr const char *DESCRIPTION = "Parallel done.state event (W3C 3.7.1 AOT)";
    using SM = RSM::Generated::test417::test417;
};

// Auto-register
inline static AotTestRegistrar<Test417> registrar_Test417;

}  // namespace RSM::W3C::AotTests
