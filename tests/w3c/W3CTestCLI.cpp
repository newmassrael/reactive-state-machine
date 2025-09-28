#include "W3CTestRunner.h"
#include "common/Logger.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>

/**
 * @brief W3C SCXML Test CLI - executes all 207 test cases
 *
 * This implements the complete W3C SCXML 1.0 compliance testing using
 * SOLID architecture principles:
 * - Factory pattern for component creation
 * - Dependency injection for loose coupling
 * - Interface segregation for focused responsibilities
 */
int main(int argc, char *argv[]) {
    try {
        std::string resourcePath = "/home/coin/reactive-state-machine/resources";
        std::string outputPath = "w3c_test_results.xml";

        // Parse command line arguments
        std::vector<int> specificTestIds;  // empty means run all tests
        bool runUpToMode = false;          // true if using ~number format
        int upToTestId = 0;                // maximum test ID when using ~number

        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--resources" && i + 1 < argc) {
                resourcePath = argv[++i];
            } else if (arg == "--output" && i + 1 < argc) {
                outputPath = argv[++i];
            } else if (arg == "--help") {
                printf("Usage: %s [options]\n", argv[0]);
                printf("Options:\n");
                printf("  --resources PATH  Path to W3C test resources (default: %s)\n", resourcePath.c_str());
                printf("  --output FILE     XML output file (default: %s)\n", outputPath.c_str());
                printf("  ID1 ID2 ...       Run specific test IDs (e.g., 150 151 152)\n");
                printf("  ~NUMBER           Run all tests up to NUMBER (e.g., ~176 runs tests 150-176)\n");
                printf("  --help           Show this help message\n");
                return 0;
            } else if (arg.length() > 1 && arg[0] == '~') {
                // Handle ~number format for "run up to" functionality
                std::string numberStr = arg.substr(1);
                try {
                    upToTestId = std::stoi(numberStr);
                    runUpToMode = true;
                    LOG_INFO("W3C CLI: Run up to mode enabled - will run tests up to {}", upToTestId);
                } catch (const std::exception &) {
                    fprintf(stderr, "Invalid ~number format: %s\n", arg.c_str());
                    return 1;
                }
            } else {
                // Try to parse as test ID if it's a number
                try {
                    specificTestIds.push_back(std::stoi(arg));
                } catch (const std::exception &) {
                    fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
                    return 1;
                }
            }
        }

        // Verify resources exist
        if (!std::filesystem::exists(resourcePath)) {
            LOG_ERROR("W3C CLI: Test resources not found at: {}", resourcePath);
            RSM::Logger::error("W3C CLI: Make sure W3C tests are copied to the resources directory");
            return 1;
        }

        RSM::Logger::info("W3C CLI: Starting W3C SCXML 1.0 Compliance Test Suite");
        LOG_INFO("W3C CLI: Resources: {}", resourcePath);
        LOG_INFO("W3C CLI: Output: {}", outputPath);

        // SOLID Design: Create all components using factory pattern (Dependency Inversion)
        auto factory = std::make_unique<RSM::W3C::TestComponentFactory>();

        auto converter = factory->createConverter();
        auto metadataParser = factory->createMetadataParser();
        auto executor = factory->createExecutor();
        auto validator = factory->createValidator();
        auto testSuite = factory->createTestSuite(resourcePath);

        // Create composite reporter for both console and XML output
        auto consoleReporter = factory->createConsoleReporter();
        auto xmlReporter = factory->createXMLReporter(outputPath);
        auto reporter = factory->createCompositeReporter(std::move(consoleReporter), std::move(xmlReporter));

        // Dependency Injection: All dependencies are injected (Inversion of Control)
        RSM::W3C::W3CTestRunner runner(std::move(converter), std::move(metadataParser), std::move(executor),
                                       std::move(validator), std::move(testSuite), std::move(reporter));

        // Show test suite information
        auto testSuiteInfo = runner.getTestSuite()->getInfo();
        LOG_INFO("W3C CLI: Test Suite: {}", testSuiteInfo.name);
        LOG_INFO("W3C CLI: Description: {}", testSuiteInfo.description);
        LOG_INFO("W3C CLI: Total Tests: {}", testSuiteInfo.totalTests);

        // Execute W3C tests
        auto startTime = std::chrono::steady_clock::now();

        RSM::W3C::TestRunSummary summary;
        if (runUpToMode) {
            // Generate test IDs from 150 up to the specified number
            std::vector<int> upToTestIds;
            for (int testId = 150; testId <= upToTestId; testId++) {
                upToTestIds.push_back(testId);
            }

            LOG_INFO("W3C CLI: Running tests up to {} ({} tests: 150-{})", upToTestId, upToTestIds.size(), upToTestId);

            std::vector<RSM::W3C::TestReport> reports;
            for (int testId : upToTestIds) {
                try {
                    LOG_INFO("W3C CLI: Running test {}", testId);
                    RSM::W3C::TestReport report = runner.runSpecificTest(testId);
                    reports.push_back(report);

                    // Show result immediately
                    std::string status;
                    switch (report.validationResult.finalResult) {
                    case RSM::W3C::TestResult::PASS:
                        status = "PASS";
                        break;
                    case RSM::W3C::TestResult::FAIL:
                        status = "FAIL";
                        break;
                    case RSM::W3C::TestResult::ERROR:
                        status = "ERROR";
                        break;
                    case RSM::W3C::TestResult::TIMEOUT:
                        status = "TIMEOUT";
                        break;
                    }

                    LOG_INFO("W3C CLI: Test {} ({}): {} ({}ms)", report.testId, report.metadata.specnum, status,
                             report.executionContext.executionTime.count());
                    if (report.validationResult.finalResult != RSM::W3C::TestResult::PASS) {
                        LOG_INFO("W3C CLI: Failure reason: {}", report.validationResult.reason);
                    }

                } catch (const std::exception &e) {
                    LOG_ERROR("W3C CLI: Error running test {}: {}", testId, e.what());
                    // Continue with other tests instead of returning
                }
            }

            // Calculate summary for "up to" tests
            summary.totalTests = reports.size();
            for (const auto &report : reports) {
                switch (report.validationResult.finalResult) {
                case RSM::W3C::TestResult::PASS:
                    summary.passedTests++;
                    break;
                case RSM::W3C::TestResult::FAIL:
                    summary.failedTests++;
                    summary.failedTestIds.push_back(report.testId);
                    break;
                case RSM::W3C::TestResult::ERROR:
                case RSM::W3C::TestResult::TIMEOUT:
                    summary.errorTests++;
                    summary.errorTestIds.push_back(report.testId);
                    break;
                }
                summary.totalExecutionTime += report.executionContext.executionTime;
            }

            if (summary.totalTests > 0) {
                summary.passRate = (static_cast<double>(summary.passedTests) / summary.totalTests) * 100.0;
            }

        } else if (!specificTestIds.empty()) {
            LOG_INFO("W3C CLI: Running {} specific W3C tests", specificTestIds.size());

            std::vector<RSM::W3C::TestReport> reports;
            for (int testId : specificTestIds) {
                try {
                    LOG_INFO("W3C CLI: Running test {}", testId);
                    RSM::W3C::TestReport report = runner.runSpecificTest(testId);
                    reports.push_back(report);

                    // Show result immediately
                    std::string status;
                    switch (report.validationResult.finalResult) {
                    case RSM::W3C::TestResult::PASS:
                        status = "PASS";
                        break;
                    case RSM::W3C::TestResult::FAIL:
                        status = "FAIL";
                        break;
                    case RSM::W3C::TestResult::ERROR:
                        status = "ERROR";
                        break;
                    case RSM::W3C::TestResult::TIMEOUT:
                        status = "TIMEOUT";
                        break;
                    }

                    LOG_INFO("W3C CLI: Test {} ({}): {} ({}ms)", report.testId, report.metadata.specnum, status,
                             report.executionContext.executionTime.count());
                    if (report.validationResult.finalResult != RSM::W3C::TestResult::PASS) {
                        LOG_INFO("W3C CLI: Failure reason: {}", report.validationResult.reason);
                    }

                } catch (const std::exception &e) {
                    LOG_ERROR("W3C CLI: Error running test {}: {}", testId, e.what());
                    // Continue with other tests instead of returning
                }
            }

            // Calculate summary for specific tests
            summary.totalTests = reports.size();
            for (const auto &report : reports) {
                switch (report.validationResult.finalResult) {
                case RSM::W3C::TestResult::PASS:
                    summary.passedTests++;
                    break;
                case RSM::W3C::TestResult::FAIL:
                    summary.failedTests++;
                    summary.failedTestIds.push_back(report.testId);
                    break;
                case RSM::W3C::TestResult::ERROR:
                case RSM::W3C::TestResult::TIMEOUT:
                    summary.errorTests++;
                    summary.errorTestIds.push_back(report.testId);
                    break;
                }
                summary.totalExecutionTime += report.executionContext.executionTime;
            }

            if (summary.totalTests > 0) {
                summary.passRate = (static_cast<double>(summary.passedTests) / summary.totalTests) * 100.0;
            }

        } else {
            RSM::Logger::info("W3C CLI: Running all W3C SCXML compliance tests...");
            summary = runner.runAllTests();
        }

        auto endTime = std::chrono::steady_clock::now();
        auto totalTime = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);

        // Final results
        printf("\n");
        printf("🎉 W3C SCXML Compliance Test Complete!\n");
        printf("⏱️  Total execution time: %ld seconds\n", totalTime.count());
        printf("📊 Test Results Summary:\n");
        printf("   Total Tests: %zu\n", summary.totalTests);
        printf("   ✅ Passed: %zu\n", summary.passedTests);
        printf("   ❌ Failed: %zu\n", summary.failedTests);
        printf("   🚨 Errors: %zu\n", summary.errorTests);
        printf("   ⏭️  Skipped: %zu\n", summary.skippedTests);
        printf("   📈 Pass Rate: %.1f%%\n", summary.passRate);

        // Show specific failed test IDs
        if (!summary.failedTestIds.empty()) {
            printf("\n");
            printf("❌ Failed Tests: ");
            for (size_t i = 0; i < summary.failedTestIds.size(); ++i) {
                printf("%s", summary.failedTestIds[i].c_str());
                if (i < summary.failedTestIds.size() - 1) {
                    printf(", ");
                }
            }
            printf("\n");
        }

        // Show specific error test IDs
        if (!summary.errorTestIds.empty()) {
            printf("🚨 Error Tests: ");
            for (size_t i = 0; i < summary.errorTestIds.size(); ++i) {
                printf("%s", summary.errorTestIds[i].c_str());
                if (i < summary.errorTestIds.size() - 1) {
                    printf(", ");
                }
            }
            printf("\n");
        }

        if (summary.passRate >= 80.0) {
            printf("🏆 EXCELLENT: High compliance with W3C SCXML 1.0 specification!\n");
        } else if (summary.passRate >= 60.0) {
            printf("👍 GOOD: Reasonable compliance with W3C SCXML 1.0 specification\n");
        } else {
            printf("⚠️  NEEDS IMPROVEMENT: Consider reviewing failing tests\n");
        }

        printf("\n");
        printf("📊 Detailed results written to: %s\n", outputPath.c_str());

        // Return appropriate exit code
        return (summary.errorTests == 0 && summary.passRate > 0) ? 0 : 1;

    } catch (const std::exception &e) {
        fprintf(stderr, "❌ FATAL ERROR: %s\n", e.what());
        return 1;
    }
}