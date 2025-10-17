#pragma once

#include "SimpleJitTest.h"
#include "test208_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Cancel delayed send by sendid (W3C 6.3 JIT)
 *
 * Requires event scheduler polling for delayed send processing.
 */
struct Test208 : public ScheduledJitTest<Test208, 208> {
    static constexpr const char *DESCRIPTION = "Cancel delayed send by sendid (W3C 6.3 JIT)";
    using SM = RSM::Generated::test208::test208;
};

// Auto-register
inline static JitTestRegistrar<Test208> registrar_Test208;

}  // namespace RSM::W3C::JitTests
