#pragma once
#include "SimpleAotTest.h"
#include "test445_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML B.2.2: ECMAScript undefined variable behavior
 *
 * Tests that ECMAScript variables defined by <data> elements without initialization
 * have the value undefined, following JavaScript undefined semantics.
 * Validates that condition expressions can correctly compare against undefined.
 *
 * W3C SCXML B.2.2: ECMAScript Data Model - undefined variables
 * W3C SCXML 5.9: Conditional Expressions - transition condition evaluation
 */
struct Test445 : public SimpleAotTest<Test445, 445> {
    static constexpr const char *DESCRIPTION = "ECMAScript undefined variables (W3C B.2.2 AOT)";
    using SM = RSM::Generated::test445::test445;
};

// Auto-register
inline static AotTestRegistrar<Test445> registrar_Test445;

}  // namespace RSM::W3C::AotTests
