#pragma once
#include "SimpleAotTest.h"
#include "test335_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10: _event.origin field must be blank for internal events
 *
 * Validates that when an event is raised internally using <raise>, the _event.origin
 * field is left blank (empty/undefined) as required by the specification. The origin
 * field should only be populated for external events received from other entities.
 *
 * This is part of the _event metadata validation series:
 * - Test 333: Validates _event.sendid is blank for non-error events
 * - Test 335: Validates _event.origin is blank for internal events
 * - Test 337: Validates _event.origintype is blank for internal events
 *
 * The test raises a "foo" event internally via <raise>, then verifies the event
 * is received with an empty origin field.
 */
struct Test335 : public SimpleAotTest<Test335, 335> {
    static constexpr const char *DESCRIPTION = "_event.origin blank for internal events (W3C 5.10 AOT)";
    using SM = RSM::Generated::test335::test335;
};

// Auto-register
inline static AotTestRegistrar<Test335> registrar_Test335;

}  // namespace RSM::W3C::AotTests
