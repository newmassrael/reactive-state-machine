#pragma once
#include "SimpleAotTest.h"
#include "test406_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.3: State entry order with parallel regions
 *
 * Tests that states are entered in proper entry order (parents before children
 * with document order used to break ties) after executable content in transitions
 * is executed. Validates that when entering a parallel state with child regions,
 * the parent's onentry executes before children's onentry, ensuring event order:
 * event1 (transition) → event2 (parallel onentry) → event3 (region1 onentry) → event4 (region2 onentry).
 */
struct Test406 : public SimpleAotTest<Test406, 406> {
    static constexpr const char *DESCRIPTION = "State entry order with parallel regions (W3C 3.3 AOT)";
    using SM = RSM::Generated::test406::test406;
};

// Auto-register
inline static AotTestRegistrar<Test406> registrar_Test406;

}  // namespace RSM::W3C::AotTests
