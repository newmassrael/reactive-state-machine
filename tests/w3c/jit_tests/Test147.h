#pragma once

#include "SimpleJitTest.h"
#include "test147_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief If/elseif/else conditionals with datamodel
 */
struct Test147 : public SimpleJitTest<Test147, 147> {
    static constexpr const char *DESCRIPTION = "If/elseif/else conditionals with datamodel";
    using SM = RSM::Generated::test147::test147;
};

// Auto-register
inline static JitTestRegistrar<Test147> registrar_Test147;

}  // namespace RSM::W3C::JitTests
