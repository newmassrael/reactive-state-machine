#pragma once
#include "SimpleAotTest.h"
#include "test311_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.9.2: error.execution for invalid assignment location
 *
 * Tests that assignment to a non-existent (empty) location yields an
 * error.execution event. The test uses <assign location="" expr="1"/>
 * to trigger the error condition.
 *
 * Expected behavior:
 * - Enter s0 state
 * - Attempt assign to empty location ""
 * - Raise error.execution event
 * - Transition to pass state
 */
struct Test311 : public SimpleAotTest<Test311, 311> {
    static constexpr const char *DESCRIPTION = "error.execution for invalid assign location (W3C 5.9.2 AOT)";
    using SM = RSM::Generated::test311::test311;
};

// Auto-register
inline static AotTestRegistrar<Test311> registrar_Test311;

}  // namespace RSM::W3C::AotTests
