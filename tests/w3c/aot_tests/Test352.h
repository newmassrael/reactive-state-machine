#pragma once
#include "SimpleAotTest.h"
#include "test352_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10.1: Event origintype field validation
 *
 * Tests that _event.origintype is correctly set to
 * 'http://www.w3.org/TR/scxml/#SCXMLEventProcessor' when events are
 * sent via the internal SCXML event processor using the <send> element.
 *
 * The test sends an event with explicit type attribute, captures the
 * origintype in a datamodel variable, and validates it matches the
 * expected processor URI.
 */
struct Test352 : public SimpleAotTest<Test352, 352> {
    static constexpr const char *DESCRIPTION = "Event origintype field (W3C 5.10.1 AOT)";
    using SM = RSM::Generated::test352::test352;
};

// Auto-register
inline static AotTestRegistrar<Test352> registrar_Test352;

}  // namespace RSM::W3C::AotTests
