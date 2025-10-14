#include "W3CTestRunner.h"
#include "common/Logger.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <set>

/**
 * @brief Find project root by searching for resources directory
 *
 * Searches upward from executable location to find the project root
 * containing the resources directory. This ensures the CLI works
 * regardless of where it's executed from.
 *
 * @param executablePath Absolute path to the executable
 * @return Path to resources directory, or empty string if not found
 */
std::string findResourcesPath(const std::string &executablePath) {
    namespace fs = std::filesystem;

    try {
        // Start from executable's directory
        fs::path currentPath = fs::path(executablePath).parent_path();

        // Search upward through parent directories (max 10 levels)
        for (int level = 0; level < 10; level++) {
            fs::path resourcesPath = currentPath / "resources";

            // Check if resources directory exists and has test directories
            if (fs::exists(resourcesPath) && fs::is_directory(resourcesPath)) {
                // Verify it's the correct resources by checking for a test directory
                bool hasTestDirs = false;
                for (const auto &entry : fs::directory_iterator(resourcesPath)) {
                    if (entry.is_directory()) {
                        hasTestDirs = true;
                        break;
                    }
                }

                if (hasTestDirs) {
                    LOG_DEBUG("W3C CLI: Found resources at: {}", resourcesPath.string());
                    return resourcesPath.string();
                }
            }

            // Move up one directory
            fs::path parentPath = currentPath.parent_path();
            if (parentPath == currentPath) {
                // Reached filesystem root
                break;
            }
            currentPath = parentPath;
        }
    } catch (const std::exception &e) {
        LOG_ERROR("W3C CLI: Error searching for resources: {}", e.what());
    }

    return "";  // Not found
}

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
        // Auto-detect resources path from executable location
        std::string executablePath;
        try {
            executablePath = std::filesystem::canonical("/proc/self/exe").string();
        } catch (...) {
            // Fallback for non-Linux systems or if /proc/self/exe is not available
            executablePath = argv[0];
        }

        std::string resourcePath = findResourcesPath(executablePath);
        std::string outputPath = "w3c_test_results.xml";

        // Validate resources path was found
        if (resourcePath.empty()) {
            LOG_ERROR("W3C CLI: Failed to locate resources directory");
            fprintf(stderr, "ERROR: Could not find W3C test resources directory.\n");
            fprintf(stderr, "       Searched upward from executable location: %s\n", executablePath.c_str());
            fprintf(stderr, "       Please ensure resources/ directory exists in project root.\n");
            fprintf(stderr, "       Or use --resources PATH to specify location manually.\n");
            return 1;
        }

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
        std::vector<RSM::W3C::TestReport> allReports;  // Store all reports for engine-specific stats
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
                    allReports.insert(allReports.end(), testReports.begin(), testReports.end());

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

            // Get all reports from reporter (includes dynamic and hybrid)
            allReports = runner.getReporter()->getAllReports();

            // Complete test run reporting
            runner.getReporter()->generateSummary(summary);
            runner.getReporter()->endTestRun();

        } else {
            RSM::Logger::info("W3C CLI: Running all W3C SCXML compliance tests...");

            // Run all tests with dynamic engine (skip reporting to avoid duplicate XML write)
            summary = runner.runAllTests(/* skipReporting = */ true);

            // Get all dynamic engine reports from reporter
            allReports = runner.getReporter()->getAllReports();

            // Extract all test IDs (including variants) from dynamic engine reports
            std::vector<std::string> allTestIds;
            for (const auto &report : allReports) {
                if (report.engineType == "dynamic") {
                    allTestIds.push_back(report.testId);
                }
            }

            // Run hybrid engine tests for all test IDs (including variants)
            LOG_INFO("W3C CLI: Running hybrid engine tests for all {} tests (including variants)", allTestIds.size());
            for (const std::string &testIdStr : allTestIds) {
                try {
                    // Extract numeric portion from testId (e.g., "403a" -> 403)
                    std::string numericPart;
                    for (char c : testIdStr) {
                        if (std::isdigit(c)) {
                            numericPart += c;
                        } else {
                            break;  // Stop at first non-digit
                        }
                    }

                    if (numericPart.empty()) {
                        continue;
                    }

                    int testId = std::stoi(numericPart);
                    RSM::W3C::TestReport hybridReport = runner.runHybridTest(testId);
                    // Preserve the original testId (with variant suffix if present)
                    hybridReport.testId = testIdStr;
                    allReports.push_back(hybridReport);
                    runner.getReporter()->reportTestResult(hybridReport);

                    // Update summary with hybrid results
                    summary.totalTests++;
                    switch (hybridReport.validationResult.finalResult) {
                    case RSM::W3C::TestResult::PASS:
                        summary.passedTests++;
                        break;
                    case RSM::W3C::TestResult::FAIL:
                        summary.failedTests++;
                        summary.failedTestIds.push_back(hybridReport.testId);
                        break;
                    case RSM::W3C::TestResult::ERROR:
                    case RSM::W3C::TestResult::TIMEOUT:
                        summary.errorTests++;
                        summary.errorTestIds.push_back(hybridReport.testId);
                        break;
                    }
                    summary.totalExecutionTime += hybridReport.executionContext.executionTime;
                } catch (const std::exception &e) {
                    LOG_ERROR("W3C CLI: Hybrid engine test {} failed: {}", testIdStr, e.what());
                    
                    // Create error report for failed hybrid test
                    RSM::W3C::TestReport errorReport;
                    errorReport.testId = testIdStr;
                    errorReport.engineType = "hybrid";
                    errorReport.validationResult.finalResult = RSM::W3C::TestResult::ERROR;
                    errorReport.validationResult.reason = std::string("Hybrid engine error: ") + e.what();
                    errorReport.executionContext.executionTime = std::chrono::milliseconds(0);
                    
                    allReports.push_back(errorReport);
                    runner.getReporter()->reportTestResult(errorReport);
                    
                    // Update summary
                    summary.totalTests++;
                    summary.errorTests++;
                    summary.errorTestIds.push_back(testIdStr);
                }
            }

            // Recalculate pass rate
            if (summary.totalTests > 0) {
                summary.passRate = (static_cast<double>(summary.passedTests) / summary.totalTests) * 100.0;
            }
            
            // Generate final report with both dynamic and hybrid results
            runner.getReporter()->generateSummary(summary);
            runner.getReporter()->endTestRun();
        }

        auto endTime = std::chrono::steady_clock::now();
        auto totalTime = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);

        // Get all reports from reporter (includes all test runs)
        if (allReports.empty()) {
            allReports = runner.getReporter()->getAllReports();
        }

        // Calculate engine-specific statistics from collected reports
        struct EngineStats {
            size_t total = 0;
            size_t passed = 0;
            size_t failed = 0;
            size_t errors = 0;
            std::vector<std::string> failedTestIds;
            std::vector<std::string> errorTestIds;
        };

        EngineStats dynamicStats, hybridStats;

        for (const auto &report : allReports) {
            EngineStats *engineStats = nullptr;
            if (report.engineType == "dynamic") {
                engineStats = &dynamicStats;
            } else if (report.engineType == "hybrid") {
                engineStats = &hybridStats;
            }

            if (engineStats) {
                engineStats->total++;
                switch (report.validationResult.finalResult) {
                case RSM::W3C::TestResult::PASS:
                    engineStats->passed++;
                    break;
                case RSM::W3C::TestResult::FAIL:
                    engineStats->failed++;
                    engineStats->failedTestIds.push_back(report.testId);
                    break;
                case RSM::W3C::TestResult::ERROR:
                case RSM::W3C::TestResult::TIMEOUT:
                    engineStats->errors++;
                    engineStats->errorTestIds.push_back(report.testId);
                    break;
                }
            }
        }

        bool hasEngineStats = !allReports.empty();

        // Final results
        printf("\n");
        printf("🎉 W3C SCXML Compliance Test Complete!\n");
        printf("⏱️  Total execution time: %ld seconds\n", totalTime.count());
        printf("📊 Test Results Summary:\n");

        // If we have engine stats, show table format
        if (hasEngineStats && summary.totalTests > 0) {
            printf("\n");
            printf("┌──────────────┬─────────┬────────┬────────┬────────┐\n");
            printf("│ Engine       │ Total   │ Passed │ Failed │ Errors │\n");
            printf("├──────────────┼─────────┼────────┼────────┼────────┤\n");
            printf("│ Dynamic      │ %-7zu │ %-6zu │ %-6zu │ %-6zu │\n", dynamicStats.total, dynamicStats.passed,
                   dynamicStats.failed, dynamicStats.errors);
            if (hybridStats.total > 0) {
                printf("│ Hybrid       │ %-7zu │ %-6zu │ %-6zu │ %-6zu │\n", hybridStats.total, hybridStats.passed,
                       hybridStats.failed, hybridStats.errors);
            }
            printf("├──────────────┼─────────┼────────┼────────┼────────┤\n");
            printf("│ Total        │ %-7zu │ %-6zu │ %-6zu │ %-6zu │\n", summary.totalTests, summary.passedTests,
                   summary.failedTests, summary.errorTests);
            printf("└──────────────┴─────────┴────────┴────────┴────────┘\n");
            printf("   📈 Pass Rate: %.1f%%\n", summary.passRate);
        } else {
            // Simple format for full test runs without engine breakdown
            printf("   Total Tests: %zu\n", summary.totalTests);
            printf("   ✅ Passed: %zu\n", summary.passedTests);
            printf("   ❌ Failed: %zu\n", summary.failedTests);
            printf("   🚨 Errors: %zu\n", summary.errorTests);
            printf("   ⏭️  Skipped: %zu\n", summary.skippedTests);
            printf("   📈 Pass Rate: %.1f%%\n", summary.passRate);
        }

        // Show specific failed test IDs by engine
        if (hasEngineStats) {
            // Show Dynamic engine failures
            if (!dynamicStats.failedTestIds.empty()) {
                printf("\n");
                printf("❌ Failed Tests (Dynamic): ");
                for (size_t i = 0; i < dynamicStats.failedTestIds.size(); ++i) {
                    printf("%s", dynamicStats.failedTestIds[i].c_str());
                    if (i < dynamicStats.failedTestIds.size() - 1) {
                        printf(", ");
                    }
                }
                printf("\n");
            }

            // Show Hybrid engine failures
            if (!hybridStats.failedTestIds.empty()) {
                printf("❌ Failed Tests (Hybrid): ");
                for (size_t i = 0; i < hybridStats.failedTestIds.size(); ++i) {
                    printf("%s", hybridStats.failedTestIds[i].c_str());
                    if (i < hybridStats.failedTestIds.size() - 1) {
                        printf(", ");
                    }
                }
                printf("\n");
            }

            // Show Dynamic engine errors
            if (!dynamicStats.errorTestIds.empty()) {
                printf("🚨 Error Tests (Dynamic): ");
                for (size_t i = 0; i < dynamicStats.errorTestIds.size(); ++i) {
                    printf("%s", dynamicStats.errorTestIds[i].c_str());
                    if (i < dynamicStats.errorTestIds.size() - 1) {
                        printf(", ");
                    }
                }
                printf("\n");
            }

            // Show Hybrid engine errors
            if (!hybridStats.errorTestIds.empty()) {
                printf("🚨 Error Tests (Hybrid): ");
                for (size_t i = 0; i < hybridStats.errorTestIds.size(); ++i) {
                    printf("%s", hybridStats.errorTestIds[i].c_str());
                    if (i < hybridStats.errorTestIds.size() - 1) {
                        printf(", ");
                    }
                }
                printf("\n");
            }
        } else {
            // Fallback for when we don't have engine-specific stats
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