#pragma once
#include "SimpleJitTest.h"
#include "test322_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML B.2.1: _sessionid system variable immutability
 *
 * Tests that the _sessionid system variable cannot be modified after initialization.
 * The test attempts to assign a new value to _sessionid and verifies that the original
 * value remains unchanged, ensuring the immutability of this system variable.
 *
 * W3C SCXML B.2.1: The '_sessionid' system variable is immutable and must not be
 * modified by the state machine. Any attempt to change its value should be ignored.
 */
struct Test322 : public SimpleJitTest<Test322, 322> {
    static constexpr const char *DESCRIPTION = "_sessionid immutability (W3C B.2.1 JIT)";
    using SM = RSM::Generated::test322::test322;
};

// Auto-register
inline static JitTestRegistrar<Test322> registrar_Test322;

}  // namespace RSM::W3C::JitTests
