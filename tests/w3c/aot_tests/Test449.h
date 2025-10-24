#pragma once
#include "SimpleAotTest.h"
#include "test449_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML B.2: ECMAScript boolean conversion for string literals
 *
 * Tests that ECMAScript non-empty string literals are converted to true
 * in boolean context (conditional expressions).
 *
 * W3C SCXML B.2: The ECMAScript data model follows ECMAScript semantics
 * for type conversion. Non-empty strings convert to true, empty strings
 * convert to false.
 *
 * W3C SCXML 5.9.2: Conditional expressions (cond attribute) are evaluated
 * in the datamodel context, following datamodel-specific conversion rules.
 *
 * Test validates:
 * - String literal 'foo' (non-empty) evaluates to true in cond expression
 * - Transition with cond="'foo'" successfully triggers
 * - ECMAScript boolean conversion semantics are properly implemented
 *
 * Implementation:
 * - Uses Static Hybrid approach (static state machine + JSEngine evaluation)
 * - JSEngine evaluates "'foo'" according to ECMAScript semantics
 * - Ensures W3C SCXML B.2 compliance for boolean conversion
 */
struct Test449 : public SimpleAotTest<Test449, 449> {
    static constexpr const char *DESCRIPTION = "ECMAScript boolean conversion for string literals (W3C B.2 AOT)";
    using SM = RSM::Generated::test449::test449;
};

// Auto-register
inline static AotTestRegistrar<Test449> registrar_Test449;

}  // namespace RSM::W3C::AotTests
