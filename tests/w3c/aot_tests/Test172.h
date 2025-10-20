#pragma once

#include "SimpleAotTest.h"
#include "test172_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Send eventexpr uses current datamodel value (AOT)
 */
struct Test172 : public SimpleAotTest<Test172, 172> {
    static constexpr const char *DESCRIPTION = "Send eventexpr uses current datamodel value (AOT)";
    using SM = RSM::Generated::test172::test172;
};

// Auto-register
inline static AotTestRegistrar<Test172> registrar_Test172;

}  // namespace RSM::W3C::AotTests
