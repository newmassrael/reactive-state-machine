#pragma once

#include "SimpleAotTest.h"
#include "test152_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Foreach error handling (AOT JSEngine)
 */
struct Test152 : public SimpleAotTest<Test152, 152> {
    static constexpr const char *DESCRIPTION = "Foreach error handling (AOT JSEngine)";
    using SM = RSM::Generated::test152::test152;
};

// Auto-register
inline static AotTestRegistrar<Test152> registrar_Test152;

}  // namespace RSM::W3C::AotTests
