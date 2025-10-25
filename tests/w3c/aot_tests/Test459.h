#pragma once
#include "SimpleAotTest.h"
#include "test459_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.4: foreach iteration order and index validation
 *
 * Tests that foreach iterates over arrays in correct order (ascending index 0→1→2).
 * Validates that:
 * - Array elements are processed sequentially (1, 2, 3)
 * - Index starts at 0 and increments to 2
 * - Item and index variables are correctly updated
 * - ECMAScript expressions in foreach body execute properly
 */
struct Test459 : public SimpleAotTest<Test459, 459> {
    static constexpr const char *DESCRIPTION = "foreach iteration order (W3C 5.4 AOT)";
    using SM = RSM::Generated::test459::test459;
};

// Auto-register
inline static AotTestRegistrar<Test459> registrar_Test459;

}  // namespace RSM::W3C::AotTests
