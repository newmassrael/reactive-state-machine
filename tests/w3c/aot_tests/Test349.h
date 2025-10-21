#pragma once
#include "SimpleAotTest.h"
#include "test349_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10.1: _event.origin system variable
 *
 * Tests that the _event.origin system variable correctly provides information
 * about the origin of the event. According to W3C SCXML 5.10.1, _event.origin
 * contains the send ID of the triggering send element if the event was sent
 * from an external source.
 */
struct Test349 : public SimpleAotTest<Test349, 349> {
    static constexpr const char *DESCRIPTION = "_event.origin system variable (W3C 5.10.1 AOT)";
    using SM = RSM::Generated::test349::test349;
};

// Auto-register
inline static AotTestRegistrar<Test349> registrar_Test349;

}  // namespace RSM::W3C::AotTests
