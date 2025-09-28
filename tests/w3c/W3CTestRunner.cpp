#include "W3CTestRunner.h"
#include "W3CHttpTestServer.h"
#include "common/Logger.h"
#include "events/EventDispatcherImpl.h"
#include "events/EventSchedulerImpl.h"
#include "events/EventTargetFactoryImpl.h"
#include "impl/TXMLConverter.h"
#include "impl/TestMetadataParser.h"
#include "runtime/EventRaiserImpl.h"
#include "runtime/StateMachine.h"
#include "runtime/StateMachineBuilder.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace RSM::W3C {

// Forward declaration for implementations
class TestResultValidator;
class W3CTestSuite;

class ConsoleTestReporter : public ITestReporter {
private:
    size_t testCount_ = 0;

public:
    void beginTestRun(const std::string &testSuiteName) override {
        LOG_INFO("=== {} ===", testSuiteName);
        testCount_ = 0;
    }

    void reportTestResult(const TestReport &report) override {
        ++testCount_;

        std::string status;
        switch (report.validationResult.finalResult) {
        case TestResult::PASS:
            status = "PASS";
            break;
        case TestResult::FAIL:
            status = "FAIL";
            break;
        case TestResult::ERROR:
            status = "ERROR";
            break;
        case TestResult::TIMEOUT:
            status = "TIMEOUT";
            break;
        }

        LOG_INFO("[{}] Test {} ({}): {}", testCount_, report.testId, report.metadata.specnum, status);

        if (report.validationResult.finalResult != TestResult::PASS) {
            LOG_INFO(" - {}", report.validationResult.reason);
        }

        LOG_INFO(" ({}ms)", report.executionContext.executionTime.count());
    }

    void generateSummary([[maybe_unused]] const TestRunSummary &summary) override {
        // Do not output summary in console reporter - let main runner handle it
    }

    void endTestRun() override {
        // Do not output end message in console reporter - let main runner handle it
    }

    std::string getOutputDestination() const override {
        return "Console";
    }
};

// Factory implementations
std::unique_ptr<ITestConverter> TestComponentFactory::createConverter() {
    return std::make_unique<TXMLConverter>();
}

std::unique_ptr<ITestMetadataParser> TestComponentFactory::createMetadataParser() {
    return std::make_unique<TestMetadataParser>();
}

