#pragma once
#include "SimpleAotTest.h"
#include "test339_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10: Non-invoked events have blank invokeid field
 *
 * Tests that events raised via <raise> have no invokeid value,
 * distinguishing them from events sent from invoked child processes.
 * Per W3C SCXML 5.10: "The SCXML Processor must set the invokeid field
 * to the invokeid of the invocation that triggered the child process.
 * For internal events, this field has no value."
 */
struct Test339 : public SimpleAotTest<Test339, 339> {
    static constexpr const char *DESCRIPTION = "Internal event invokeid blank (W3C 5.10 AOT)";
    using SM = RSM::Generated::test339::test339;
};

// Auto-register
inline static AotTestRegistrar<Test339> registrar_Test339;

}  // namespace RSM::W3C::AotTests
