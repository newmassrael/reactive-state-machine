#pragma once
#include "SimpleJitTest.h"
#include "test326_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML B.2.3: _ioprocessors system variable immutability
 *
 * Tests that the _ioprocessors system variable:
 * 1. Is bound at session start
 * 2. Stays bound throughout the session
 * 3. Cannot be assigned to (raises error.execution)
 * 4. Remains immutable after failed assignment attempts
 *
 * Expected behavior:
 * - s0: Check Var1 = _ioprocessors is defined → s1
 * - s1: Attempt assign to _ioprocessors → error.execution → s2
 * - s2: Verify Var2 = _ioprocessors still bound → pass
 */
struct Test326 : public SimpleJitTest<Test326, 326> {
    static constexpr const char *DESCRIPTION = "_ioprocessors immutability (W3C B.2.3 JIT)";
    using SM = RSM::Generated::test326::test326;
};

// Auto-register
inline static JitTestRegistrar<Test326> registrar_Test326;

}  // namespace RSM::W3C::JitTests
