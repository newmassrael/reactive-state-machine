#pragma once
#include "SimpleJitTest.h"
#include "test336_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 5.10.1: _event.origin field bidirectional communication
 *
 * Validates that the _event.origin field contains a URL that enables sending responses
 * back to the event originator. Tests bidirectional event communication by:
 * 1. Sending event "foo" to itself
 * 2. Receiving "foo" with populated _event.origin
 * 3. Using targetexpr="_event.origin" to send "bar" back
 * 4. Verifying successful round-trip communication
 *
 * This test validates ECMAScript expression evaluation in targetexpr attributes,
 * requiring Static Hybrid code generation with JSEngine integration.
 *
 * Related tests:
 * - Test 335: Validates _event.origin is blank for internal events
 * - Test 336: Validates _event.origin enables bidirectional communication
 * - Test 337: Validates _event.origintype field classification
 */
struct Test336 : public SimpleJitTest<Test336, 336> {
    static constexpr const char *DESCRIPTION = "_event.origin bidirectional communication (W3C 5.10.1 JIT)";
    using SM = RSM::Generated::test336::test336;
};

// Auto-register
inline static JitTestRegistrar<Test336> registrar_Test336;

}  // namespace RSM::W3C::JitTests
