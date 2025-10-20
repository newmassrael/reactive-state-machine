#pragma once

#include "SimpleAotTest.h"
#include "test186_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Delayed send with params (W3C SCXML 6.2/5.10 AOT)
 *
 * Requires event scheduler polling for delayed send processing.
 */
struct Test186 : public ScheduledAotTest<Test186, 186> {
    static constexpr const char *DESCRIPTION = "Delayed send with params (W3C SCXML 6.2/5.10 AOT)";
    using SM = RSM::Generated::test186::test186;
};

// Auto-register
inline static AotTestRegistrar<Test186> registrar_Test186;

}  // namespace RSM::W3C::AotTests
