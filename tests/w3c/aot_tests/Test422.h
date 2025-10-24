#pragma once
#include "SimpleAotTest.h"
#include "test422_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 6.4: Invoke Execution at Macrostep End
 *
 * Tests that invoke elements in states that are entered but not exited during
 * a macrostep are executed at the end of that macrostep. The test validates
 * that only invokes in states that remain active after all transitions are
 * processed will actually execute.
 *
 * Test structure:
 * - State s1 has invoke with static inline SCXML child (<invoke><content>)
 * - s1 contains compound state s11 which transitions to s12
 * - s11 has invoke but immediately exits (invoke should NOT execute)
 * - s12 has invoke and remains active (invoke SHOULD execute)
 * - Variable Var1 tracks invoke executions:
 *   - s1 invoke increments Var1 (executed, entered and not exited)
 *   - s11 invoke does not increment (not executed, entered then immediately exited)
 *   - s12 invoke increments Var1 (executed, entered and not exited)
 * - Final Var1 == 2 means correct invoke timing (s1 + s12, not s11)
 *
 * W3C SCXML Requirements:
 * - 6.4: Invoke elements are processed at macrostep end
 * - 6.4: Only invokes in entered-and-not-exited states execute
 * - 3.3: Macrostep completes before invoke processing begins
 */
struct Test422 : public ScheduledAotTest<Test422, 422> {
    static constexpr const char *DESCRIPTION = "Invoke at macrostep end (W3C 6.4 AOT)";
    using SM = RSM::Generated::test422::test422;
};

// Auto-register
inline static AotTestRegistrar<Test422> registrar_Test422;

}  // namespace RSM::W3C::AotTests
