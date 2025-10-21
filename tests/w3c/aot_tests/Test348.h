#pragma once
#include "SimpleAotTest.h"
#include "test348_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 6.2: Send event parameter sets event name
 *
 * Tests that the event parameter in <send> correctly sets the name of the event
 * being sent. The event name specified in the event attribute should be used
 * as-is when dispatching the event to the target.
 */
struct Test348 : public SimpleAotTest<Test348, 348> {
    static constexpr const char *DESCRIPTION = "Send event parameter sets event name (W3C 6.2 AOT)";
    using SM = RSM::Generated::test348::test348;
};

// Auto-register
inline static AotTestRegistrar<Test348> registrar_Test348;

}  // namespace RSM::W3C::AotTests
