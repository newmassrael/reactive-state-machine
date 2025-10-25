#pragma once
#include "SimpleAotTest.h"
#include "test460_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 4.6: Foreach element shallow copy semantics
 *
 * Tests that <foreach> creates a shallow copy of the array, so modifying
 * the array during iteration does not change the iteration count.
 * The test initializes Var1=[1,2,3], then in each foreach iteration:
 * 1. Appends 4 to Var1 (making it longer)
 * 2. Increments Var2 counter
 * Expected: Var2==3 (exactly 3 iterations, despite array growing to [1,2,3,4,4,4])
 */
struct Test460 : public SimpleAotTest<Test460, 460> {
    static constexpr const char *DESCRIPTION = "Foreach shallow copy (W3C 4.6 AOT)";
    using SM = RSM::Generated::test460::test460;
};

// Auto-register
inline static AotTestRegistrar<Test460> registrar_Test460;

}  // namespace RSM::W3C::AotTests
