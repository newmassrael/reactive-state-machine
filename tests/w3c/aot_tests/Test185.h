#pragma once

#include "SimpleAotTest.h"
#include "test185_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Basic delayed send (W3C SCXML 6.2 AOT)
 *
 * Requires event scheduler polling for delayed send processing.
 */
struct Test185 : public ScheduledAotTest<Test185, 185> {
    static constexpr const char *DESCRIPTION = "Basic delayed send (W3C SCXML 6.2 AOT)";
    using SM = RSM::Generated::test185::test185;
};

// Auto-register
inline static AotTestRegistrar<Test185> registrar_Test185;

}  // namespace RSM::W3C::AotTests
