#pragma once
#include "SimpleAotTest.h"
#include "test419_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.13: Eventless Transition Precedence
 *
 * Tests that eventless transitions have priority over event-driven transitions.
 * The state s1 entry action raises both internal and external events, but an
 * eventless transition from s1 to pass should be taken immediately before any
 * events are processed from the internal or external event queues.
 *
 * Test structure:
 * - Initial state s1 has entry action that raises "internalEvent" and "externalEvent"
 * - s1 has three outgoing transitions:
 *   1. Eventless transition to pass (should be taken immediately)
 *   2. Event transition on "internalEvent" to fail
 *   3. Event transition on "*" (wildcard) to fail
 * - If eventless transition is correctly prioritized, machine reaches pass state
 * - If events are processed first, machine incorrectly reaches fail state
 *
 * W3C SCXML Requirements:
 * - 3.13: Eventless transitions are evaluated before events are dequeued
 * - 3.12.2: Internal event queue processing occurs only after eventless transitions
 * - 3.12.1: External event queue processing occurs only after internal queue exhausted
 */
struct Test419 : public SimpleAotTest<Test419, 419> {
    static constexpr const char *DESCRIPTION = "Eventless transition precedence (W3C 3.13 AOT)";
    using SM = RSM::Generated::test419::test419;
};

// Auto-register
inline static AotTestRegistrar<Test419> registrar_Test419;

}  // namespace RSM::W3C::AotTests
