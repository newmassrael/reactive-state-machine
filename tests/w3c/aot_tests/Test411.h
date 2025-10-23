#pragma once
#include "SimpleAotTest.h"
#include "test411_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.12.1: Active State Configuration - State Addition During Entry
 *
 * Tests that states are correctly added to the active states list before their onentry handlers execute.
 * When s01's onentry handler runs during state entry, s01 should already be in the active state list,
 * making In('s01') return true. This validates proper state addition timing according to W3C SCXML 3.12.1.
 *
 * Test Flow:
 * 1. Enter s0 (onentry: In('s01') should be false - s01 not yet entered)
 * 2. Enter s01 (onentry: In('s01') should be true - s01 already in active states)
 * 3. If both conditions hold, no event1 is raised
 * 4. Timeout fires → pass
 * 5. If either In() check fails, event1 raised → fail
 *
 * Requires event scheduler polling for delayed send (1s timeout).
 */
struct Test411 : public ScheduledAotTest<Test411, 411> {
    static constexpr const char *DESCRIPTION = "State addition to active states during entry (W3C 3.12.1 AOT)";
    using SM = RSM::Generated::test411::test411;
};

// Auto-register
inline static AotTestRegistrar<Test411> registrar_Test411;

}  // namespace RSM::W3C::AotTests
