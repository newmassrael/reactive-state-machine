#pragma once

#include "SimpleJitTest.h"
#include "test151_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Foreach declares new variables (JIT JSEngine)
 */
struct Test151 : public SimpleJitTest<Test151, 151> {
    static constexpr const char *DESCRIPTION = "Foreach declares new variables (JIT JSEngine)";
    using SM = RSM::Generated::test151::test151;
};

// Auto-register
inline static JitTestRegistrar<Test151> registrar_Test151;

}  // namespace RSM::W3C::JitTests
