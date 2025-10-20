#pragma once

#include "SimpleAotTest.h"
#include "test194_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Invalid target raises error.execution (W3C 6.2 AOT)
 */
struct Test194 : public SimpleAotTest<Test194, 194> {
    static constexpr const char *DESCRIPTION = "Invalid target raises error.execution (W3C 6.2 AOT)";
    using SM = RSM::Generated::test194::test194;
};

// Auto-register
inline static AotTestRegistrar<Test194> registrar_Test194;

}  // namespace RSM::W3C::AotTests
