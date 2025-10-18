#pragma once
#include "SimpleJitTest.h"
#include "test321_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 5.10.1: _sessionid system variable binding on startup
 *
 * Tests that the _sessionid system variable is properly bound when the state machine
 * is created and can be accessed in data model initialization expressions.
 * Uses ECMAScript typeof operator to verify variable is defined.
 *
 * W3C SCXML 5.10.1: The '_sessionid' system variable contains a unique identifier
 * for the state machine session. It must be bound at state machine creation time.
 */
struct Test321 : public SimpleJitTest<Test321, 321> {
    static constexpr const char *DESCRIPTION = "_sessionid binding (W3C 5.10.1 JIT)";
    using SM = RSM::Generated::test321::test321;
};

// Auto-register
inline static JitTestRegistrar<Test321> registrar_Test321;

}  // namespace RSM::W3C::JitTests
