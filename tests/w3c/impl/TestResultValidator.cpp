#include "../interfaces/ITestResultValidator.h"
#include <algorithm>

namespace RSM::W3C {

/**
 * @brief W3C test result validator implementation
 */
class TestResultValidator : public ITestResultValidator {
public:
    TestResultValidator() = default;
    ~TestResultValidator() override = default;

    ValidationResult validateResult(const TestExecutionContext &context) override {
        // Skip manual tests
        if (context.metadata.manual) {
            return ValidationResult(true, TestResult::PASS, "Manual test skipped");
        }

        // Handle execution errors
        if (!context.errorMessage.empty()) {
            return ValidationResult(false, TestResult::ERROR, "Execution error: " + context.errorMessage);
        }

        // Handle timeouts
        if (context.executionTime > std::chrono::milliseconds(10000)) {
            return ValidationResult(false, TestResult::TIMEOUT, "Test execution timed out");
        }

        // Validate final state against expected target
        return validateFinalState(context);
    }

    bool shouldSkipTest(const TestMetadata &metadata) override {
        // Skip manual tests
        if (metadata.manual) {
            return true;
        }

        // Skip optional tests based on configuration
        // For now, run all mandatory and optional tests
        return false;
    }

private:
    ValidationResult validateFinalState(const TestExecutionContext &context) {
        const std::string &finalState = context.finalState;
        const std::string &expectedTarget = context.expectedTarget;

        if (expectedTarget == "unknown") {
            return ValidationResult(false, TestResult::ERROR, "Cannot determine expected test outcome");
        }

        // Direct state match
        if (finalState == expectedTarget) {
            if (expectedTarget == "pass") {
                return ValidationResult(true, TestResult::PASS, "Test reached expected pass state");
            } else {
                return ValidationResult(true, TestResult::FAIL, "Test reached expected fail state");
            }
        }

        // Inverse logic: if test should pass but reached fail, it's a failure
        if (expectedTarget == "pass" && finalState == "fail") {
            return ValidationResult(true, TestResult::FAIL, "Test should pass but reached fail state");
        }

        if (expectedTarget == "fail" && finalState == "pass") {
            return ValidationResult(true, TestResult::FAIL, "Test should fail but reached pass state");
        }

        // Unknown final state
        return ValidationResult(false, TestResult::ERROR, "Test ended in unknown state: " + finalState);
    }
};

}  // namespace RSM::W3C