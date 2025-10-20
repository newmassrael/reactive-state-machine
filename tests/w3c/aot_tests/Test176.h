#pragma once

#include "SimpleAotTest.h"
#include "test176_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Send param uses current datamodel value (AOT)
 */
struct Test176 : public SimpleAotTest<Test176, 176> {
    static constexpr const char *DESCRIPTION = "Send param uses current datamodel value (AOT)";
    using SM = RSM::Generated::test176::test176;
};

// Auto-register
inline static AotTestRegistrar<Test176> registrar_Test176;

}  // namespace RSM::W3C::AotTests
