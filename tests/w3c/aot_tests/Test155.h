#pragma once

#include "SimpleAotTest.h"
#include "test155_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Foreach sums array items into variable (AOT JSEngine)
 */
struct Test155 : public SimpleAotTest<Test155, 155> {
    static constexpr const char *DESCRIPTION = "Foreach sums array items into variable (AOT JSEngine)";
    using SM = RSM::Generated::test155::test155;
};

// Auto-register
inline static AotTestRegistrar<Test155> registrar_Test155;

}  // namespace RSM::W3C::AotTests
