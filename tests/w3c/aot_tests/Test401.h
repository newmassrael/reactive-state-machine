#pragma once
#include "SimpleAotTest.h"
#include "test401_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.12.1: Internal event queue priority over external events
 *
 * Tests that error events raised by the processor are placed in the internal
 * event queue and processed with higher priority than external events.
 *
 * Test flow:
 * 1. Send external event "foo" to self via <send event="foo"/>
 * 2. Immediately raise error by invalid assign: <assign location="" expr="2"/>
 * 3. Processor must process error event (internal queue) before foo (external queue)
 *
 * Success: Transition to "pass" via error event (internal queue processed first)
 * Failure: Transition to "fail" via foo event (incorrect queue priority)
 */
struct Test401 : public SimpleAotTest<Test401, 401> {
    static constexpr const char *DESCRIPTION = "Internal event queue priority (W3C 3.12.1 AOT)";
    using SM = RSM::Generated::test401::test401;
};

// Auto-register
inline static AotTestRegistrar<Test401> registrar_Test401;

}  // namespace RSM::W3C::AotTests
