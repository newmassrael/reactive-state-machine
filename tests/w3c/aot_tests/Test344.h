#pragma once
#include "SimpleAotTest.h"
#include "test344_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.9: Invalid cond expression raises error.execution
 *
 * Tests that a cond expression that cannot be evaluated as a boolean value
 * causes error.execution to be raised. The test uses cond="return" which is
 * invalid ECMAScript syntax (standalone return keyword).
 *
 * Per W3C SCXML 5.9: "If a conditional expression cannot be evaluated as a
 * boolean value ('true' or 'false') or if its evaluation causes an error,
 * the SCXML processor MUST place the error 'error.execution' in the internal
 * event queue."
 *
 * Test flow:
 * 1. S0 has eventless transition with cond="return" → fail (should not execute)
 * 2. S0 has eventless transition without cond → s1 (executes after cond fails)
 * 3. S1 raises event "foo"
 * 4. error.execution raised by JSEngine when evaluating cond="return"
 * 5. S1 catches error.execution → transitions to pass
 */
struct Test344 : public SimpleAotTest<Test344, 344> {
    static constexpr const char *DESCRIPTION = "Invalid cond expression error.execution (W3C 5.9 AOT)";
    using SM = RSM::Generated::test344::test344;
};

// Auto-register
inline static AotTestRegistrar<Test344> registrar_Test344;

}  // namespace RSM::W3C::AotTests
