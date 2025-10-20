#pragma once

#include "SimpleAotTest.h"
#include "test173_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Send targetexpr uses current datamodel value (AOT)
 */
struct Test173 : public SimpleAotTest<Test173, 173> {
    static constexpr const char *DESCRIPTION = "Send targetexpr uses current datamodel value (AOT)";
    using SM = RSM::Generated::test173::test173;
};

// Auto-register
inline static AotTestRegistrar<Test173> registrar_Test173;

}  // namespace RSM::W3C::AotTests
