#pragma once
#include "SimpleAotTest.h"
#include "test337_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10.1: Internal events have blank origintype field
 *
 * Tests that events raised via <raise> have no origintype value,
 * distinguishing them from external events (which have "http://www.w3.org/TR/scxml/#SCXMLEventProcessor").
 * Per W3C SCXML 5.10.1: "The SCXML Processor must set the origintype field to a value
 * which indicates the type of the Event I/O Processor that the event was received from.
 * For internal events, this field has no value."
 */
struct Test337 : public SimpleAotTest<Test337, 337> {
    static constexpr const char *DESCRIPTION = "Internal event origintype blank (W3C 5.10.1 AOT)";
    using SM = RSM::Generated::test337::test337;
};

// Auto-register
inline static AotTestRegistrar<Test337> registrar_Test337;

}  // namespace RSM::W3C::AotTests
