#pragma once
#include "SimpleAotTest.h"
#include "test503_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.13: Targetless transitions do not exit/re-enter source state
 *
 * Validates that transitions without a 'target' attribute have an empty exit set,
 * meaning they do not cause the source state to be exited and re-entered.
 *
 * This test verifies correct W3C SCXML 3.13 semantics by using a targetless
 * transition in state s2 that increments a counter variable (Var2) without
 * exiting s2. The test tracks the number of times s2's onexit handler executes
 * via Var1 - it should be exactly 1 (when leaving s2 to go to s3), not 2
 * (which would indicate the targetless transition incorrectly exited/re-entered s2).
 *
 * Expected behavior:
 * - State s1 raises events 'foo' and 'bar', then transitions to s2
 * - State s2 has targetless transition on 'foo' that increments Var2 (no target attribute)
 * - Targetless transition executes without exiting s2 (Var1 remains 0)
 * - Event 'bar' causes transition from s2 to s3 (Var1 incremented to 1 in onexit)
 * - State s3 validates Var1 == 1 and Var2 == 1, then transitions to pass
 *
 * Uses Static Hybrid approach: static state machine structure with
 * runtime ECMAScript expression evaluation via JSEngine for variable operations.
 */
struct Test503 : public SimpleAotTest<Test503, 503> {
    static constexpr const char *DESCRIPTION =
        "Targetless transitions do not exit/re-enter (W3C 3.13 AOT Static Hybrid)";
    using SM = RSM::Generated::test503::test503;
};

// Auto-register
inline static AotTestRegistrar<Test503> registrar_Test503;

}  // namespace RSM::W3C::AotTests
