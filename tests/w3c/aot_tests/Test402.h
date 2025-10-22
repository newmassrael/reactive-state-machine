#pragma once

#include "SimpleAotTest.h"
#include "test402_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.12: Error events processed like any other event
 *
 * Validates that error events (specifically error.execution from invalid assignment)
 * are pulled off the internal queue in order and can be caught with event="error"
 * or prefix matching. Tests error event ordering: event1 → error.execution → event2.
 *
 * Test Structure:
 * - State s01: Raises event1, intentionally triggers error.execution (empty location),
 *   then raises event2
 * - Transition to s02 on event1 (consumed first)
 * - Transition to s03 on error (catches error.execution via prefix matching)
 * - Transition to pass on event2 (consumed last)
 *
 * Expected: All events processed in order, reaching pass state.
 * W3C SCXML 3.12: Error events follow standard event processing rules.
 */
struct Test402 : public SimpleAotTest<Test402, 402> {
    static constexpr const char *DESCRIPTION = "Error event ordering and prefix matching (W3C 3.12 AOT)";
    using SM = RSM::Generated::test402::test402;
};

// Auto-register
inline static AotTestRegistrar<Test402> registrar_Test402;

}  // namespace RSM::W3C::AotTests
