#pragma once

#include "SimpleJitTest.h"
#include "test200_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief SCXML event processor support (W3C 6.2 JIT)
 */
struct Test200 : public SimpleJitTest<Test200, 200> {
    static constexpr const char *DESCRIPTION = "SCXML event processor support (W3C 6.2 JIT)";
    using SM = RSM::Generated::test200::test200;
};

// Auto-register
inline static JitTestRegistrar<Test200> registrar_Test200;

}  // namespace RSM::W3C::JitTests
