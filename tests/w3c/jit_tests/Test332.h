#pragma once
#include "SimpleJitTest.h"
#include "test332_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 5.10.1: _event.sendid field binding in error events
 *
 * Validates that when a <send> element with an invalid target triggers an error.execution event,
 * the _event.sendid field is correctly populated with the send ID.
 * Also tests W3C SCXML 6.2.4 send idlocation attribute for storing the send ID in a variable.
 *
 * The test uses:
 * - <send target="!invalid"> to trigger error.execution
 * - idlocation="Var1" to store the send ID
 * - <assign location="Var2" expr="_event.sendid"/> to capture sendid from error event
 * - Condition to verify Var1 === Var2 (both contain the same send ID)
 */
struct Test332 : public SimpleJitTest<Test332, 332> {
    static constexpr const char *DESCRIPTION = "_event.sendid in error events (W3C 5.10.1, 6.2.4 JIT)";
    using SM = RSM::Generated::test332::test332;
};

// Auto-register
inline static JitTestRegistrar<Test332> registrar_Test332;

}  // namespace RSM::W3C::JitTests
