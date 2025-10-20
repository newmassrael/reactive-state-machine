#pragma once

#include "SimpleAotTest.h"
#include "test156_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief Foreach error handling stops loop (AOT JSEngine)
 */
struct Test156 : public SimpleAotTest<Test156, 156> {
    static constexpr const char *DESCRIPTION = "Foreach error handling stops loop (AOT JSEngine)";
    using SM = RSM::Generated::test156::test156;
};

// Auto-register
inline static AotTestRegistrar<Test156> registrar_Test156;

}  // namespace RSM::W3C::AotTests
