#pragma once

#include "SimpleAotTest.h"
#include "test153_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Foreach array iteration order (AOT JSEngine)
 */
struct Test153 : public SimpleAotTest<Test153, 153> {
    static constexpr const char *DESCRIPTION = "Foreach array iteration order (AOT JSEngine)";
    using SM = RSM::Generated::test153::test153;
};

// Auto-register
inline static AotTestRegistrar<Test153> registrar_Test153;

}  // namespace RSM::W3C::AotTests
