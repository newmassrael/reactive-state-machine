#pragma once
#include "SimpleJitTest.h"
#include "test312_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 5.9.2: Assignment error handling for illegal expressions
 *
 * Tests that assignment with an illegal expression (undefined.invalidProperty)
 * raises an error.execution event. Verifies datamodel error handling per
 * W3C SCXML 5.9.2 specification.
 */
struct Test312 : public SimpleJitTest<Test312, 312> {
    static constexpr const char *DESCRIPTION = "Assignment illegal expr error (W3C 5.9.2 JIT)";
    using SM = RSM::Generated::test312::test312;
};

// Auto-register
inline static JitTestRegistrar<Test312> registrar_Test312;

}  // namespace RSM::W3C::JitTests
