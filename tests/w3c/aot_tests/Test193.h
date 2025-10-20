#pragma once

#include "SimpleAotTest.h"
#include "test193_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Type attribute routes events to external queue (W3C 6.2.4 AOT)
 */
struct Test193 : public SimpleAotTest<Test193, 193> {
    static constexpr const char *DESCRIPTION = "Type attribute routes events to external queue (W3C 6.2.4 AOT)";
    using SM = RSM::Generated::test193::test193;
};

// Auto-register
inline static AotTestRegistrar<Test193> registrar_Test193;

}  // namespace RSM::W3C::AotTests
