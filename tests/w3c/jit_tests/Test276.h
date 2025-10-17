#pragma once

#include "SimpleJitTest.h"
#include "test276_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 6.2/6.4: child SCXML with params (JIT)
 */
struct Test276 : public SimpleJitTest<Test276, 276> {
    static constexpr const char *DESCRIPTION = "W3C SCXML 6.2/6.4: child SCXML with params (JIT)";
    using SM = RSM::Generated::test276::test276;
};

// Auto-register
inline static JitTestRegistrar<Test276> registrar_Test276;

}  // namespace RSM::W3C::JitTests
