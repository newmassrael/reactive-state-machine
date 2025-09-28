#pragma once

#include "ITestExecutor.h"
#include <string>

namespace RSM::W3C {

/**
 * @brief W3C test validation result
 */
struct ValidationResult {
    bool isValid{false};
    TestResult finalResult{TestResult::ERROR};
    std::string reason;  // Explanation of validation logic

    // Default constructor
    ValidationResult() = default;

    ValidationResult(bool valid, TestResult result, const std::string &explanation)
        : isValid(valid), finalResult(result), reason(explanation) {}
};

/**
 * @brief Interface for validating test execution results
 *
 * Single Responsibility: Only validates test outcomes
 * - Interprets final states against expected targets
 * - Handles W3C-specific validation rules
 * - Provides clear validation reasoning
 */
class ITestResultValidator {
public:
    virtual ~ITestResultValidator() = default;

    /**
     * @brief Validate test execution result against expected outcome
     * @param context Complete test execution context
     * @return Validation result with explanation
     */
    virtual ValidationResult validateResult(const TestExecutionContext &context) = 0;

    /**
     * @brief Check if a test should be skipped (e.g., manual tests)
     * @param metadata Test metadata
     * @return true if test should be skipped
     */
    virtual bool shouldSkipTest(const TestMetadata &metadata) = 0;
};

}  // namespace RSM::W3C