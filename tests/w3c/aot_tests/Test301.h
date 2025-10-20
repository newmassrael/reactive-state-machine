#pragma once
#include "SimpleAotTest.h"
#include "test301_sm.h"

namespace RSM::W3C::AotTests {

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
struct Test301 : public SimpleAotTest<Test301, 301> {
    static constexpr const char *DESCRIPTION = "Script download timeout rejection (W3C 5.8 AOT - Manual)";
    using SM = RSM::Generated::test301::test301;

    // Override run() to handle manual test - always return true (skip actual execution)
    bool run() override {
        // Manual test - Interpreter handles document rejection validation
        // AOT tests skip manual tests (similar to Interpreter behavior)
        return true;  // PASS (skipped)
    }
};

// Auto-register
inline static AotTestRegistrar<Test301> registrar_Test301;

}  // namespace RSM::W3C::AotTests
