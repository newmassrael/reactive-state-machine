#pragma once

#include "SimpleAotTest.h"
#include "test179_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Send content populates event body (AOT)
 */
struct Test179 : public SimpleAotTest<Test179, 179> {
    static constexpr const char *DESCRIPTION = "Send content populates event body (AOT)";
    using SM = RSM::Generated::test179::test179;
};

// Auto-register
inline static AotTestRegistrar<Test179> registrar_Test179;

}  // namespace RSM::W3C::AotTests
