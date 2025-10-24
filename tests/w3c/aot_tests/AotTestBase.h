#pragma once

#include <chrono>
#include <string>

namespace RSM::W3C::AotTests {

/**
 * @brief Base interface for AOT engine tests
 *
 * All AOT tests inherit from this interface and implement the run() method.
 * Tests are automatically registered via REGISTER_AOT_TEST macro.
 */
class AotTestBase {
public:
    virtual ~AotTestBase() = default;

    /**
     * @brief Execute the AOT test
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

    /**
     * @brief Get test type: pure_static, static_hybrid, or interpreter_fallback
     * @return Test type string for XML reporting
     */
    virtual const char *getTestType() const {
        return "pure_static";  // Default for most tests
    }
};

}  // namespace RSM::W3C::AotTests
