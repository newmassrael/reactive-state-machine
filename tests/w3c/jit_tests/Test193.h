#pragma once

#include "SimpleJitTest.h"
#include "test193_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Type attribute routes events to external queue (W3C 6.2.4 JIT)
 */
struct Test193 : public SimpleJitTest<Test193, 193> {
    static constexpr const char *DESCRIPTION = "Type attribute routes events to external queue (W3C 6.2.4 JIT)";
    using SM = RSM::Generated::test193::test193;
};

// Auto-register
inline static JitTestRegistrar<Test193> registrar_Test193;

}  // namespace RSM::W3C::JitTests
