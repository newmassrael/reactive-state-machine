#pragma once
#include "SimpleAotTest.h"
#include "test399_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.9.3: Event name matching algorithm
 *
 * Tests correct event name matching including:
 * - Multiple event descriptors in transition (space-separated)
 * - Prefix matching (foo.zoo matches "foo bar")
 * - Token boundary checking (foos does NOT match "foo")
 * - Wildcard suffix (foo.* matches foo.zoo)
 * - Universal wildcard (* matches any event)
 */
struct Test399 : public SimpleAotTest<Test399, 399> {
    static constexpr const char *DESCRIPTION = "Event name matching with prefix and wildcard (W3C 5.9.3 AOT)";
    using SM = RSM::Generated::test399::test399;
};

// Auto-register
inline static AotTestRegistrar<Test399> registrar_Test399;

}  // namespace RSM::W3C::AotTests