std::unique_ptr<ITestExecutor> TestComponentFactory::createExecutor() {
    // W3C SCXML compliance: Use real StateMachine with full invoke support
    class StateMachineTestExecutor : public ITestExecutor {
    private:
        std::chrono::milliseconds timeout_{2000};

    public:
        void setTimeout(std::chrono::milliseconds timeoutMs) override {
            timeout_ = timeoutMs;
        }

        TestExecutionContext executeTest(const std::string &scxmlContent, const TestMetadata &metadata) override {
            auto startTime = std::chrono::steady_clock::now();

            TestExecutionContext context;
            context.scxmlContent = scxmlContent;
            context.metadata = metadata;
            // W3C tests should pass when correctly implemented, regardless of conformance level
            // conformance indicates whether implementation is required, not expected outcome
            context.expectedTarget = "pass";

            try {
                LOG_DEBUG("StateMachineTestExecutor: Starting test execution for test {}", metadata.id);

                // Create EventDispatcher infrastructure for delayed events and #_parent routing
                auto eventRaiser = std::make_shared<RSM::EventRaiserImpl>();
                auto targetFactory = std::make_shared<RSM::EventTargetFactoryImpl>(eventRaiser);

                // Create StateMachine using Builder pattern with proper dependency injection
                auto builder = RSM::StateMachineBuilder().withEventDispatcher(std::make_shared<
                                                                              RSM::EventDispatcherImpl>(
                    std::make_shared<RSM::EventSchedulerImpl>([](const RSM::EventDescriptor &event,
                                                                 std::shared_ptr<RSM::IEventTarget> target,
                                                                 const std::string &sendId) -> bool {
                        LOG_DEBUG(
                            "EventScheduler: Executing event '{}' on target '{}' with sendId '{}', event.target='{}'",
                            event.eventName, target->getDebugInfo(), sendId, event.target);

                        // Check if target can handle this event
                        if (!target->canHandle(event.target)) {
                            LOG_WARN("EventScheduler: Target '{}' cannot handle target '{}'", target->getDebugInfo(),
                                     event.target);
                            return false;
                        }

                        // W3C SCXML 6.2: Send event to the target using sendId for tracking
                        LOG_DEBUG("EventScheduler: Calling target->send() for event '{}' on target '{}'",
                                  event.eventName, target->getDebugInfo());
                        auto future = target->send(event);
                        LOG_DEBUG("EventScheduler: target->send() completed for event '{}'", event.eventName);
                        bool targetResult = false;
                        try {
                            auto sendResult = future.get();
                            targetResult = sendResult.isSuccess;
                        } catch (const std::exception &e) {
                            LOG_ERROR("EventScheduler: Failed to send event '{}': {}", event.eventName, e.what());
                            targetResult = false;
                        }

                        return targetResult;
                    }),
                    targetFactory));

                // Inject EventRaiser into builder for proper dependency injection
                builder.withEventRaiser(eventRaiser);

                auto stateMachine = builder.build();

                // W3C SCXML compliance: EventRaiser callback should pass eventData to StateMachine
                // StateMachine will automatically set its own callback, so we don't override it here
                // eventRaiser->setEventCallback(...); // Removed - let StateMachine set its own callback

                // Load SCXML content
                if (!stateMachine->loadSCXMLFromString(scxmlContent)) {
                    LOG_ERROR("StateMachineTestExecutor: Failed to load SCXML content");
                    context.finalState = "error";
                    context.errorMessage = "Failed to load SCXML content";
                    return context;
                }

                // W3C SCXML compliance: Ensure EventRaiser callback is properly set after SCXML loading
                // This guarantees that any initialization during SCXML loading doesn't override our callback
                stateMachine->setEventRaiser(eventRaiser);

                // Start the state machine
                if (!stateMachine->start()) {
                    LOG_ERROR("StateMachineTestExecutor: Failed to start StateMachine");
                    context.finalState = "error";
                    context.errorMessage = "Failed to start StateMachine";
                    return context;
                }

                // Wait for StateMachine to reach final state or timeout
                auto waitStart = std::chrono::steady_clock::now();
                std::string currentState;

                while (std::chrono::steady_clock::now() - waitStart < timeout_) {
                    currentState = stateMachine->getCurrentState();

                    // Check if we reached a final state (pass or fail)
                    if (currentState == "pass" || currentState == "fail") {
                        LOG_DEBUG("StateMachineTestExecutor: Reached final state: {}", currentState);
                        break;
                    }

                    // Small sleep to avoid busy waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                // Get final state
                context.finalState = currentState;
                LOG_DEBUG("StateMachineTestExecutor: Test completed with final state: {}", context.finalState);

                auto endTime = std::chrono::steady_clock::now();
                context.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

                return context;

            } catch (const std::exception &e) {
                LOG_ERROR("StateMachineTestExecutor: Exception during test execution: {}", e.what());
                context.finalState = "error";
                context.errorMessage = "Exception: " + std::string(e.what());

                auto endTime = std::chrono::steady_clock::now();
                context.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

                return context;
            }
        }
    };

    return std::make_unique<StateMachineTestExecutor>();
}

std::unique_ptr<ITestResultValidator> TestComponentFactory::createValidator() {
    // Include implementation from TestResultValidator.cpp
    class TestResultValidator : public ITestResultValidator {
    public:
        ValidationResult validateResult(const TestExecutionContext &context) override {
            if (context.metadata.manual) {
                return ValidationResult(true, TestResult::PASS, "Manual test skipped");
            }

            if (!context.errorMessage.empty()) {
                return ValidationResult(false, TestResult::ERROR, "Execution error: " + context.errorMessage);
            }

            if (context.executionTime > std::chrono::milliseconds(10000)) {
                return ValidationResult(false, TestResult::TIMEOUT, "Test execution timed out");
            }

            return validateFinalState(context);
        }

        bool shouldSkipTest(const TestMetadata &metadata) override {
            return metadata.manual;
        }

    private:
        ValidationResult validateFinalState(const TestExecutionContext &context) {
            const std::string &finalState = context.finalState;
            const std::string &expectedTarget = context.expectedTarget;

            if (expectedTarget == "unknown") {
                return ValidationResult(false, TestResult::ERROR, "Cannot determine expected test outcome");
            }

            if (finalState == expectedTarget) {
                if (expectedTarget == "pass") {
                    return ValidationResult(true, TestResult::PASS, "Test reached expected pass state");
                } else {
                    return ValidationResult(true, TestResult::FAIL, "Test reached expected fail state");
                }
            }

            if (expectedTarget == "pass" && finalState == "fail") {
                return ValidationResult(true, TestResult::FAIL, "Test should pass but reached fail state");
            }

            if (expectedTarget == "fail" && finalState == "pass") {
                return ValidationResult(true, TestResult::FAIL, "Test should fail but reached pass state");
            }

            return ValidationResult(false, TestResult::ERROR, "Test ended in unknown state: " + finalState);
        }
    };

    return std::make_unique<TestResultValidator>();
}

std::unique_ptr<ITestSuite> TestComponentFactory::createTestSuite(const std::string &resourcePath) {
    // Include implementation from W3CTestSuite.cpp
    class W3CTestSuite : public ITestSuite {
    private:
        std::string resourcePath_;

        bool isNumericTestDir(const std::string &dirName) {
            return !dirName.empty() && std::all_of(dirName.begin(), dirName.end(), ::isdigit);
        }

        static int extractTestId(const std::string &testPath) {
            std::filesystem::path path(testPath);
            std::string dirName = path.filename().string();
            try {
                return std::stoi(dirName);
            } catch (...) {
                return 0;
            }
        }

    public:
        explicit W3CTestSuite(const std::string &resourcePath) : resourcePath_(resourcePath) {}

        TestSuiteInfo getInfo() const override {
            TestSuiteInfo info;
            info.name = "W3C SCXML Test Suite";
            info.description = "Official W3C SCXML 1.0 Conformance Tests";
            info.resourcePath = resourcePath_;

            try {
                auto tests = const_cast<W3CTestSuite *>(this)->discoverTests();
                info.totalTests = tests.size();
            } catch (...) {
                info.totalTests = 0;
            }

            return info;
        }

        std::vector<std::string> discoverTests() override {
            std::vector<std::string> testDirs;

            try {
                for (const auto &entry : std::filesystem::directory_iterator(resourcePath_)) {
                    if (entry.is_directory()) {
                        std::string dirName = entry.path().filename().string();

                        if (isNumericTestDir(dirName)) {
                            std::string txmlPath = getTXMLPath(entry.path().string());
                            std::string metadataPath = getMetadataPath(entry.path().string());

                            if (std::filesystem::exists(txmlPath)) {
                                testDirs.push_back(entry.path().string());
                            }
                        }
                    }
                }
            } catch (const std::exception &e) {
                throw std::runtime_error("Failed to discover W3C tests: " + std::string(e.what()));
            }

            std::sort(testDirs.begin(), testDirs.end(),
                      [](const std::string &a, const std::string &b) { return extractTestId(a) < extractTestId(b); });

            return testDirs;
        }

        std::string getTXMLPath(const std::string &testDirectory) override {
            int testId = extractTestId(testDirectory);
            return testDirectory + "/test" + std::to_string(testId) + ".txml";
        }

        std::string getMetadataPath(const std::string &testDirectory) override {
            return testDirectory + "/metadata.txt";
        }

        std::vector<std::string> filterTests(const std::string &conformanceLevel,
                                             const std::string &specSection) override {
            auto allTests = discoverTests();
            std::vector<std::string> filteredTests;

            // Single Responsibility: Filter based on conformance level and spec section
            for (const auto &testDir : allTests) {
                try {
                    std::string metadataPath = getMetadataPath(testDir);
                    if (!std::filesystem::exists(metadataPath)) {
                        continue;
                    }

                    std::ifstream metadataFile(metadataPath);
                    std::string line;
                    std::string testConformance, testSpec;

                    while (std::getline(metadataFile, line)) {
                        if (line.find("conformance=") == 0) {
                            testConformance = line.substr(12);
                        } else if (line.find("specnum=") == 0) {
                            testSpec = line.substr(8);
                        }
                    }

                    bool matchesConformance =
                        conformanceLevel.empty() || testConformance.find(conformanceLevel) != std::string::npos;
                    bool matchesSpec = specSection.empty() || testSpec.find(specSection) != std::string::npos;

                    if (matchesConformance && matchesSpec) {
                        filteredTests.push_back(testDir);
                    }

                } catch (const std::exception &) {
                    // Skip tests with metadata parsing errors
                    continue;
                }
            }

            return filteredTests;
        }
    };

    return std::make_unique<W3CTestSuite>(resourcePath);
}

std::unique_ptr<ITestReporter> TestComponentFactory::createConsoleReporter() {
    class ConsoleTestReporter : public ITestReporter {
    private:
        size_t testCount_ = 0;

    public:
        void beginTestRun(const std::string &testSuiteName) override {
            LOG_INFO("=== {} ===", testSuiteName);
            testCount_ = 0;
        }

        void reportTestResult(const TestReport &report) override {
            ++testCount_;

            std::string status;
            switch (report.validationResult.finalResult) {
            case TestResult::PASS:
                status = "PASS";
                break;
            case TestResult::FAIL:
                status = "FAIL";
                break;
            case TestResult::ERROR:
                status = "ERROR";
                break;
            case TestResult::TIMEOUT:
                status = "TIMEOUT";
                break;
            }

            LOG_INFO("[{}] Test {} ({}): {}", testCount_, report.testId, report.metadata.specnum, status);

            if (report.validationResult.finalResult != TestResult::PASS) {
                LOG_INFO(" - {}", report.validationResult.reason);
            }

            LOG_INFO(" ({}ms)", report.executionContext.executionTime.count());
        }

        void generateSummary(const TestRunSummary &summary) override {
            LOG_INFO("\n=== Test Results Summary ===");
            LOG_INFO("Total tests: {}", summary.totalTests);
            LOG_INFO("Passed: {}", summary.passedTests);
            LOG_INFO("Failed: {}", summary.failedTests);
            LOG_INFO("Errors: {}", summary.errorTests);
            LOG_INFO("Skipped: {}", summary.skippedTests);
            LOG_INFO("Pass rate: {}%", summary.passRate);
            LOG_INFO("Total time: {}ms", summary.totalExecutionTime.count());
        }

        void endTestRun() override {
            // Do not output end message - let main runner handle it
        }

        std::string getOutputDestination() const override {
            return "Console";
        }
    };

    return std::make_unique<ConsoleTestReporter>();
}

std::unique_ptr<ITestReporter> TestComponentFactory::createXMLReporter(const std::string &outputPath) {
    // Single Responsibility: XML test result reporting
    class XMLTestReporter : public ITestReporter {
    private:
        std::string outputPath_;
        size_t testCount_ = 0;
        std::vector<TestReport> allReports_;

    public:
        explicit XMLTestReporter(const std::string &outputPath) : outputPath_(outputPath) {}

        void beginTestRun(const std::string &testSuiteName) override {
            testCount_ = 0;
            allReports_.clear();
            LOG_INFO("=== {} (Writing to XML: {}) ===", testSuiteName, outputPath_);
        }

        void reportTestResult(const TestReport &report) override {
            ++testCount_;
            allReports_.push_back(report);
            // XMLReporter only stores data - no console output
        }

        void generateSummary(const TestRunSummary &summary) override {
            // Write XML report file
            std::ofstream xmlFile(outputPath_);
            if (xmlFile) {
                xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
                xmlFile << "<testsuite name=\"W3C_SCXML_Tests\" "
                        << "tests=\"" << summary.totalTests << "\" "
                        << "failures=\"" << summary.failedTests << "\" "
                        << "errors=\"" << summary.errorTests << "\" "
                        << "time=\"" << (summary.totalExecutionTime.count() / 1000.0) << "\">" << std::endl;

                for (const auto &report : allReports_) {
                    xmlFile << "  <testcase classname=\"W3C\" "
                            << "name=\"Test_" << report.testId << "\" "
                            << "time=\"" << (report.executionContext.executionTime.count() / 1000.0) << "\"";

                    if (report.validationResult.finalResult != TestResult::PASS) {
                        xmlFile << ">" << std::endl;
                        xmlFile << "    <failure message=\"" << report.validationResult.reason << "\"/>" << std::endl;
                        xmlFile << "  </testcase>" << std::endl;
                    } else {
                        xmlFile << "/>" << std::endl;
                    }
                }

                xmlFile << "</testsuite>" << std::endl;
                xmlFile.close();
            }

            // Do not show console summary - let main runner handle it
        }

        void endTestRun() override {
            // XMLReporter only writes files - no console output
        }

        std::string getOutputDestination() const override {
            return outputPath_;
        }
    };

    return std::make_unique<XMLTestReporter>(outputPath);
}

std::unique_ptr<ITestReporter>
TestComponentFactory::createCompositeReporter(std::unique_ptr<ITestReporter> consoleReporter,
                                              std::unique_ptr<ITestReporter> xmlReporter) {
    // Composite Pattern: Combines multiple reporters
    class CompositeTestReporter : public ITestReporter {
    private:
        std::unique_ptr<ITestReporter> consoleReporter_;
        std::unique_ptr<ITestReporter> xmlReporter_;

    public:
        CompositeTestReporter(std::unique_ptr<ITestReporter> consoleReporter,
                              std::unique_ptr<ITestReporter> xmlReporter)
            : consoleReporter_(std::move(consoleReporter)), xmlReporter_(std::move(xmlReporter)) {}

        void beginTestRun(const std::string &suiteName) override {
            consoleReporter_->beginTestRun(suiteName);
            xmlReporter_->beginTestRun(suiteName);
        }

        void reportTestResult(const TestReport &report) override {
            consoleReporter_->reportTestResult(report);
            xmlReporter_->reportTestResult(report);
        }

        void generateSummary(const TestRunSummary &summary) override {
            consoleReporter_->generateSummary(summary);
            xmlReporter_->generateSummary(summary);
        }

        void endTestRun() override {
            consoleReporter_->endTestRun();
            xmlReporter_->endTestRun();
        }

        std::string getOutputDestination() const override {
            // Return XML reporter's destination as the primary output
            return xmlReporter_->getOutputDestination();
        }
    };

    return std::make_unique<CompositeTestReporter>(std::move(consoleReporter), std::move(xmlReporter));
}

// W3CTestRunner implementation
W3CTestRunner::W3CTestRunner(std::unique_ptr<ITestConverter> converter,
                             std::unique_ptr<ITestMetadataParser> metadataParser,
                             std::unique_ptr<ITestExecutor> executor, std::unique_ptr<ITestResultValidator> validator,
                             std::unique_ptr<ITestSuite> testSuite, std::unique_ptr<ITestReporter> reporter)
    : converter_(std::move(converter)), metadataParser_(std::move(metadataParser)), executor_(std::move(executor)),
      validator_(std::move(validator)), testSuite_(std::move(testSuite)), reporter_(std::move(reporter)) {}

TestRunSummary W3CTestRunner::runAllTests() {
    auto testSuiteInfo = testSuite_->getInfo();
    reporter_->beginTestRun(testSuiteInfo.name);

    std::vector<TestReport> reports;
    auto testDirectories = testSuite_->discoverTests();

    LOG_INFO("W3C Test Execution: Starting {} discovered tests", testDirectories.size());

    for (const auto &testDir : testDirectories) {
        try {
            LOG_DEBUG("W3C Test Execution: Running test {}", testDir);
            TestReport report = runSingleTest(testDir);
            reports.push_back(report);
            reporter_->reportTestResult(report);
            LOG_DEBUG("W3C Test Execution: Test {} completed successfully", testDir);
        } catch (const std::exception &e) {
            LOG_ERROR("W3C Test Execution: Failed to run test in {}: {}", testDir, e.what());
            LOG_ERROR("Failed to run test in {}: {}", testDir, e.what());
        }
    }

    LOG_INFO("W3C Test Execution: Completed {} tests total", reports.size());

    TestRunSummary summary = calculateSummary(reports);
    reporter_->generateSummary(summary);
    reporter_->endTestRun();

    return summary;
}

TestReport W3CTestRunner::runSingleTest(const std::string &testDirectory) {
    TestReport report;
    report.timestamp = std::chrono::system_clock::now();

    try {
        // Parse metadata
        std::string metadataPath = testSuite_->getMetadataPath(testDirectory);
        LOG_DEBUG("W3C Single Test: Parsing metadata from {}", metadataPath);
        report.metadata = metadataParser_->parseMetadata(metadataPath);
        report.testId = std::to_string(report.metadata.id);

        // Skip if necessary
        if (validator_->shouldSkipTest(report.metadata)) {
            LOG_DEBUG("W3C Single Test: Skipping test {} (manual test)", report.testId);
            report.validationResult = ValidationResult(true, TestResult::PASS, "Test skipped");
            return report;
        }

        // Read and convert TXML
        std::string txmlPath = testSuite_->getTXMLPath(testDirectory);
        LOG_DEBUG("W3C Single Test: Reading TXML from {}", txmlPath);
        std::ifstream txmlFile(txmlPath);
        std::string txml((std::istreambuf_iterator<char>(txmlFile)), std::istreambuf_iterator<char>());

        LOG_DEBUG("W3C Single Test: Converting TXML to SCXML for test {}", report.testId);

        // Log original TXML before conversion
        LOG_INFO("W3C Test {}: Original TXML content:\n{}", report.testId, txml);

        std::string scxml = converter_->convertTXMLToSCXML(txml);

        // Log converted SCXML after conversion
        LOG_INFO("W3C Test {}: Converted SCXML content:\n{}", report.testId, scxml);

        // Execute test
        LOG_DEBUG("W3C Single Test: Executing test {}", report.testId);
        report.executionContext = executor_->executeTest(scxml, report.metadata);

        // Validate result
        LOG_DEBUG("W3C Single Test: Validating result for test {}", report.testId);
        report.validationResult = validator_->validateResult(report.executionContext);

        LOG_DEBUG("W3C Single Test: Test {} completed with result: {}", report.testId,
                  static_cast<int>(report.validationResult.finalResult));

        return report;
    } catch (const std::exception &e) {
        LOG_ERROR("W3C Single Test: Exception in test {}: {}", testDirectory, e.what());
        throw;  // Re-throw to be caught by runAllTests
    }
}

TestRunSummary W3CTestRunner::calculateSummary(const std::vector<TestReport> &reports) {
    TestRunSummary summary;
    summary.totalTests = reports.size();

    for (const auto &report : reports) {
        switch (report.validationResult.finalResult) {
        case TestResult::PASS:
            summary.passedTests++;
            break;
        case TestResult::FAIL:
            summary.failedTests++;
            summary.failedTestIds.push_back(report.testId);
            break;
        case TestResult::ERROR:
            summary.errorTests++;
            summary.errorTestIds.push_back(report.testId);
            break;
        case TestResult::TIMEOUT:
            summary.errorTests++;
            summary.errorTestIds.push_back(report.testId);
            break;
        }

        summary.totalExecutionTime += report.executionContext.executionTime;
    }

    if (summary.totalTests > 0) {
        summary.passRate = (static_cast<double>(summary.passedTests) / summary.totalTests) * 100.0;
    }

    return summary;
}

TestReport W3CTestRunner::runSpecificTest(int testId) {
    auto testDirectories = testSuite_->discoverTests();

    for (const auto &testDir : testDirectories) {
        std::filesystem::path path(testDir);
        std::string dirName = path.filename().string();

        if (std::stoi(dirName) == testId) {
            // Special handling for HTTP tests that require bidirectional communication
            if (testId == 201) {
                LOG_INFO("W3C Test {}: Starting HTTP server for bidirectional communication", testId);

                // Create and start the generic W3C HTTP test server
                W3CHttpTestServer httpServer(8080, "/test");

                if (!httpServer.start()) {
                    LOG_ERROR("W3C Test {}: Failed to start HTTP server on port 8080", testId);
                    throw std::runtime_error("Failed to start HTTP server for test " + std::to_string(testId));
                }

                LOG_INFO("W3C Test {}: HTTP server started successfully on localhost:8080/test", testId);

                try {
                    // Run the test with HTTP server running
                    TestReport result = runSingleTestWithHttpServer(testDir, &httpServer);

                    // Stop the server after test completion
                    httpServer.stop();
                    LOG_INFO("W3C Test {}: HTTP server stopped successfully", testId);

                    return result;
                } catch (const std::exception &e) {
                    // Ensure server is stopped even if test fails
                    httpServer.stop();
                    LOG_ERROR("W3C Test {}: Test execution failed, HTTP server stopped: {}", testId, e.what());
                    throw;
                }
            }

            return runSingleTest(testDir);
        }
    }

    throw std::runtime_error("Test " + std::to_string(testId) + " not found");
}

TestRunSummary W3CTestRunner::runFilteredTests(const std::string &conformanceLevel, const std::string &specSection) {
    // Open/Closed Principle: Use existing test suite filtering capability
    auto filteredTests = testSuite_->filterTests(conformanceLevel, specSection);

    auto testSuiteInfo = testSuite_->getInfo();
    reporter_->beginTestRun(testSuiteInfo.name + " (Filtered)");

    std::vector<TestReport> reports;

    for (const auto &testDir : filteredTests) {
        try {
            TestReport report = runSingleTest(testDir);
            reports.push_back(report);
            reporter_->reportTestResult(report);
        } catch (const std::exception &e) {
            LOG_ERROR("Failed to run filtered test in {}: {}", testDir, e.what());
        }
    }

    TestRunSummary summary = calculateSummary(reports);
    reporter_->generateSummary(summary);
    reporter_->endTestRun();

    return summary;
}

TestReport W3CTestRunner::runSingleTestWithHttpServer(const std::string &testDirectory, W3CHttpTestServer *httpServer) {
    TestReport report;
    report.timestamp = std::chrono::system_clock::now();

    try {
        // Parse metadata
        std::string metadataPath = testSuite_->getMetadataPath(testDirectory);
        LOG_DEBUG("W3C Single Test (HTTP): Parsing metadata from {}", metadataPath);
        report.metadata = metadataParser_->parseMetadata(metadataPath);
        report.testId = std::to_string(report.metadata.id);

        // Skip if necessary
        if (validator_->shouldSkipTest(report.metadata)) {
            LOG_DEBUG("W3C Single Test (HTTP): Skipping test {} (manual test)", report.testId);
            report.validationResult = ValidationResult(true, TestResult::PASS, "Test skipped");
            return report;
        }

        // Read and convert TXML
        std::string txmlPath = testSuite_->getTXMLPath(testDirectory);
        LOG_DEBUG("W3C Single Test (HTTP): Reading TXML from {}", txmlPath);
        std::ifstream txmlFile(txmlPath);
        std::string txml((std::istreambuf_iterator<char>(txmlFile)), std::istreambuf_iterator<char>());

        LOG_DEBUG("W3C Single Test (HTTP): Converting TXML to SCXML for test {}", report.testId);
        std::string scxml = converter_->convertTXMLToSCXML(txml);

        // Create custom executor with HTTP server integration
        auto startTime = std::chrono::steady_clock::now();

        TestExecutionContext context;
        context.scxmlContent = scxml;
        context.metadata = report.metadata;
        context.expectedTarget = "pass";

        try {
            LOG_DEBUG("StateMachineTestExecutor (HTTP): Starting test execution for test {}", report.metadata.id);

            // Create EventDispatcher infrastructure for delayed events and #_parent routing
            auto eventRaiser = std::make_shared<RSM::EventRaiserImpl>();
            auto targetFactory = std::make_shared<RSM::EventTargetFactoryImpl>(eventRaiser);

            // Set up HTTP server eventCallback to use the EventRaiser
            httpServer->setEventCallback([eventRaiser](const std::string &eventName, const std::string &eventData) {
                LOG_INFO("W3CHttpTestServer: Receiving echoed event '{}' - raising to SCXML", eventName);
                eventRaiser->raiseEvent(eventName, eventData);
            });

            // Create StateMachine using Builder pattern with proper dependency injection
            auto builder = RSM::StateMachineBuilder().withEventDispatcher(std::make_shared<RSM::EventDispatcherImpl>(
                std::make_shared<RSM::EventSchedulerImpl>([](const RSM::EventDescriptor &event,
                                                             std::shared_ptr<RSM::IEventTarget> target,
                                                             const std::string &sendId) -> bool {
                    LOG_DEBUG("EventScheduler (HTTP): Executing event '{}' on target '{}' with sendId '{}'",
                              event.eventName, target->getDebugInfo(), sendId);

                    if (!target->canHandle(event.target)) {
                        LOG_WARN("EventScheduler (HTTP): Target '{}' cannot handle target '{}'", target->getDebugInfo(),
                                 event.target);
                        return false;
                    }

                    auto future = target->send(event);
                    bool targetResult = false;
                    try {
                        auto sendResult = future.get();
                        targetResult = sendResult.isSuccess;
                    } catch (const std::exception &e) {
                        LOG_ERROR("EventScheduler (HTTP): Failed to send event '{}': {}", event.eventName, e.what());
                        targetResult = false;
                    }

                    return targetResult;
                }),
                targetFactory));

            // Inject EventRaiser into builder for proper dependency injection
            builder.withEventRaiser(eventRaiser);

            auto stateMachine = builder.build();

            // Load SCXML content
            if (!stateMachine->loadSCXMLFromString(scxml)) {
                LOG_ERROR("StateMachineTestExecutor (HTTP): Failed to load SCXML content");
                context.finalState = "error";
                context.errorMessage = "Failed to load SCXML content";
                report.executionContext = context;
                return report;
            }

            // Ensure EventRaiser callback is properly set after SCXML loading
            stateMachine->setEventRaiser(eventRaiser);

            // Start the state machine
            if (!stateMachine->start()) {
                LOG_ERROR("StateMachineTestExecutor (HTTP): Failed to start StateMachine");
                context.finalState = "error";
                context.errorMessage = "Failed to start StateMachine";
                report.executionContext = context;
                return report;
            }

            // Wait for StateMachine to reach final state or timeout
            auto waitStart = std::chrono::steady_clock::now();
            std::string currentState;
            const std::chrono::milliseconds timeout{2000};

            while (std::chrono::steady_clock::now() - waitStart < timeout) {
                currentState = stateMachine->getCurrentState();

                // Check if we reached a final state (pass or fail)
                if (currentState == "pass" || currentState == "fail") {
                    LOG_DEBUG("StateMachineTestExecutor (HTTP): Reached final state: {}", currentState);
                    break;
                }

                // Small sleep to avoid busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // Get final state
            context.finalState = currentState;
            LOG_DEBUG("StateMachineTestExecutor (HTTP): Test completed with final state: {}", context.finalState);

            auto endTime = std::chrono::steady_clock::now();
            context.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            report.executionContext = context;

        } catch (const std::exception &e) {
            LOG_ERROR("StateMachineTestExecutor (HTTP): Exception during test execution: {}", e.what());
            context.finalState = "error";
            context.errorMessage = "Exception: " + std::string(e.what());

            auto endTime = std::chrono::steady_clock::now();
            context.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            report.executionContext = context;
        }

        // Validate result
        LOG_DEBUG("W3C Single Test (HTTP): Validating result for test {}", report.testId);
        report.validationResult = validator_->validateResult(report.executionContext);

        LOG_DEBUG("W3C Single Test (HTTP): Test {} completed with result: {}", report.testId,
                  static_cast<int>(report.validationResult.finalResult));

        return report;
    } catch (const std::exception &e) {
        LOG_ERROR("W3C Single Test (HTTP): Exception in test {}: {}", testDirectory, e.what());
        throw;  // Re-throw to be caught by runSpecificTest
    }
}

}  // namespace RSM::W3C