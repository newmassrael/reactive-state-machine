#pragma once
#include "SimpleAotTest.h"
#include "test436_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.9.2: In() predicate evaluation in null data model
 *
 * Tests that the In() predicate correctly evaluates state membership
 * within a null data model and parallel state configuration.
 * - Parallel state 'p' contains child states 'ps0' and 'ps1'
 * - ps0 tests In('s1') (should fail - s1 not active)
 * - ps0 tests In('ps1') (should pass - ps1 active in parallel)
 *
 * Validates: Static In() predicate implementation without ECMAScript engine
 */
struct Test436 : public SimpleAotTest<Test436, 436> {
    static constexpr const char *DESCRIPTION = "In() predicate in null data model (W3C 5.9.2 AOT)";
    using SM = RSM::Generated::test436::test436;
};

// Auto-register
inline static AotTestRegistrar<Test436> registrar_Test436;

}  // namespace RSM::W3C::AotTests
