#pragma once

#include "SimpleJitTest.h"
#include "test158_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Executable content document order (JIT)
 */
struct Test158 : public SimpleJitTest<Test158, 158> {
    static constexpr const char *DESCRIPTION = "Executable content document order (JIT)";
    using SM = RSM::Generated::test158::test158;
};

// Auto-register
inline static JitTestRegistrar<Test158> registrar_Test158;

}  // namespace RSM::W3C::JitTests
