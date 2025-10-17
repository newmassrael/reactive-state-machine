#pragma once

#include "SimpleJitTest.h"
#include "test194_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Invalid target raises error.execution (W3C 6.2 JIT)
 */
struct Test194 : public SimpleJitTest<Test194, 194> {
    static constexpr const char *DESCRIPTION = "Invalid target raises error.execution (W3C 6.2 JIT)";
    using SM = RSM::Generated::test194::test194;
};

// Auto-register
inline static JitTestRegistrar<Test194> registrar_Test194;

}  // namespace RSM::W3C::JitTests
