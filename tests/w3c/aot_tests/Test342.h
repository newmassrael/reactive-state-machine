#pragma once
#include "SimpleAotTest.h"
#include "test342_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 6.2: eventexpr dynamic event name evaluation
 *
 * Tests that <send eventexpr="Var1"/> evaluates the expression at send time
 * and dispatches an event with the resulting name. The test verifies that
 * _event.name correctly reflects the dynamically evaluated event name.
 * Per W3C SCXML 6.2: "The SCXML Processor must evaluate 'eventexpr' to obtain
 * the name of the event to be dispatched."
 */
struct Test342 : public SimpleAotTest<Test342, 342> {
    static constexpr const char *DESCRIPTION = "eventexpr dynamic event name (W3C 6.2 AOT)";
    using SM = RSM::Generated::test342::test342;
};

// Auto-register
inline static AotTestRegistrar<Test342> registrar_Test342;

}  // namespace RSM::W3C::AotTests
