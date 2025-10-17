#pragma once

#include "SimpleJitTest.h"
#include "test159_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Error in executable content stops subsequent elements (JIT)
 */
struct Test159 : public SimpleJitTest<Test159, 159> {
    static constexpr const char *DESCRIPTION = "Error in executable content stops subsequent elements (JIT)";
    using SM = RSM::Generated::test159::test159;
};

// Auto-register
inline static JitTestRegistrar<Test159> registrar_Test159;

}  // namespace RSM::W3C::JitTests
