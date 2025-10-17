#pragma once
#include "SimpleJitTest.h"
#include "test286_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 5.3: error.execution for invalid assignment location
 *
 * Tests that assignment to a non-declared variable (empty location)
 * causes error.execution to be raised. The test has two paths:
 * 1. error.execution → pass (correct behavior)
 * 2. foo event (no error raised) → fail (incorrect behavior)
 *
 * Per W3C SCXML 5.3: "If the location expression does not denote a valid
 * location in the data model... the processor must place the error
 * 'error.execution' in the internal event queue."
 */
struct Test286 : public SimpleJitTest<Test286, 286> {
    static constexpr const char *DESCRIPTION = "Invalid assignment location error handling (W3C 5.3 JIT)";
    using SM = RSM::Generated::test286::test286;
};

// Auto-register
inline static JitTestRegistrar<Test286> registrar_Test286;

}  // namespace RSM::W3C::JitTests
