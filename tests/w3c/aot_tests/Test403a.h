#pragma once

#include "SimpleAotTest.h"
#include "test403a_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.13: Optimal enabled transition set selection (basic case)
 *
 * Tests that the SCXML Processor executes transitions in the optimal enabled
 * transition set, where the optimal set is the largest set of non-conflicting
 * transitions enabled by an event in the current configuration. This variant
 * tests the basic case without parallel states.
 *
 * Test Structure:
 * - Hierarchical states with multiple transitions enabled by same event
 * - Tests transition priority based on document order
 * - Tests that higher-priority transitions prevent lower-priority ones
 * - W3C SCXML 3.13: Optimal set selection with priority ordering
 *
 * Expected: Only highest-priority non-conflicting transitions execute.
 */
struct Test403a : public SimpleAotTest<Test403a, 403> {
    static constexpr const char *DESCRIPTION = "Optimal enabled transition set (W3C 3.13 basic AOT)";
    using SM = RSM::Generated::test403a::test403a;
};

// Auto-register
inline static AotTestRegistrar<Test403a> registrar_Test403a;

}  // namespace RSM::W3C::AotTests
