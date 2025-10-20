#pragma once

#include "SimpleJitTest.h"
#include "test276_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 6.4: Static invoke with param passing
 *
 * Test that values passed in from parent process override default values
 * specified in the child. The child returns event1 if Var1 has value 1,
 * event0 if it has default value 0.
 * Requires event scheduler polling for child state machine completion.
 */
struct Test276 : public ScheduledJitTest<Test276, 276> {
    static constexpr const char *DESCRIPTION = "Static invoke param passing (W3C 6.4 JIT)";
    using SM = RSM::Generated::test276::test276;
};

// Auto-register
inline static JitTestRegistrar<Test276> registrar_Test276;

}  // namespace RSM::W3C::JitTests
