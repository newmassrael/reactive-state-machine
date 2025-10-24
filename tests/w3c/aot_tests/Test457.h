#pragma once
#include "SimpleAotTest.h"
#include "test457_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.4/D.3.1: foreach error handling with ECMAScript validation
 *
 * Validates comprehensive error handling for foreach loops, including invalid array
 * types, illegal item identifiers (reserved keywords), and correct iteration behavior.
 *
 * W3C SCXML 5.4: The foreach element allows iteration over arrays and collections
 * with strict validation requirements. If the array attribute does not evaluate to
 * an iterable collection, or if the item attribute contains an illegal identifier,
 * the SCXML Processor MUST raise error.execution and skip the foreach body.
 *
 * W3C SCXML D.3.1: error.execution events are raised when executable content fails
 * to execute properly, including foreach validation failures (invalid array, illegal
 * item names such as reserved keywords).
 *
 * Test validates three scenarios:
 * 1. Invalid array type: foreach with non-array value (integer 7) raises error.execution
 * 2. Illegal item identifier: foreach with reserved keyword 'continue' raises error.execution
 * 3. Valid foreach: Correct iteration over [1,2,3] array with proper accumulation
 *
 * Implementation:
 * - Uses Static Hybrid approach (static state machine + JSEngine for ECMAScript)
 * - ForeachHelper validates array type and item identifier (W3C SCXML 5.4)
 * - Reserved keyword detection: 'continue', 'break', 'if', 'while', 'for', etc.
 * - Error cases: foreach body NOT executed, error.execution event raised
 * - Success case: Accumulator correctly sums array elements (1+2+3 = 6)
 *
 * Test flow:
 * 1. s0: Invalid array (7) → error.execution → s1
 * 2. s1: Illegal item ('continue') → error.execution → s2
 * 3. s2: Valid foreach [1,2,3] → accumulator = 6 → pass
 * 4. Any validation failure in s2 → fail state
 */
struct Test457 : public SimpleAotTest<Test457, 457> {
    static constexpr const char *DESCRIPTION = "foreach error handling (W3C 5.4/D.3.1 AOT)";
    using SM = RSM::Generated::test457::test457;
};

// Auto-register
inline static AotTestRegistrar<Test457> registrar_Test457;

}  // namespace RSM::W3C::AotTests
