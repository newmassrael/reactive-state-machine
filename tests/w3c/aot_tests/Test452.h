#pragma once
#include "SimpleAotTest.h"
#include "test452_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.3/5.4: Datamodel substructure assignment (object properties)
 *
 * Tests that assignments can target substructures within the datamodel,
 * specifically validating assignment to object properties rather than
 * only top-level variables.
 *
 * W3C SCXML 5.3: The assign element updates the value of a data location
 * in the datamodel. The location attribute specifies the data location,
 * which can be a substructure like "foo.bar".
 *
 * W3C SCXML 5.4: ECMAScript datamodel supports object creation via
 * constructor functions and property access/assignment.
 *
 * W3C SCXML 5.9: ECMAScript expressions in guards evaluate property
 * access (e.g., "foo.bar == 1") with proper JavaScript semantics.
 *
 * Test validates:
 * - JavaScript constructor function definition via <script> element
 * - Object creation with "new testobject()" expression
 * - Property assignment to foo.bar location (substructure assignment)
 * - Guard evaluation with property access expression
 * - ECMAScript datamodel structure manipulation
 *
 * Implementation:
 * - Uses Static Hybrid approach (static state machine + JSEngine evaluation)
 * - JSEngine evaluates "new testobject()" constructor call
 * - JSEngine executes "foo.bar = 1" property assignment
 * - Guard "foo.bar == 1" evaluated via safeEvaluateGuard()
 * - ARCHITECTURE.md Zero Duplication: Follows established Helper pattern
 *   (GuardHelper) for Single Source of Truth in guard evaluation
 * - Script content loaded into JSEngine session context
 */
struct Test452 : public SimpleAotTest<Test452, 452> {
    static constexpr const char *DESCRIPTION = "Datamodel substructure assignment (W3C 5.3/5.4 AOT)";
    using SM = RSM::Generated::test452::test452;
};

// Auto-register
inline static AotTestRegistrar<Test452> registrar_Test452;

}  // namespace RSM::W3C::AotTests
