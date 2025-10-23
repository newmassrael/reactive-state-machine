#pragma once
#include "SimpleAotTest.h"
#include "test412_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.3.2: Initial Transition Executable Content Execution Order
 *
 * Tests that executable content in the <initial> transition executes after the parent state's
 * onentry handler and before the child state's onentry handler. This validates the proper timing
 * of initial transition executable content according to W3C SCXML 3.3.2.
 *
 * Test Flow:
 * 1. Enter s0 (onentry: send timeout event)
 * 2. Enter s01 (onentry: raise event1)
 * 3. Execute <initial> transition executable content (raise event2)
 * 4. Enter s011 (onentry: raise event3)
 * 5. Transition to s02 (event queue: event1, event2, event3)
 * 6. s02 processes event1 first → transition to s03
 * 7. If event2 processed first → fail (incorrect execution order)
 * 8. Timeout fires → fail (no events processed)
 *
 * Expected order: event1 (s01 onentry) → event2 (initial transition) → event3 (s011 onentry)
 * Correct behavior: event1 processed first → pass
 *
 * Requires event scheduler polling for delayed send (1s timeout).
 */
struct Test412 : public ScheduledAotTest<Test412, 412> {
    static constexpr const char *DESCRIPTION = "Initial transition executable content execution order (W3C 3.3.2 AOT)";
    using SM = RSM::Generated::test412::test412;
};

// Auto-register
inline static AotTestRegistrar<Test412> registrar_Test412;

}  // namespace RSM::W3C::AotTests
