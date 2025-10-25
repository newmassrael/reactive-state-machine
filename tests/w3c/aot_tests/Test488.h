#pragma once
#include "SimpleAotTest.h"
#include "test488_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.7: Error handling in <param> expressions
 *
 * Tests that illegal expressions in <param> produce error.execution event
 * and result in empty event.data. The test validates:
 * 1. Invalid property access (undefined.invalidProperty) in <param expr> raises error.execution
 * 2. Error occurs before done.state event is processed
 * 3. Subsequent done.state event has empty event.data (param evaluation failed)
 *
 * Expected behavior:
 * - State s0 contains substates s01 and s02
 * - Transition from s01 to s02 has <donedata> with <param expr="undefined.invalidProperty">
 * - JSEngine evaluates param expression at runtime, detects illegal access
 * - DoneDataHelper raises error.execution event when param evaluation fails
 * - State machine transitions to pass state upon receiving error.execution
 * - done.state.s0 event follows with empty data
 *
 * Uses Static Hybrid approach: static state machine structure with
 * runtime ECMAScript expression evaluation via JSEngine.
 */
struct Test488 : public SimpleAotTest<Test488, 488> {
    static constexpr const char *DESCRIPTION = "donedata param error handling (W3C 5.7 AOT)";
    using SM = RSM::Generated::test488::test488;
};

// Auto-register
inline static AotTestRegistrar<Test488> registrar_Test488;

}  // namespace RSM::W3C::AotTests
