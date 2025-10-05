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
        // Use relative path from build/tests directory: ../../resources
        std::string resourcePath = "../../resources";
        std::string outputPath = "w3c_test_results.xml";

        // Parse command line arguments
        std::vector<std::string> specificTestIds;  // empty means run all tests (supports both "403" and "403a")
        bool runUpToMode = false;                  // true if using ~number format
        int upToTestId = 0;                        // maximum test ID when using ~number

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
                printf("  START~END         Run tests in range START to END (e.g., 100~200)\n");
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
                // Check if it's a range format: START~END
                size_t tildePos = arg.find('~');
                if (tildePos != std::string::npos && tildePos > 0) {
                    // Parse START~END range
                    std::string startStr = arg.substr(0, tildePos);
                    std::string endStr = arg.substr(tildePos + 1);
                    try {
                        int startId = std::stoi(startStr);
                        int endId = std::stoi(endStr);

                        if (startId > endId) {
                            fprintf(stderr, "Invalid range: start (%d) must be <= end (%d)\n", startId, endId);
                            return 1;
                        }

                        // Add all test IDs in the range
                        for (int testId = startId; testId <= endId; testId++) {
                            specificTestIds.push_back(std::to_string(testId));
                        }

                        LOG_INFO("W3C CLI: Range mode enabled - will run tests {}-{} ({} tests)", startId, endId,
                                 endId - startId + 1);
                    } catch (const std::exception &) {
                        fprintf(stderr, "Invalid range format: %s (expected START~END)\n", arg.c_str());
                        return 1;
                    }
                } else {
                    // Store test ID as string (supports both numeric "403" and variants "403a")
                    specificTestIds.push_back(arg);
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

            // Begin test run for consistent reporting
            auto testSuiteInfo = runner.getTestSuite()->getInfo();
            runner.getReporter()->beginTestRun(testSuiteInfo.name + " (Up To Tests)");

            std::vector<RSM::W3C::TestReport> reports;
            for (int testId : upToTestIds) {
                try {
                    LOG_INFO("W3C CLI: Running test {} (including variants if any)", testId);
                    std::vector<RSM::W3C::TestReport> testReports = runner.runAllMatchingTests(testId);
                    reports.insert(reports.end(), testReports.begin(), testReports.end());

                    // Show results for all variants
                    for (const auto &report : testReports) {
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
                    }

                } catch (const std::exception &e) {
                    std::string errorMsg = e.what();
                    // Test not found is normal for sparse test IDs - log as debug instead of error
                    if (errorMsg.find("not found") != std::string::npos) {
                        LOG_DEBUG("W3C CLI: Test {} not found (skipped)", testId);
                    } else {
                        LOG_ERROR("W3C CLI: Error running test {}: {}", testId, errorMsg);
                    }
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

            // Complete test run reporting
            runner.getReporter()->generateSummary(summary);
            runner.getReporter()->endTestRun();

        } else if (!specificTestIds.empty()) {
            LOG_INFO("W3C CLI: Running {} specific W3C tests", specificTestIds.size());

            // Begin test run for consistent reporting
            auto testSuiteInfo = runner.getTestSuite()->getInfo();
            runner.getReporter()->beginTestRun(testSuiteInfo.name + " (Specific Tests)");

            std::vector<RSM::W3C::TestReport> reports;
            for (const std::string &testId : specificTestIds) {
                try {
                    // Check if testId is purely numeric or has variant suffix (e.g., "403a")
                    bool isNumeric = true;
                    for (char c : testId) {
                        if (!std::isdigit(c)) {
                            isNumeric = false;
                            break;
                        }
                    }

                    std::vector<RSM::W3C::TestReport> testReports;

                    if (isNumeric) {
                        // Numeric test ID - run all variants (e.g., "403" runs 403a, 403b, 403c)
                        LOG_INFO("W3C CLI: Running test {} (including all variants)", testId);
                        testReports = runner.runAllMatchingTests(std::stoi(testId));
                    } else {
                        // Test ID with variant suffix - run exact match only (e.g., "403a" runs only test403a.scxml)
                        LOG_INFO("W3C CLI: Running exact test {}", testId);
                        RSM::W3C::TestReport report = runner.runTest(testId);
                        testReports.push_back(report);
                    }

                    reports.insert(reports.end(), testReports.begin(), testReports.end());

                    // Show results for all variants
                    for (const auto &report : testReports) {
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
                    }

                } catch (const std::exception &e) {
                    std::string errorMsg = e.what();
                    // Test not found is normal for sparse test IDs - log as debug instead of error
                    if (errorMsg.find("not found") != std::string::npos) {
                        LOG_DEBUG("W3C CLI: Test {} not found (skipped)", testId);
                    } else {
                        LOG_ERROR("W3C CLI: Error running test {}: {}", testId, errorMsg);
                    }
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

            // Complete test run reporting
            runner.getReporter()->generateSummary(summary);
            runner.getReporter()->endTestRun();

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