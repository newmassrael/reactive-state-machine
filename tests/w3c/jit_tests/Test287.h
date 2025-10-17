#pragma once
#include "SimpleJitTest.h"
#include "test287_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 5.4: Valid assignment to valid location
 *
 * Tests that if the location expression denotes a valid location in the datamodel
 * and the value specified by 'expr' is a legal value for that location,
 * the processor places the specified value at the specified location.
 *
 * Test assigns value 1 to variable Var1 (initialized to 0), then verifies
 * the assignment succeeded by checking Var1 == 1.
 */
struct Test287 : public SimpleJitTest<Test287, 287> {
    static constexpr const char *DESCRIPTION = "Valid assignment to valid location (W3C 5.4 JIT)";
    using SM = RSM::Generated::test287::test287;
};

// Auto-register
inline static JitTestRegistrar<Test287> registrar_Test287;

}  // namespace RSM::W3C::JitTests
