#pragma once

#include "SimpleAotTest.h"
#include "test150_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Foreach with dynamic variables (AOT JSEngine)
 */
struct Test150 : public SimpleAotTest<Test150, 150> {
    static constexpr const char *DESCRIPTION = "Foreach with dynamic variables (AOT JSEngine)";
    using SM = RSM::Generated::test150::test150;
};

// Auto-register
inline static AotTestRegistrar<Test150> registrar_Test150;

}  // namespace RSM::W3C::AotTests
