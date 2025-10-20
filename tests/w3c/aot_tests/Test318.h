#pragma once
#include "SimpleAotTest.h"
#include "test318_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10: _event variable binding and persistence
 *
 * Tests that the SCXML Processor binds the _event variable when an event
 * is pulled off the internal or external event queue to be processed, and
 * keeps the variable bound to that event until another event is processed.
 *
 * The test verifies that _event.name remains "foo" during the onentry of
 * the next state (s1), even after a new event "bar" is raised.
 */
struct Test318 : public SimpleAotTest<Test318, 318> {
    static constexpr const char *DESCRIPTION = "_event variable binding (W3C 5.10 AOT)";
    using SM = RSM::Generated::test318::test318;
};

// Auto-register
inline static AotTestRegistrar<Test318> registrar_Test318;

}  // namespace RSM::W3C::AotTests
