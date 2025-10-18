#pragma once
#include "SimpleJitTest.h"
#include "test313_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 5.9.2: Assign with illegal expression must raise error.execution
 *
 * This test verifies that when an assign element contains an illegal expression
 * (undefined.invalidProperty), the processor raises error.execution and stops
 * processing subsequent executable content (raise foo should not execute).
 */
struct Test313 : public SimpleJitTest<Test313, 313> {
    static constexpr const char *DESCRIPTION = "Assign illegal expression error.execution (W3C 5.9.2 JIT)";
    using SM = RSM::Generated::test313::test313;
};

// Auto-register
inline static JitTestRegistrar<Test313> registrar_Test313;

}  // namespace RSM::W3C::JitTests
