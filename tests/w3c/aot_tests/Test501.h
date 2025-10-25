#pragma once
#include "SimpleAotTest.h"
#include "test501_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML C.1: SCXML Event I/O Processor location field as send target
 *
 * Validates that _ioprocessors['scxml']['location'] provides an accessible
 * address for external entities to communicate with the SCXML session, and
 * that this location can be used as a send target for event routing.
 *
 * Tests that the location field is properly initialized via SystemVariableHelper
 * (Single Source of Truth shared between Interpreter and AOT engines).
 *
 * ARCHITECTURE.md Zero Duplication: SystemVariableHelper ensures identical
 * W3C SCXML 5.10 system variable initialization semantics across both engines.
 *
 * Expected behavior:
 * - Data element stores _ioprocessors['scxml']['location'] in Var1
 * - SystemVariableHelper sets up _ioprocessors system variable at runtime
 * - Send action uses Var1 as target to send event to self
 * - Event successfully delivered, triggering transition to pass state
 *
 * Uses Static Hybrid approach: static state machine structure with
 * runtime ECMAScript expression evaluation via JSEngine for system variables.
 */
struct Test501 : public SimpleAotTest<Test501, 501> {
    static constexpr const char *DESCRIPTION =
        "SCXML Event I/O Processor location as send target (W3C C.1 AOT Static Hybrid)";
    using SM = RSM::Generated::test501::test501;
};

// Auto-register
inline static AotTestRegistrar<Test501> registrar_Test501;

}  // namespace RSM::W3C::AotTests
