#pragma once
#include "SimpleAotTest.h"
#include "test579_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.11: Default history content executed only when no stored history
 *
 * Tests that default history content is executed correctly. The Process MUST execute
 * any executable content in the transition after the parent state's onentry handlers.
 * However the Processor MUST execute this content only if there is no stored history.
 * Once the history state's parent state has been visited and exited, the default
 * history content must not be executed.
 */
struct Test579 : public SimpleAotTest<Test579, 579> {
    static constexpr const char *DESCRIPTION = "Default history content execution (W3C 3.11 AOT)";
    using SM = RSM::Generated::test579::test579;
};

// Auto-register
inline static AotTestRegistrar<Test579> registrar_Test579;

}  // namespace RSM::W3C::AotTests
