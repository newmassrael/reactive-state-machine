#pragma once

#include "ITestExecutor.h"
#include "ITestMetadataParser.h"
#include "ITestResultValidator.h"
#include <chrono>
#include <string>
#include <vector>

namespace RSM::W3C {

/**
 * @brief Test run summary statistics
 */
struct TestRunSummary {
    size_t totalTests{0};
    size_t passedTests{0};
    size_t failedTests{0};
    size_t errorTests{0};
    size_t skippedTests{0};
    std::chrono::milliseconds totalExecutionTime{0};
    double passRate{0.0};                    // Percentage of passed tests
    std::vector<std::string> failedTestIds;  // IDs of failed tests
    std::vector<std::string> errorTestIds;   // IDs of tests with errors
};

/**
 * @brief Individual test report entry
 */
struct TestReport {
    std::string testId;
    std::string engineType;  // "interpreter" or "aot" - indicates which engine executed the test
    std::string testType;    // "pure_static", "static_hybrid", or "interpreter_fallback" - AOT implementation type
    TestMetadata metadata;
    TestExecutionContext executionContext;
    ValidationResult validationResult;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Interface for reporting test results
 *
 * Single Responsibility: Only handles result reporting and formatting
 * - Collects test results
 * - Generates reports in different formats
 * - Provides summary statistics
 *
 * Strategy Pattern: Different reporters for different output formats
 */
class ITestReporter {
public:
    virtual ~ITestReporter() = default;

    /**
     * @brief Report result of a single test
     * @param report Complete test report
     */
    virtual void reportTestResult(const TestReport &report) = 0;

    /**
     * @brief Generate final summary report
     * @param summary Test run statistics
     */
    virtual void generateSummary(const TestRunSummary &summary) = 0;

    /**
     * @brief Initialize reporter for a new test run
     * @param testSuiteName Name of the test suite being run
     */
    virtual void beginTestRun(const std::string &testSuiteName) = 0;

    /**
     * @brief Finalize reporter after test run completion
     */
    virtual void endTestRun() = 0;

    /**
     * @brief Get output destination (file path, console, etc.)
     * @return Description of where results are being written
     */
    virtual std::string getOutputDestination() const = 0;

    /**
     * @brief Get all collected test reports (optional, returns empty if not supported)
     * @return Vector of all test reports collected during the test run
     */
    virtual std::vector<TestReport> getAllReports() const {
        return {};
    }
};

;

}  // namespace RSM::W3C