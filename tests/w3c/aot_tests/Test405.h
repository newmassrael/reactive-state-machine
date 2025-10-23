#pragma once
#include "SimpleAotTest.h"
#include "test405_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 3.9/D.2: Executable content execution order in transitions
 *
 * Tests that executable content in transitions is executed in document order
 * after states are exited. Specifically validates that when transitioning from
 * parallel substates, onexit handlers execute before transition actions, and
 * all actions execute in document order (event1→event2→event3→event4).
 */
struct Test405 : public SimpleAotTest<Test405, 405> {
    static constexpr const char *DESCRIPTION = "Transition executable content ordering (W3C 3.9 AOT)";
    using SM = RSM::Generated::test405::test405;
};

// Auto-register
inline static AotTestRegistrar<Test405> registrar_Test405;

}  // namespace RSM::W3C::AotTests
