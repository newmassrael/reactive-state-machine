#pragma once
#include "SimpleAotTest.h"
#include "test421_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10.1: Internal Event Priority Over External Events
 *
 * Tests that internal events (raised via <raise>) have priority over external
 * events (sent via <send>) in the event processing queue. The state machine
 * processes internal events first, and only when the internal queue is empty
 * does it process external events.
 *
 * Test structure:
 * - State s1 with nested compound state structure (s11, s12)
 * - s11 entry action sends externalEvent and raises internalEvent1-4
 * - Transitions from s11:
 *   - internalEvent1 → s11 (stays in s11, raises more internal events)
 *   - internalEvent2 → s11 (stays in s11, raises more internal events)
 *   - internalEvent3 → s12 (moves to s12)
 *   - externalEvent → fail (should never trigger if internal events processed first)
 * - s12 entry action verifies internal events were processed before external
 * - If internalEvent3 triggers before externalEvent, machine reaches pass state
 * - If externalEvent is processed before internal queue exhausted, machine reaches fail
 *
 * W3C SCXML Requirements:
 * - 5.10.1: Internal events have priority over external events
 * - 5.10: Internal event queue is processed before external event queue
 * - 3.13: Events are processed in order, respecting queue priorities
 */
struct Test421 : public SimpleAotTest<Test421, 421> {
    static constexpr const char *DESCRIPTION = "Internal event priority (W3C 5.10.1 AOT)";
    using SM = RSM::Generated::test421::test421;
};

// Auto-register
inline static AotTestRegistrar<Test421> registrar_Test421;

}  // namespace RSM::W3C::AotTests
