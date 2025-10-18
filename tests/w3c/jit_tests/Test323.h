#pragma once
#include "SimpleJitTest.h"
#include "test323_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML B.2.1: System variable _name binding on startup
 *
 * Tests that the _name system variable is bound to the state machine name
 * at initialization time and accessible in datamodel expressions. The test
 * validates that _name contains the correct machine name and can be used
 * in conditional expressions.
 *
 * W3C SCXML B.2.1: The '_name' system variable is bound to the name of the
 * state machine and is available throughout the session.
 */
struct Test323 : public SimpleJitTest<Test323, 323> {
    static constexpr const char *DESCRIPTION = "_name system variable (W3C B.2.1 JIT)";
    using SM = RSM::Generated::test323::test323;
};

// Auto-register
inline static JitTestRegistrar<Test323> registrar_Test323;

}  // namespace RSM::W3C::JitTests
