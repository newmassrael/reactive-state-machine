#pragma once
#include "SimpleAotTest.h"
#include "test446_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.2.2: External file loading via src attribute
 *
 * Tests that <data> elements with src attribute correctly load external file content
 * at build time for static code generation. Validates ECMAScript Array type checking
 * with instanceof operator on both inline and externally loaded data.
 *
 * W3C SCXML 5.2.2: Data Model - src attribute for external content
 * W3C SCXML B.2: ECMAScript Data Model - Array type semantics
 */
struct Test446 : public SimpleAotTest<Test446, 446> {
    static constexpr const char *DESCRIPTION = "External file loading with src attribute (W3C 5.2.2 AOT)";
    using SM = RSM::Generated::test446::test446;
};

// Auto-register
inline static AotTestRegistrar<Test446> registrar_Test446;

}  // namespace RSM::W3C::AotTests
