#pragma once
#include "SimpleAotTest.h"
#include "test496_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML C.1: error.communication for unreachable target
 *
 * Tests unreachable target detection using SendHelper::isUnreachableTarget()
 * (Single Source of Truth shared between Interpreter and AOT engines).
 *
 * ARCHITECTURE.md Zero Duplication: SendHelper pattern ensures identical
 * error.communication semantics across both engines.
 *
 * Expected behavior:
 * - State s0 sends event with targetexpr="undefined"
 * - JSEngine evaluates targetexpr at runtime (Static Hybrid approach)
 * - SendHelper::isUnreachableTarget() validates target is "undefined"
 * - error.communication event raised and placed on internal queue
 * - State machine transitions to pass state upon receiving error.communication
 *
 * Uses Static Hybrid approach: static state machine structure with
 * runtime ECMAScript expression evaluation via JSEngine for targetexpr.
 */
struct Test496 : public SimpleAotTest<Test496, 496> {
    static constexpr const char *DESCRIPTION = "error.communication for unreachable target (W3C C.1 AOT Static Hybrid)";
    using SM = RSM::Generated::test496::test496;
};

// Auto-register
inline static AotTestRegistrar<Test496> registrar_Test496;

}  // namespace RSM::W3C::AotTests
