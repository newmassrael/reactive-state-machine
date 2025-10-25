#pragma once
#include "SimpleAotTest.h"
#include "test487_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.3/5.4: Illegal assignment error.execution event
 *
 * Tests that attempting to access a property on undefined raises error.execution event.
 * The test initializes with an assignment expression that tries to access
 * undefined.invalidProperty, which should trigger error.execution.
 *
 * Expected behavior:
 * - Assignment expression: Var1 = undefined.invalidProperty
 * - Runtime evaluation via JSEngine detects illegal access
 * - error.execution event raised and caught by transition
 * - Transitions to pass state upon receiving error.execution
 *
 * Uses Static Hybrid approach: static state machine structure with
 * runtime ECMAScript expression evaluation via JSEngine.
 */
struct Test487 : public SimpleAotTest<Test487, 487> {
    static constexpr const char *DESCRIPTION = "Illegal assignment (W3C 5.3/5.4 AOT)";
    using SM = RSM::Generated::test487::test487;
};

// Auto-register
inline static AotTestRegistrar<Test487> registrar_Test487;

}  // namespace RSM::W3C::AotTests
