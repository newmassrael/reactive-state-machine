#pragma once

#include "SimpleAotTest.h"
#include "test174_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Send typeexpr uses current datamodel value (AOT)
 */
struct Test174 : public SimpleAotTest<Test174, 174> {
    static constexpr const char *DESCRIPTION = "Send typeexpr uses current datamodel value (AOT)";
    using SM = RSM::Generated::test174::test174;
};

// Auto-register
inline static AotTestRegistrar<Test174> registrar_Test174;

}  // namespace RSM::W3C::AotTests
