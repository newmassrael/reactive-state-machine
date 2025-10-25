#pragma once
#include "SimpleAotTest.h"
#include "test505_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.13: Internal transition does not exit source state
 *
 * Validates that an internal transition (type="internal") does not exit its
 * source state when the source state is compound and all target states are
 * proper descendants of the source state.
 *
 * This test verifies correct W3C SCXML 3.13 internal transition semantics by
 * using counters to track exit/entry behavior. An internal transition from s1
 * to its child s11 should NOT trigger s1's onexit handler, while an external
 * transition or a transition that leaves s1 should trigger it.
 *
 * Expected behavior:
 * - State s1 is entered, entering child s11 (Var1 = 0, s1 not exited yet)
 * - Event "foo" triggers internal transition from s1 to s11 (type="internal")
 * - Internal transition does NOT exit s1 (Var1 remains 0)
 * - s11 is exited and re-entered (Var2 incremented)
 * - Var3 tracks internal transition execution (incremented to 1)
 * - Event "bar" triggers external transition from s1 to s2
 * - s1 onexit executed (Var1 = 1), validation checks counters
 *
 * Uses Static Hybrid approach: static state machine structure with
 * runtime ECMAScript expression evaluation via JSEngine for variable tracking.
 */
struct Test505 : public SimpleAotTest<Test505, 505> {
    static constexpr const char *DESCRIPTION = "Internal transition does not exit source (W3C 3.13 AOT Static Hybrid)";
    using SM = RSM::Generated::test505::test505;
};

// Auto-register
inline static AotTestRegistrar<Test505> registrar_Test505;

}  // namespace RSM::W3C::AotTests
