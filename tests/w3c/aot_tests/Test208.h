#pragma once

#include "SimpleAotTest.h"
#include "test208_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Cancel delayed send by sendid (W3C 6.3 AOT)
 *
 * Requires event scheduler polling for delayed send processing.
 */
struct Test208 : public ScheduledAotTest<Test208, 208> {
    static constexpr const char *DESCRIPTION = "Cancel delayed send by sendid (W3C 6.3 AOT)";
    using SM = RSM::Generated::test208::test208;
};

// Auto-register
inline static AotTestRegistrar<Test208> registrar_Test208;

}  // namespace RSM::W3C::AotTests
