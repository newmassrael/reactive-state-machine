#pragma once

#include "SimpleJitTest.h"
#include "test173_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Send targetexpr uses current datamodel value (JIT)
 */
struct Test173 : public SimpleJitTest<Test173, 173> {
    static constexpr const char *DESCRIPTION = "Send targetexpr uses current datamodel value (JIT)";
    using SM = RSM::Generated::test173::test173;
};

// Auto-register
inline static JitTestRegistrar<Test173> registrar_Test173;

}  // namespace RSM::W3C::JitTests
