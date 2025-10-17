#pragma once

#include "SimpleJitTest.h"
#include "test185_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Basic delayed send (W3C SCXML 6.2 JIT)
 *
 * Requires event scheduler polling for delayed send processing.
 */
struct Test185 : public ScheduledJitTest<Test185, 185> {
    static constexpr const char *DESCRIPTION = "Basic delayed send (W3C SCXML 6.2 JIT)";
    using SM = RSM::Generated::test185::test185;
};

// Auto-register
inline static JitTestRegistrar<Test185> registrar_Test185;

}  // namespace RSM::W3C::JitTests
