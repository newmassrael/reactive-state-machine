#pragma once

#include "SimpleJitTest.h"
#include "test277_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Datamodel init error.execution (W3C 5.3 JIT)
 */
struct Test277 : public SimpleJitTest<Test277, 277> {
    static constexpr const char *DESCRIPTION = "Datamodel init error.execution (W3C 5.3 JIT)";
    using SM = RSM::Generated::test277::test277;
};

// Auto-register
inline static JitTestRegistrar<Test277> registrar_Test277;

}  // namespace RSM::W3C::JitTests
