#pragma once

#include "ITestMetadataParser.h"
#include <chrono>
#include <memory>
#include <string>

namespace RSM::W3C {

/**
 * @brief Test execution result
 */
enum class TestResult {
    PASS,    // Test completed successfully, reached expected target
    FAIL,    // Test completed but reached wrong target
    ERROR,   // Test execution failed (parse error, runtime exception, etc.)
    TIMEOUT  // Test execution timed out
};

/**
 * @brief Test execution context - contains all execution details
 */
struct TestExecutionContext {
    std::string scxmlContent;
    TestMetadata metadata;
    std::string finalState;      // The state machine ended in
    std::string expectedTarget;  // Expected target (pass/fail)
    std::chrono::milliseconds executionTime{0};
    std::string errorMessage;  // If result == ERROR
};

/**
 * @brief Interface for executing SCXML tests
 *
 * Single Responsibility: Only executes individual tests
 * - Runs SCXML through RSM engine
 * - Captures execution results
 * - Handles timeouts and errors
 */
class ITestExecutor {
public:
    virtual ~ITestExecutor() = default;

    /**
     * @brief Execute a single SCXML test
     * @param scxml The SCXML content to execute
     * @param metadata Test metadata for context
     * @return Test execution result with full context
     */
    virtual TestExecutionContext executeTest(const std::string &scxml, const TestMetadata &metadata) = 0;

    /**
     * @brief Execute a single SCXML test with source file path for relative resolution
     * @param scxml The SCXML content to execute
     * @param metadata Test metadata for context
     * @param sourceFilePath Path to the original TXML/SCXML file for relative path resolution
     * @return Test execution result with full context
     */
    virtual TestExecutionContext executeTest(const std::string &scxml, const TestMetadata &metadata,
                                             const std::string &sourceFilePath) {
        // Default implementation calls the original method (for backward compatibility)
        LOG_DEBUG("ITestExecutor: executeTest called with sourceFilePath: '{}'", sourceFilePath);
        return executeTest(scxml, metadata);
    }

    /**
     * @brief Set execution timeout
     * @param timeoutMs Timeout in milliseconds (default: 5000ms)
     */
    virtual void setTimeout(std::chrono::milliseconds timeoutMs) = 0;
};

}  // namespace RSM::W3C