#pragma once
#include "SimpleAotTest.h"
#include "test350_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.6: Default initial state (first child in document order)
 *
 * Tests that when the <scxml> element has no initial attribute, the state machine
 * correctly defaults to the first child state in document order, as required by
 * W3C SCXML specification section 3.6. This test validates the automatic initial
 * state resolution when no explicit initial is specified.
 */
struct Test350 : public SimpleAotTest<Test350, 350> {
    static constexpr const char *DESCRIPTION = "Default initial state (W3C 3.6 AOT)";
    using SM = RSM::Generated::test350::test350;
};

// Auto-register
inline static AotTestRegistrar<Test350> registrar_Test350;

}  // namespace RSM::W3C::AotTests
