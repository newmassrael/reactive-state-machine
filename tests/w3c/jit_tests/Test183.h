#pragma once

#include "SimpleJitTest.h"
#include "test183_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Basic conditional transition (JIT)
 */
struct Test183 : public SimpleJitTest<Test183, 183> {
    static constexpr const char *DESCRIPTION = "Basic conditional transition (JIT)";
    using SM = RSM::Generated::test183::test183;
};

// Auto-register
inline static JitTestRegistrar<Test183> registrar_Test183;

}  // namespace RSM::W3C::JitTests
