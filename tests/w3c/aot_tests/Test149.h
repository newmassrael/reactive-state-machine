#pragma once

#include "SimpleAotTest.h"
#include "test149_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Neither if nor elseif executes
 */
struct Test149 : public SimpleAotTest<Test149, 149> {
    static constexpr const char *DESCRIPTION = "Neither if nor elseif executes";
    using SM = RSM::Generated::test149::test149;
};

// Auto-register
inline static AotTestRegistrar<Test149> registrar_Test149;

}  // namespace RSM::W3C::AotTests
