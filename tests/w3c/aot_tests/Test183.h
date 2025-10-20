#pragma once

#include "SimpleAotTest.h"
#include "test183_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Basic conditional transition (AOT)
 */
struct Test183 : public SimpleAotTest<Test183, 183> {
    static constexpr const char *DESCRIPTION = "Basic conditional transition (AOT)";
    using SM = RSM::Generated::test183::test183;
};

// Auto-register
inline static AotTestRegistrar<Test183> registrar_Test183;

}  // namespace RSM::W3C::AotTests
