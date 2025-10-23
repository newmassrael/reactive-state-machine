#pragma once
#include "SimpleAotTest.h"
#include "test409_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.12.1: Active State Configuration - State Removal During Exit
 *
 * Tests that states are correctly removed from the active states list as they are exited.
 * When s01's onexit handler executes during transition, its child state s011 should no longer
 * be in the active state list, making In('s011') return false. This validates proper state
 * exit ordering according to W3C SCXML 3.12.1.
 *
 * Test Flow:
 * 1. Enter s0 → s01 → s011 (all active)
 * 2. Transition from s011 to s02 triggers
 * 3. Exit s011 first (removed from active states)
 * 4. Exit s01 (onexit handler runs)
 * 5. In('s011') should be false (s011 already exited)
 * 6. event1 should NOT be raised
 * 7. Timeout fires → pass
 *
 * Requires event scheduler polling for delayed send (1s timeout).
 */
struct Test409 : public ScheduledAotTest<Test409, 409> {
    static constexpr const char *DESCRIPTION = "State removal from active states during exit (W3C 3.12.1 AOT)";
    using SM = RSM::Generated::test409::test409;
};

// Auto-register
inline static AotTestRegistrar<Test409> registrar_Test409;

}  // namespace RSM::W3C::AotTests
