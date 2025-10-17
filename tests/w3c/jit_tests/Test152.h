#pragma once

#include "SimpleJitTest.h"
#include "test152_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Foreach error handling (JIT JSEngine)
 */
struct Test152 : public SimpleJitTest<Test152, 152> {
    static constexpr const char *DESCRIPTION = "Foreach error handling (JIT JSEngine)";
    using SM = RSM::Generated::test152::test152;
};

// Auto-register
inline static JitTestRegistrar<Test152> registrar_Test152;

}  // namespace RSM::W3C::JitTests
