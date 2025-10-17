#pragma once

#include "SimpleJitTest.h"
#include "test155_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Foreach sums array items into variable (JIT JSEngine)
 */
struct Test155 : public SimpleJitTest<Test155, 155> {
    static constexpr const char *DESCRIPTION = "Foreach sums array items into variable (JIT JSEngine)";
    using SM = RSM::Generated::test155::test155;
};

// Auto-register
inline static JitTestRegistrar<Test155> registrar_Test155;

}  // namespace RSM::W3C::JitTests
