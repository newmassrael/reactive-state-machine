#pragma once

#include "SimpleJitTest.h"
#include "test179_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Send content populates event body (JIT)
 */
struct Test179 : public SimpleJitTest<Test179, 179> {
    static constexpr const char *DESCRIPTION = "Send content populates event body (JIT)";
    using SM = RSM::Generated::test179::test179;
};

// Auto-register
inline static JitTestRegistrar<Test179> registrar_Test179;

}  // namespace RSM::W3C::JitTests
