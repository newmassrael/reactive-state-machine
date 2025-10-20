#pragma once
#include "SimpleAotTest.h"
#include "test319_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML B.1: System variable _event initially unbound
 *
 * Tests that the _event system variable is not bound before any event
 * has been raised. The test checks this condition in the initial state's
 * onentry and raises "unbound" if _event is undefined (pass), or "bound"
 * if it exists (fail).
 *
 * W3C SCXML 5.10.1: The '_event' system variable contains the current
 * event being processed. It is undefined before the first event.
 */
struct Test319 : public SimpleAotTest<Test319, 319> {
    static constexpr const char *DESCRIPTION = "_event not bound before first event (W3C B.1 AOT)";
    using SM = RSM::Generated::test319::test319;
};

// Auto-register
inline static AotTestRegistrar<Test319> registrar_Test319;

}  // namespace RSM::W3C::AotTests
