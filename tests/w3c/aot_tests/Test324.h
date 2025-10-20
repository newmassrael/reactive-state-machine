#pragma once
#include "SimpleAotTest.h"
#include "test324_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML B.2.2: System variable _name immutability
 *
 * Tests that the _name system variable cannot be reassigned after initialization.
 * The test verifies that _name remains bound to "machineName" throughout the session,
 * even after explicit assignment attempts. System variables are immutable per W3C spec.
 *
 * W3C SCXML B.2.2: System variables such as '_name', '_sessionid', and '_event'
 * are immutable and cannot be reassigned by <assign> or other executable content.
 * Attempts to modify them should not change their values.
 */
struct Test324 : public SimpleAotTest<Test324, 324> {
    static constexpr const char *DESCRIPTION = "_name immutability (W3C B.2.2 AOT)";
    using SM = RSM::Generated::test324::test324;
};

// Auto-register
inline static AotTestRegistrar<Test324> registrar_Test324;

}  // namespace RSM::W3C::AotTests
