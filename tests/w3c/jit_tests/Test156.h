#pragma once

#include "SimpleJitTest.h"
#include "test156_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Foreach error handling stops loop (JIT JSEngine)
 */
struct Test156 : public SimpleJitTest<Test156, 156> {
    static constexpr const char *DESCRIPTION = "Foreach error handling stops loop (JIT JSEngine)";
    using SM = RSM::Generated::test156::test156;
};

// Auto-register
inline static JitTestRegistrar<Test156> registrar_Test156;

}  // namespace RSM::W3C::JitTests
