#pragma once
#include "SimpleAotTest.h"
#include "test330_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10: Event field binding validation
 *
 * Tests that required _event fields are properly bound for both:
 * - Internal events (raised via <raise>)
 * - External events (sent via <send>)
 *
 * Validates presence of _event.name, _event.type, and other required fields
 * per W3C SCXML specification section 5.10.
 *
 * Expected behavior:
 * - s0: Raise internal event "foo" → s1
 * - s1: Verify internal event fields are bound → send external event "foo" → s2
 * - s2: Verify external event fields are bound → pass
 */
struct Test330 : public SimpleAotTest<Test330, 330> {
    static constexpr const char *DESCRIPTION = "Event field binding (W3C 5.10 AOT)";
    using SM = RSM::Generated::test330::test330;
};

// Auto-register
inline static AotTestRegistrar<Test330> registrar_Test330;

}  // namespace RSM::W3C::AotTests
