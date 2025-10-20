#pragma once

#include "SimpleAotTest.h"
#include "test277_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Datamodel init error.execution (W3C 5.3 AOT)
 */
struct Test277 : public SimpleAotTest<Test277, 277> {
    static constexpr const char *DESCRIPTION = "Datamodel init error.execution (W3C 5.3 AOT)";
    using SM = RSM::Generated::test277::test277;
};

// Auto-register
inline static AotTestRegistrar<Test277> registrar_Test277;

}  // namespace RSM::W3C::AotTests
