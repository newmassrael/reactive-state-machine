#pragma once
#include "SimpleAotTest.h"
#include "test413_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.13: Parallel initial states with space-separated state IDs
 *
 * Tests that at startup, the SCXML Processor places the state machine in the
 * configuration specified by the 'initial' attribute of the scxml element,
 * specifically testing parallel initial state format with space-separated state IDs.
 *
 * The initial attribute contains "s2p112 s2p122" which are parallel regions that
 * should both be entered simultaneously.
 */
struct Test413 : public SimpleAotTest<Test413, 413> {
    static constexpr const char *DESCRIPTION = "Parallel initial states (W3C 3.13 AOT)";
    using SM = RSM::Generated::test413::test413;
};

// Auto-register
inline static AotTestRegistrar<Test413> registrar_Test413;

}  // namespace RSM::W3C::AotTests
