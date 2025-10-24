#pragma once
#include "SimpleAotTest.h"
#include "test444_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.2/5.9: ECMAScript datamodel variable creation and condition evaluation
 *
 * Tests that <data> elements create ECMAScript variables accessible in condition expressions.
 * Validates pre-increment operator (++var1) side effects and equality evaluation (==2).
 *
 * W3C SCXML 5.2: Data Model - <data> creates variables
 * W3C SCXML 5.9: Conditional Expressions - ECMAScript evaluation
 * W3C SCXML B.2: ECMAScript Data Model - variable semantics
 */
struct Test444 : public SimpleAotTest<Test444, 444> {
    static constexpr const char *DESCRIPTION = "ECMAScript datamodel variable in condition (W3C 5.2/5.9 AOT)";
    using SM = RSM::Generated::test444::test444;
};

// Auto-register
inline static AotTestRegistrar<Test444> registrar_Test444;

}  // namespace RSM::W3C::AotTests
