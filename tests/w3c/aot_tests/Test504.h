#pragma once
#include "SimpleAotTest.h"
#include "test504_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.13: External transition exit sets (LCCA)
 *
 * Validates that an external transition exits all active states that are
 * proper descendants of the Least Common Compound Ancestor (LCCA) of the
 * source and target states.
 *
 * This test verifies correct W3C SCXML 3.13 exit set calculation for external
 * transitions in parallel state configurations. The test uses counters to track
 * which states' onexit handlers are executed during transitions between parallel
 * regions, ensuring that the proper exit set is computed based on LCCA.
 *
 * Expected behavior:
 * - State s1 transitions to parallel state p, entering both ps1 and ps2 regions
 * - External transition from ps1 to ps2 (event "foo") calculates LCCA as state p
 * - Exit set includes ps1 (proper descendant of LCCA), increments Var1
 * - Target ps2 is already active, but external transition semantics apply
 * - Validation checks that correct onexit handlers executed (Var1 == 1, Var2 == 0, etc.)
 *
 * Uses Static Hybrid approach: static state machine structure with
 * runtime ECMAScript expression evaluation via JSEngine for variable tracking.
 */
struct Test504 : public SimpleAotTest<Test504, 504> {
    static constexpr const char *DESCRIPTION = "External transition exit sets LCCA (W3C 3.13 AOT Static Hybrid)";
    using SM = RSM::Generated::test504::test504;
};

// Auto-register
inline static AotTestRegistrar<Test504> registrar_Test504;

}  // namespace RSM::W3C::AotTests
