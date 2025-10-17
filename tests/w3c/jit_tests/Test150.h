#pragma once

#include "SimpleJitTest.h"
#include "test150_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Foreach with dynamic variables (JIT JSEngine)
 */
struct Test150 : public SimpleJitTest<Test150, 150> {
    static constexpr const char *DESCRIPTION = "Foreach with dynamic variables (JIT JSEngine)";
    using SM = RSM::Generated::test150::test150;
};

// Auto-register
inline static JitTestRegistrar<Test150> registrar_Test150;

}  // namespace RSM::W3C::JitTests
