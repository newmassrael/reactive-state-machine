#pragma once
#include "SimpleAotTest.h"
#include "test451_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.9.2: In() predicate in parallel states
 *
 * Tests that the In() predicate correctly checks state activation
 * within parallel state configurations.
 *
 * W3C SCXML 5.9.2: The In(stateID) predicate returns true if the state
 * machine is in the specified state. For parallel states, a region is
 * considered "in" a state if that state is in the active configuration.
 *
 * W3C SCXML 3.4: Parallel states execute all child states concurrently.
 * Each child state maintains its own active configuration independently.
 *
 * Test validates:
 * - In() predicate correctly identifies active parallel state s1
 * - Parallel state p activates both s0 and s1 simultaneously
 * - Transition with cond="In('s1')" successfully triggers in s0
 * - Pure static implementation via isStateActive() (no JSEngine needed)
 *
 * Implementation:
 * - Uses Pure Static approach (direct C++ isStateActive() call)
 * - In() predicate translated to this->isStateActive("s1")
 * - InPredicateHelper::isStateActive() checks active configuration
 * - ARCHITECTURE.md Zero Duplication: Follows established Helper pattern
 *   (SendHelper, GuardHelper, ForeachHelper) for Single Source of Truth
 * - No JSEngine needed - compile-time state ID verification
 */
struct Test451 : public SimpleAotTest<Test451, 451> {
    static constexpr const char *DESCRIPTION = "In() predicate in parallel states (W3C 5.9.2 AOT)";
    using SM = RSM::Generated::test451::test451;
};

// Auto-register
inline static AotTestRegistrar<Test451> registrar_Test451;

}  // namespace RSM::W3C::AotTests
