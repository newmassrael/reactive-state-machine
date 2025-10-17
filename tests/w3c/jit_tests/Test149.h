#pragma once

#include "SimpleJitTest.h"
#include "test149_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Neither if nor elseif executes
 */
struct Test149 : public SimpleJitTest<Test149, 149> {
    static constexpr const char *DESCRIPTION = "Neither if nor elseif executes";
    using SM = RSM::Generated::test149::test149;
};

// Auto-register
inline static JitTestRegistrar<Test149> registrar_Test149;

}  // namespace RSM::W3C::JitTests
