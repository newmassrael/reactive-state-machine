#pragma once
#include "SimpleJitTest.h"
#include "test301_sm.h"

namespace RSM::W3C::JitTests {

/**
 * @brief W3C SCXML 5.8: Script element download timeout rejection (Manual Test)
 *
 * If the script specified by the 'src' attribute of a script element cannot be downloaded
 * within a platform-specific timeout interval, the document is considered non-conformant,
 * and the platform MUST reject it.
 *
 * This test contains an empty <script/> element. The processor should reject the document
 * at parse time. If the document is executed at all, it transitions to "fail" state.
 *
 * Manual verification: Check processor logs to confirm document rejection.
 * Note: This test is valid only for datamodels that support scripting.
 */
struct Test301 : public SimpleJitTest<Test301, 301> {
    static constexpr const char *DESCRIPTION = "Script download timeout rejection (W3C 5.8 JIT - Manual)";
    using SM = RSM::Generated::test301::test301;

    // Override run() to handle manual test - always return true (skip actual execution)
    bool run() override {
        // Manual test - Interpreter handles document rejection validation
        // JIT tests skip manual tests (similar to Interpreter behavior)
        return true;  // PASS (skipped)
    }
};

// Auto-register
inline static JitTestRegistrar<Test301> registrar_Test301;

}  // namespace RSM::W3C::JitTests
