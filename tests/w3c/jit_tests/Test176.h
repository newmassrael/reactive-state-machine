#pragma once

#include "SimpleJitTest.h"
#include "test176_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Send param uses current datamodel value (JIT)
 */
struct Test176 : public SimpleJitTest<Test176, 176> {
    static constexpr const char *DESCRIPTION = "Send param uses current datamodel value (JIT)";
    using SM = RSM::Generated::test176::test176;
};

// Auto-register
inline static JitTestRegistrar<Test176> registrar_Test176;

}  // namespace RSM::W3C::JitTests
