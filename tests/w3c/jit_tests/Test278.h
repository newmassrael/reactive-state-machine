#pragma once

#include "SimpleJitTest.h"
#include "test278_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 5.10: Global datamodel scope
 *
 * Tests that variables defined in state-level datamodel are globally accessible.
 * Variable Var1 defined in state s1's datamodel should be accessible from state s0.
 */
struct Test278 : public SimpleJitTest<Test278, 278> {
    static constexpr const char *DESCRIPTION = "Global scope datamodel access (W3C 5.10 JIT)";
    using SM = RSM::Generated::test278::test278;
};

// Auto-register
inline static JitTestRegistrar<Test278> registrar_Test278;

}  // namespace RSM::W3C::JitTests
