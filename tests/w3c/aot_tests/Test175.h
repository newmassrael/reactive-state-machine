#pragma once

#include "SimpleAotTest.h"
#include "test175_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 6.2: Send delayexpr uses current datamodel value
 *
 * Tests delayed send with expression-based delay calculation.
 * Requires event scheduler polling.
 */
struct Test175 : public ScheduledAotTest<Test175, 175> {
    static constexpr const char *DESCRIPTION = "Send delayexpr uses current datamodel value (AOT)";
    using SM = RSM::Generated::test175::test175;
};

// Auto-register
inline static AotTestRegistrar<Test175> registrar_Test175;

}  // namespace RSM::W3C::AotTests
