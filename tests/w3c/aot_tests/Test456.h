#pragma once
#include "SimpleAotTest.h"
#include "test456_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML B.2/5.9: ECMAScript script element execution
 *
 * Validates that the SCXML processor can execute arbitrary ECMAScript code
 * within <script> elements and update the data model accordingly.
 *
 * W3C SCXML B.2: The ECMAScript data model supports execution of arbitrary
 * JavaScript code through the <script> element, with full access to the
 * data model for reading and modifying variables.
 *
 * W3C SCXML 5.9: The <script> element contains ECMAScript code that is
 * executed when the SCXML processor processes the containing executable
 * content block (e.g., <onentry>, <onexit>, <transition>).
 *
 * Test validates:
 * - Variable initialization: Var1 = 0
 * - Script execution in <onentry>: Var1+=1 (increment operation)
 * - Guard evaluation: Var1 == 1 (verifies variable was updated)
 * - ECMAScript data model mutation through script elements
 *
 * Implementation:
 * - Uses Static Hybrid approach (static state machine + JSEngine evaluation)
 * - JSEngine executes script: "Var1+=1" via executeScript()
 * - Guard "Var1 == 1" evaluated via safeEvaluateGuard()
 * - ARCHITECTURE.md Zero Duplication: Follows GuardHelper pattern
 * - Script content executed in JSEngine session context
 *
 * Test flow:
 * 1. Enter s0 state
 * 2. Execute onentry script: Var1+=1
 * 3. Raise event1
 * 4. Check guard: Var1 == 1
 * 5. Transition to pass if true, fail if false
 */
struct Test456 : public SimpleAotTest<Test456, 456> {
    static constexpr const char *DESCRIPTION = "ECMAScript script execution (W3C B.2/5.9 AOT)";
    using SM = RSM::Generated::test456::test456;
};

// Auto-register
inline static AotTestRegistrar<Test456> registrar_Test456;

}  // namespace RSM::W3C::AotTests
