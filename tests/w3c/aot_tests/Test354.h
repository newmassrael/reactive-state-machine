#pragma once
#include "SimpleAotTest.h"
#include "test354_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10: Event data population with namelist, param, and content
 *
 * Tests that _event.data can be populated using three mechanisms:
 * - namelist attribute: Variables listed in namelist should appear in event.data
 * - <param> elements: Parameters should appear in event.data
 * - <content> element: Content should be accessible via event.data
 *
 * The test validates that all three mechanisms correctly populate the event data
 * and that the values are accessible via ECMAScript expressions like _event.data.Var1.
 */
struct Test354 : public SimpleAotTest<Test354, 354> {
    static constexpr const char *DESCRIPTION = "Event data with namelist/param/content (W3C 5.10 AOT)";
    using SM = RSM::Generated::test354::test354;
};

// Auto-register
inline static AotTestRegistrar<Test354> registrar_Test354;

}  // namespace RSM::W3C::AotTests
