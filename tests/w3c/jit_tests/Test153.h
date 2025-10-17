#pragma once

#include "SimpleJitTest.h"
#include "test153_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Foreach array iteration order (JIT JSEngine)
 */
struct Test153 : public SimpleJitTest<Test153, 153> {
    static constexpr const char *DESCRIPTION = "Foreach array iteration order (JIT JSEngine)";
    using SM = RSM::Generated::test153::test153;
};

// Auto-register
inline static JitTestRegistrar<Test153> registrar_Test153;

}  // namespace RSM::W3C::JitTests
