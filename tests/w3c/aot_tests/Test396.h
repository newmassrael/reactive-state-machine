#pragma once
#include "SimpleAotTest.h"
#include "test396_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.12.1: Event name matching and document order transition selection
 *
 * Verifies that the SCXML processor uses the event name value from _event.name
 * to match against transition 'event' attributes. When multiple transitions match
 * the same event, the first transition in document order is selected.
 *
 * Test raises event "foo" which matches two transitions - the first one (to "pass")
 * should be taken per document order selection rules.
 */
struct Test396 : public SimpleAotTest<Test396, 396> {
    static constexpr const char *DESCRIPTION = "Event name matching (W3C 3.12.1 AOT)";
    using SM = RSM::Generated::test396::test396;
};

// Auto-register
inline static AotTestRegistrar<Test396> registrar_Test396;

}  // namespace RSM::W3C::AotTests
