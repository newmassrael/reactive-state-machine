#pragma once
#include "SimpleAotTest.h"
#include "test347_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 6.4: SCXML Event I/O processor parent-child communication
 *
 * Tests bidirectional communication between parent and child SCXML processes using
 * the SCXML Event I/O processor (http://www.w3.org/TR/scxml/#SCXMLEventProcessor).
 *
 * This test verifies:
 * - Child SCXML invocation with inline content (static invoke)
 * - Child-to-parent event communication using target="#_parent"
 * - Parent-to-child event communication using target="#_child"
 * - done.invoke event upon child session completion
 * - Timeout handling for communication failures
 *
 * Uses static code generation for both parent and child state machines.
 */
struct Test347 : public SimpleAotTest<Test347, 347> {
    static constexpr const char *DESCRIPTION = "SCXML Event I/O processor communication (W3C 6.4 AOT)";
    using SM = RSM::Generated::test347::test347;
};

// Auto-register
inline static AotTestRegistrar<Test347> registrar_Test347;

}  // namespace RSM::W3C::AotTests
