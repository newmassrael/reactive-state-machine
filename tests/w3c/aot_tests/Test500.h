#pragma once
#include "SimpleAotTest.h"
#include "test500_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 6.3.1: SCXML Event I/O Processor Location Field
 *
 * Tests that the location field is accessible via _ioprocessors['scxml']['location']
 * in the ECMAScript datamodel using SystemVariableHelper for initialization
 * (Single Source of Truth shared between Interpreter and AOT engines).
 *
 * ARCHITECTURE.md Zero Duplication: SystemVariableHelper ensures identical
 * W3C SCXML 5.10 system variable initialization semantics across both engines.
 *
 * Expected behavior:
 * - Data element with expr="_ioprocessors['scxml']['location']" initializes Var1
 * - SystemVariableHelper sets up _ioprocessors system variable at runtime
 * - JSEngine evaluates _ioprocessors system variable at runtime (Static Hybrid)
 * - Transition condition uses typeof operator to verify Var1 is defined
 * - State machine transitions to pass if Var1 !== 'undefined'
 *
 * Uses Static Hybrid approach: static state machine structure with
 * runtime ECMAScript expression evaluation via JSEngine for system variables.
 */
struct Test500 : public SimpleAotTest<Test500, 500> {
    static constexpr const char *DESCRIPTION = "SCXML I/O processor location field (W3C 6.3.1 AOT Static Hybrid)";
    using SM = RSM::Generated::test500::test500;
};

// Auto-register
inline static AotTestRegistrar<Test500> registrar_Test500;

}  // namespace RSM::W3C::AotTests
