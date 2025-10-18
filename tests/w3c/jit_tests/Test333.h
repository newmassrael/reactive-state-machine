#pragma once
#include "SimpleJitTest.h"
#include "test333_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 5.10.1: _event.sendid field must be blank for non-error events
 *
 * Validates that when a <send> element does NOT specify an id attribute and is NOT an error event,
 * the _event.sendid field is left blank (empty/undefined) as required by the specification.
 *
 * This is the inverse of test 332:
 * - Test 332: Validates _event.sendid IS populated in error events
 * - Test 333: Validates _event.sendid IS BLANK in normal events without explicit sendid
 *
 * The test sends a "foo" event without specifying a sendid, then verifies the event
 * is received with an empty sendid field.
 */
struct Test333 : public SimpleJitTest<Test333, 333> {
    static constexpr const char *DESCRIPTION = "_event.sendid blank for non-error events (W3C 5.10.1 JIT)";
    using SM = RSM::Generated::test333::test333;
};

// Auto-register
inline static JitTestRegistrar<Test333> registrar_Test333;

}  // namespace RSM::W3C::JitTests
