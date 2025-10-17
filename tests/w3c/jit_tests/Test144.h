#pragma once

#include "SimpleJitTest.h"
#include "test144_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 3.5.1: Event queue ordering
 *
 * Tests that events are processed in document order.
 */
struct Test144 : public SimpleJitTest<Test144, 144> {
    static constexpr const char *DESCRIPTION = "Event queue ordering";
    using SM = RSM::Generated::test144::test144;
};

// Auto-register
inline static JitTestRegistrar<Test144> registrar_Test144;

}  // namespace RSM::W3C::JitTests
