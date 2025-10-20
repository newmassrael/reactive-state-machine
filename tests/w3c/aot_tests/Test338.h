#pragma once
#include "SimpleAotTest.h"
#include "test338_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 6.4: Static invoke with inline content child SCXML
 *
 * Tests that invokeid is correctly set in events received from an invoked child process
 * when the child SCXML is defined inline using <invoke><content><scxml>...</scxml></content></invoke>.
 * Per W3C SCXML 6.4: "The SCXML Processor must support the ability to invoke SCXML sessions
 * via the type http://www.w3.org/TR/scxml/."
 *
 * This test verifies:
 * - Inline child SCXML extraction and static code generation
 * - Parent-child communication via #_parent target
 * - Invokeid assignment and verification (Var1 === Var2)
 * - Closed World static invoke (compile-time child SCXML, no external dependencies)
 */
struct Test338 : public SimpleAotTest<Test338, 338> {
    static constexpr const char *DESCRIPTION = "Static invoke inline content (W3C 6.4 AOT)";
    using SM = RSM::Generated::test338::test338;
};

// Auto-register
inline static AotTestRegistrar<Test338> registrar_Test338;

}  // namespace RSM::W3C::AotTests
