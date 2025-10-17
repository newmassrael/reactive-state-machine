#pragma once

#include "SimpleJitTest.h"
#include "test174_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Send typeexpr uses current datamodel value (JIT)
 */
struct Test174 : public SimpleJitTest<Test174, 174> {
    static constexpr const char *DESCRIPTION = "Send typeexpr uses current datamodel value (JIT)";
    using SM = RSM::Generated::test174::test174;
};

// Auto-register
inline static JitTestRegistrar<Test174> registrar_Test174;

}  // namespace RSM::W3C::JitTests
