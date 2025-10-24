#pragma once
#include "SimpleAotTest.h"
#include "test453_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML B.2/3.12.1: ECMAScript function expression evaluation
 *
 * Tests that any ECMAScript expression can be used as a value expression,
 * specifically validating function expressions assigned to datamodel variables
 * and subsequent function invocation in guard conditions.
 *
 * W3C SCXML B.2: The ECMAScript data model supports all ECMAScript expressions,
 * including function expressions (anonymous functions) that can be assigned to
 * variables and invoked later.
 *
 * W3C SCXML 3.12.1: Any valid ECMAScript expression can be used as a value
 * expression in the expr attribute, including function definitions.
 *
 * W3C SCXML 5.9: Conditional expressions (cond attribute) support any valid
 * ECMAScript expression, including function calls with parameters.
 *
 * Test validates:
 * - Function expression assignment: var1 = function(invar) {return invar + 1;}
 * - Function invocation in guard: var1(2) == 3 (evaluates 2+1 == 3)
 * - Closure semantics: function retains access to parameter scope
 * - First-class functions: functions as values in ECMAScript datamodel
 *
 * Implementation:
 * - Uses Static Hybrid approach (static state machine + JSEngine evaluation)
 * - JSEngine evaluates function expression during datamodel initialization
 * - Guard "var1(2) == 3" evaluated via safeEvaluateGuard()
 * - ARCHITECTURE.md Zero Duplication: Follows established Helper pattern
 *   (GuardHelper) for Single Source of Truth in guard evaluation
 * - Function stored as JSEngine value, callable across state machine execution
 */
struct Test453 : public SimpleAotTest<Test453, 453> {
    static constexpr const char *DESCRIPTION = "ECMAScript function expression evaluation (W3C B.2/3.12.1 AOT)";
    using SM = RSM::Generated::test453::test453;
};

// Auto-register
inline static AotTestRegistrar<Test453> registrar_Test453;

}  // namespace RSM::W3C::AotTests
