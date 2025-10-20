#pragma once
#include "SimpleAotTest.h"
#include "test343_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10.1: error.execution event with empty event.data for invalid param
 *
 * Tests that an illegal <param> element (with invalid location attribute)
 * in <donedata> produces an error.execution event with empty event.data.
 * Per W3C SCXML 5.10.1: "If evaluation of <param> fails, the SCXML Processor
 * must place an error.execution event in the internal event queue and use an
 * empty value for event.data."
 *
 * Test flow:
 * 1. s0/s01 transitions to s0/s02 (final state)
 * 2. s02 <donedata> contains <param location="foo"/> where "foo" doesn't exist
 * 3. DoneDataHelper::evaluateParams() detects invalid location and raises error.execution
 * 4. S0 transitions to s1 via <transition event="error.execution" target="s1"/>
 * 5. s1 receives done.state.s0 with empty event.data → transitions to Pass
 */
struct Test343 : public SimpleAotTest<Test343, 343> {
    static constexpr const char *DESCRIPTION = "Invalid param error.execution (W3C 5.10.1 AOT)";
    using SM = RSM::Generated::test343::test343;
};

// Auto-register
inline static AotTestRegistrar<Test343> registrar_Test343;

}  // namespace RSM::W3C::AotTests
