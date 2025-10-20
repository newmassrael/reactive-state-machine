#pragma once

#include "SimpleAotTest.h"
#include "test148_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Else clause execution with datamodel
 */
struct Test148 : public SimpleAotTest<Test148, 148> {
    static constexpr const char *DESCRIPTION = "Else clause execution with datamodel";
    using SM = RSM::Generated::test148::test148;
};

// Auto-register
inline static AotTestRegistrar<Test148> registrar_Test148;

}  // namespace RSM::W3C::AotTests
