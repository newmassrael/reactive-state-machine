#pragma once
#include "SimpleAotTest.h"
#include "test314_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.9.2: Delayed evaluation of illegal expression in assign
 *
 * Tests that error.execution is not raised until the illegal expression
 * is actually evaluated (in state s03), not when the document is loaded.
 * Unlike test313 which evaluates immediately in s0, this test verifies
 * the error is raised only when reaching s03.
 */
struct Test314 : public SimpleAotTest<Test314, 314> {
    static constexpr const char *DESCRIPTION = "Delayed assign illegal expression (W3C 5.9.2 AOT)";
    using SM = RSM::Generated::test314::test314;
};

// Auto-register
inline static AotTestRegistrar<Test314> registrar_Test314;

}  // namespace RSM::W3C::AotTests
