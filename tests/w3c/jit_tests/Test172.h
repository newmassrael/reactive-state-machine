#pragma once

#include "SimpleJitTest.h"
#include "test172_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief Send eventexpr uses current datamodel value (JIT)
 */
struct Test172 : public SimpleJitTest<Test172, 172> {
    static constexpr const char *DESCRIPTION = "Send eventexpr uses current datamodel value (JIT)";
    using SM = RSM::Generated::test172::test172;
};

// Auto-register
inline static JitTestRegistrar<Test172> registrar_Test172;

}  // namespace RSM::W3C::JitTests
