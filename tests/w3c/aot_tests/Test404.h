#pragma once

#include "SimpleAotTest.h"
#include "test404_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.13: Exit order and transition execution
 *
 * Validates that states are exited in exit order (children before parents, with
 * reverse document order used to break ties) before executing transition content.
 * Tests parallel state exit order where events are raised in specific sequence:
 * event1 (s01p2 onexit) → event2 (s01p1 onexit) → event3 (s01p onexit) → event4 (transition).
 *
 * Test Structure:
 * - Parallel state `s01p` with two child states `s01p1` and `s01p2`
 * - Each state's onexit action raises a numbered event
 * - Exit order follows W3C SCXML 3.13: children first (reverse document order), then parent
 * - Transition content raises event4 after all exits complete
 *
 * Expected: Events raised in correct order (event1, event2, event3, event4).
 * W3C SCXML 3.13: Parallel state exit order with document order tie-breaking.
 */
struct Test404 : public SimpleAotTest<Test404, 404> {
    static constexpr const char *DESCRIPTION = "Parallel state exit order (W3C 3.13 AOT)";
    using SM = RSM::Generated::test404::test404;
};

// Auto-register
inline static AotTestRegistrar<Test404> registrar_Test404;

}  // namespace RSM::W3C::AotTests
