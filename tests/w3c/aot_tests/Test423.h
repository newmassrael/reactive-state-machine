#pragma once
#include "SimpleAotTest.h"
#include "test423_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.9.2: External event queue processing with internal event priority
 *
 * Validates that:
 * 1. Internal events (raise) take priority over external events (send)
 * 2. External events are dequeued sequentially until a matching transition is found
 * 3. Non-matching external events are discarded during transition selection
 *
 * Test scenario:
 * - s0 sends externalEvent1 (immediate), externalEvent2 (1s delay), raises internalEvent
 * - s0 transitions on internalEvent (not externalEvent1) to s1
 * - s1 ignores externalEvent1, transitions on externalEvent2 to pass
 *
 * Uses ScheduledAotTest for 1-second delay event polling (W3C SCXML 6.2)
 */
struct Test423 : public ScheduledAotTest<Test423, 423> {
    static constexpr const char *DESCRIPTION = "External event queue processing (W3C 5.9.2 AOT)";
    using SM = RSM::Generated::test423::test423;
};

// Auto-register
inline static AotTestRegistrar<Test423> registrar_Test423;

}  // namespace RSM::W3C::AotTests
