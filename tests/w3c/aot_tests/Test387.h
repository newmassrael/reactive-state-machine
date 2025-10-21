#pragma once
#include "SimpleAotTest.h"
#include "test387_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.11: History States with Default Transitions
 *
 * Tests that default history state mechanisms work correctly:
 * - Shallow history with default transition to direct child state
 * - Deep history with default transition to deeply nested state
 * - History state behavior when no previous configuration exists
 * - Correct entry action execution during history resolution
 *
 * From initial state s3, transitions to s0's default shallow history,
 * which targets s01/s011 (generating "enteringS011"). Then transitions
 * to s4, which goes to s1's default deep history targeting s122
 * (generating "enteringS122" for pass).
 *
 * Static Generation Implementation:
 * - History state targets resolved to default transitions at parse time
 * - No runtime history recording/restoration needed for this test
 * - Sufficient for W3C compliance when history is not previously recorded
 */
struct Test387 : public SimpleAotTest<Test387, 387> {
    static constexpr const char *DESCRIPTION = "History states with default transitions (W3C 3.11 AOT)";
    using SM = RSM::Generated::test387::test387;
};

// Auto-register
inline static AotTestRegistrar<Test387> registrar_Test387;

}  // namespace RSM::W3C::AotTests
