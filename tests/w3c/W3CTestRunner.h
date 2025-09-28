#pragma once

#include "interfaces/ITestConverter.h"
#include "interfaces/ITestExecutor.h"
#include "interfaces/ITestMetadataParser.h"
#include "interfaces/ITestReporter.h"
#include "interfaces/ITestResultValidator.h"
#include "interfaces/ITestSuite.h"
#include <memory>
#include <vector>

namespace RSM::W3C {

/**
 * @brief Factory for creating W3C test components
 *
 * Dependency Inversion: Creates concrete implementations through interfaces
 * Single Responsibility: Only responsible for component creation
 */
class TestComponentFactory {
public:
    static std::unique_ptr<ITestConverter> createConverter();
    static std::unique_ptr<ITestMetadataParser> createMetadataParser();
    static std::unique_ptr<ITestExecutor> createExecutor();
    static std::unique_ptr<ITestResultValidator> createValidator();
    static std::unique_ptr<ITestSuite> createTestSuite(const std::string &resourcePath = "resources");
    static std::unique_ptr<ITestReporter> createConsoleReporter();
    static std::unique_ptr<ITestReporter> createXMLReporter(const std::string &outputPath);
    static std::unique_ptr<ITestReporter> createCompositeReporter(std::unique_ptr<ITestReporter> consoleReporter,
                                                                  std::unique_ptr<ITestReporter> xmlReporter);
};

/**
 * @brief Main W3C test runner orchestrator
 *
 * Single Responsibility: Only orchestrates the testing process
 * Dependency Inversion: Depends on interfaces, not concrete implementations
 */
class W3CTestRunner {
private:
    std::unique_ptr<ITestConverter> converter_;
    std::unique_ptr<ITestMetadataParser> metadataParser_;
    std::unique_ptr<ITestExecutor> executor_;
    std::unique_ptr<ITestResultValidator> validator_;
    std::unique_ptr<ITestSuite> testSuite_;
    std::unique_ptr<ITestReporter> reporter_;

    /**
     * @brief Run a single test
     */
    TestReport runSingleTest(const std::string &testDirectory);

    /**
     * @brief Calculate test run statistics
     */
    TestRunSummary calculateSummary(const std::vector<TestReport> &reports);

public:
    /**
     * @brief Constructor with dependency injection
     */
    W3CTestRunner(std::unique_ptr<ITestConverter> converter, std::unique_ptr<ITestMetadataParser> metadataParser,
                  std::unique_ptr<ITestExecutor> executor, std::unique_ptr<ITestResultValidator> validator,
                  std::unique_ptr<ITestSuite> testSuite, std::unique_ptr<ITestReporter> reporter);

    ~W3CTestRunner() = default;

    /**
     * @brief Run all W3C tests
     * @return Test run summary
     */
    TestRunSummary runAllTests();

    /**
     * @brief Run specific test by ID
     * @param testId W3C test ID (e.g., 144)
     * @return Test report
     */
    TestReport runSpecificTest(int testId);

    /**
     * @brief Run filtered tests
     * @param conformanceLevel Filter by conformance level
     * @param specSection Filter by spec section
     * @return Test run summary
     */
    TestRunSummary runFilteredTests(const std::string &conformanceLevel = "", const std::string &specSection = "");

    /**
     * @brief Get test suite for accessing information
     * @return Pointer to test suite interface
     */
    ITestSuite *getTestSuite() const {
        return testSuite_.get();
    }
};

}  // namespace RSM::W3C