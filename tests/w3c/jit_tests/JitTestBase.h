#pragma once

#include <chrono>
#include <string>

namespace RSM::W3C::JitTests {

/**
 * @brief Base interface for JIT engine tests
 *
 * All JIT tests inherit from this interface and implement the run() method.
 * Tests are automatically registered via REGISTER_JIT_TEST macro.
 */
class JitTestBase {
public:
    virtual ~JitTestBase() = default;

    /**
     * @brief Execute the JIT test
     * @return true if test passed, false otherwise
     */
    virtual bool run() = 0;

    /**
     * @brief Get test ID
     * @return Test number (e.g., 144, 147)
     */
    virtual int getTestId() const = 0;

    /**
     * @brief Get test description
     * @return Human-readable test description
     */
    virtual const char *getDescription() const = 0;

    /**
     * @brief Get timeout duration for this test
     * @return Timeout in seconds (default: 2 seconds)
     */
    virtual std::chrono::seconds getTimeout() const {
        return std::chrono::seconds(2);
    }

    /**
     * @brief Check if test requires event scheduler polling
     * @return true if test uses delayed send/invoke
     */
    virtual bool needsSchedulerPolling() const {
        return false;
    }
};

}  // namespace RSM::W3C::JitTests
