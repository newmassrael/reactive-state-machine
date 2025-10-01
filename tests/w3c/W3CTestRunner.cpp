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
#include "runtime/StateMachineContext.h"
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

            TestExecutionContext testContext;
            testContext.scxmlContent = scxmlContent;
            testContext.metadata = metadata;
            // W3C tests should pass when correctly implemented, regardless of conformance level
            // conformance indicates whether implementation is required, not expected outcome
            testContext.expectedTarget = "pass";

            // Create shared resources using RAII factory pattern
            auto resources = TestComponentFactory::createResources();

            try {
                LOG_DEBUG("StateMachineTestExecutor: Starting test execution for test {}", metadata.id);

                // Build StateMachine with resource injection, then wrap in RAII context
                auto stateMachineUnique = RSM::StateMachineBuilder()
                                              .withEventDispatcher(resources->eventDispatcher)
                                              .withEventRaiser(resources->eventRaiser)
                                              .build();

                // Wrap in StateMachineContext for RAII cleanup
                auto smContext = std::make_unique<RSM::StateMachineContext>(std::move(stateMachineUnique));
                auto *stateMachine = smContext->get();

                // W3C SCXML compliance: EventRaiser callback should pass eventData to StateMachine
                // StateMachine will automatically set its own callback, so we don't override it here
                // eventRaiser->setEventCallback(...); // Removed - let StateMachine set its own callback

                // Load SCXML content
                if (!stateMachine->loadSCXMLFromString(scxmlContent)) {
                    LOG_ERROR("StateMachineTestExecutor: Failed to load SCXML content");
                    testContext.finalState = "error";
                    testContext.errorMessage = "Failed to load SCXML content";
                    return testContext;
                }

                // W3C SCXML compliance: Ensure EventRaiser callback is properly set after SCXML loading
                // This guarantees that any initialization during SCXML loading doesn't override our callback
                stateMachine->setEventRaiser(resources->eventRaiser);

                // Start the state machine
                if (!stateMachine->start()) {
                    LOG_ERROR("StateMachineTestExecutor: Failed to start StateMachine");
                    testContext.finalState = "error";
                    testContext.errorMessage = "Failed to start StateMachine";
                    return testContext;
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

                // Get final state - always read fresh state after loop exit
                testContext.finalState = stateMachine->getCurrentState();
                LOG_DEBUG("StateMachineTestExecutor: Test completed with final state: {}", testContext.finalState);

                auto endTime = std::chrono::steady_clock::now();
                testContext.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

                // RAII cleanup: StateMachineContext destructor handles all cleanup automatically
                LOG_DEBUG("StateMachineTestExecutor: Automatic cleanup will occur on scope exit");
                return testContext;

            } catch (const std::exception &e) {
                LOG_ERROR("StateMachineTestExecutor: Exception during test execution: {}", e.what());
                testContext.finalState = "error";
                testContext.errorMessage = "Exception: " + std::string(e.what());

                auto endTime = std::chrono::steady_clock::now();
                testContext.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

                // RAII cleanup: StateMachineContext destructor handles cleanup on exception
                LOG_DEBUG("StateMachineTestExecutor: Automatic cleanup will occur on scope exit after exception");
                return testContext;
            }
        }

        TestExecutionContext executeTest(const std::string &scxmlContent, const TestMetadata &metadata,
                                         const std::string &sourceFilePath) override {
            auto startTime = std::chrono::steady_clock::now();

            TestExecutionContext testContext;
            testContext.scxmlContent = scxmlContent;
            testContext.metadata = metadata;
            testContext.expectedTarget = "pass";

            // Create shared resources using RAII factory pattern
            auto resources = TestComponentFactory::createResources();

            try {
                LOG_DEBUG("StateMachineTestExecutor: Starting test execution for test {} with source path: {}",
                          metadata.id, sourceFilePath);

                // Build StateMachine with resource injection, then wrap in RAII context
                auto stateMachineUnique = RSM::StateMachineBuilder()
                                              .withEventDispatcher(resources->eventDispatcher)
                                              .withEventRaiser(resources->eventRaiser)
                                              .build();

                // Wrap in StateMachineContext for RAII cleanup
                auto smContext = std::make_unique<RSM::StateMachineContext>(std::move(stateMachineUnique));
                auto *stateMachine = smContext->get();

                // Register source file path for relative path resolution before loading SCXML
                RSM::JSEngine::instance().registerSessionFilePath(stateMachine->getSessionId(), sourceFilePath);
                LOG_DEBUG("StateMachineTestExecutor: Registered source file path '{}' for session '{}'", sourceFilePath,
                          stateMachine->getSessionId());

                // Load SCXML content
                if (!stateMachine->loadSCXMLFromString(scxmlContent)) {
                    LOG_ERROR("StateMachineTestExecutor: Failed to load SCXML content");
                    testContext.finalState = "error";
                    testContext.errorMessage = "Failed to load SCXML content";
                    return testContext;
                }

                // Set EventRaiser and start the state machine
                stateMachine->setEventRaiser(resources->eventRaiser);
                if (!stateMachine->start()) {
                    LOG_ERROR("StateMachineTestExecutor: Failed to start StateMachine");
                    testContext.finalState = "error";
                    testContext.errorMessage = "Failed to start StateMachine";
                    return testContext;
                }

                // Wait for StateMachine to reach final state or timeout
                auto waitStart = std::chrono::steady_clock::now();
                std::string currentState;

                while (std::chrono::steady_clock::now() - waitStart < timeout_) {
                    currentState = stateMachine->getCurrentState();
                    if (currentState == "pass" || currentState == "fail") {
                        LOG_DEBUG("StateMachineTestExecutor: Reached final state: {}", currentState);
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                // Get final state - always read fresh state after loop exit
                testContext.finalState = stateMachine->getCurrentState();
                LOG_DEBUG("StateMachineTestExecutor: Test completed with final state: {}", testContext.finalState);

                auto endTime = std::chrono::steady_clock::now();
                testContext.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

                // RAII cleanup: StateMachineContext destructor handles all cleanup automatically
                LOG_DEBUG("StateMachineTestExecutor: Automatic cleanup will occur on scope exit");
                return testContext;

            } catch (const std::exception &e) {
                LOG_ERROR("StateMachineTestExecutor: Exception during test execution: {}", e.what());
                testContext.finalState = "error";
                testContext.errorMessage = "Exception: " + std::string(e.what());

                auto endTime = std::chrono::steady_clock::now();
                testContext.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

                // RAII cleanup: StateMachineContext destructor handles cleanup on exception
                LOG_DEBUG("StateMachineTestExecutor: Automatic cleanup will occur on scope exit after exception");
                return testContext;
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
                                // Main test file exists - add it
                                testDirs.push_back(entry.path().string());
                            } else {
                                // Check for variant test files (test403a.txml, test403b.txml, etc.)
                                int testId = extractTestId(entry.path().string());
                                bool hasVariants = false;

                                // Common variant suffixes: a, b, c, d, e
                                for (char suffix = 'a'; suffix <= 'z'; ++suffix) {
                                    std::string variantPath =
                                        entry.path().string() + "/test" + std::to_string(testId) + suffix + ".txml";
                                    if (std::filesystem::exists(variantPath)) {
                                        // Add variant as separate test (with suffix in path for identification)
                                        testDirs.push_back(entry.path().string() + ":" + std::string(1, suffix));
                                        hasVariants = true;
                                    } else {
                                        // Stop checking once we hit a missing variant
                                        break;
                                    }
                                }

                                if (!hasVariants) {
                                    // No main file and no variants - skip this test
                                    LOG_DEBUG("W3CTestSuite: No TXML file found for test {}", testId);
                                }
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
            // Check if path contains variant suffix (format: "path/403:a")
            size_t colonPos = testDirectory.find(':');
            if (colonPos != std::string::npos) {
                // Extract base path and variant suffix
                std::string basePath = testDirectory.substr(0, colonPos);
                std::string variant = testDirectory.substr(colonPos + 1);
                int testId = extractTestId(basePath);
                return basePath + "/test" + std::to_string(testId) + variant + ".txml";
            }

            // Normal path without variant
            int testId = extractTestId(testDirectory);
            return testDirectory + "/test" + std::to_string(testId) + ".txml";
        }

        std::string getMetadataPath(const std::string &testDirectory) override {
            // Remove variant suffix if present (format: "path/403:a")
            size_t colonPos = testDirectory.find(':');
            if (colonPos != std::string::npos) {
                std::string basePath = testDirectory.substr(0, colonPos);
                return basePath + "/metadata.txt";
            }
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

std::unique_ptr<TestResources> TestComponentFactory::createResources() {
    // Create EventRaiser
    auto eventRaiser = std::make_shared<RSM::EventRaiserImpl>();

    // Create EventScheduler with event execution callback
    auto scheduler = std::make_shared<RSM::EventSchedulerImpl>([](const RSM::EventDescriptor &event,
                                                                  std::shared_ptr<RSM::IEventTarget> target,
                                                                  const std::string &sendId) -> bool {
        // Event execution callback: send event to target and return success status
        LOG_DEBUG("EventScheduler: Executing event '{}' with sendId '{}' on target '{}'", event.eventName, sendId,
                  target->getDebugInfo());

        auto future = target->send(event);
        try {
            auto sendResult = future.get();
            if (sendResult.isSuccess) {
                LOG_DEBUG("EventScheduler: Event '{}' (sendId: '{}') executed successfully", event.eventName, sendId);
            } else {
                LOG_WARN("EventScheduler: Event '{}' (sendId: '{}') execution failed", event.eventName, sendId);
            }
            return sendResult.isSuccess;
        } catch (const std::exception &e) {
            LOG_ERROR("EventScheduler: Failed to send event '{}' (sendId: '{}'): {}", event.eventName, sendId,
                      e.what());
            return false;
        }
    });

    // Create EventTargetFactory and EventDispatcher
    auto targetFactory = std::make_shared<RSM::EventTargetFactoryImpl>(eventRaiser, scheduler);
    auto eventDispatcher = std::make_shared<RSM::EventDispatcherImpl>(scheduler, targetFactory);

    // Create TestResources with const fields initialized via constructor
    return std::make_unique<TestResources>(std::move(eventRaiser), std::move(scheduler), std::move(eventDispatcher));
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

            // Extract test ID from directory name
            std::filesystem::path path(testDir);
            std::string dirName = path.filename().string();
            int testId = 0;
            try {
                testId = std::stoi(dirName);
            } catch (...) {
                testId = 0;
            }

            TestReport report;

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
                    report = runSingleTestWithHttpServer(testDir, &httpServer);

                    // Stop the server after test completion
                    httpServer.stop();
                    LOG_INFO("W3C Test {}: HTTP server stopped successfully", testId);
                } catch (const std::exception &e) {
                    // Ensure server is stopped even if test fails
                    httpServer.stop();
                    LOG_ERROR("W3C Test {}: Test execution failed, HTTP server stopped: {}", testId, e.what());
                    throw;
                }
            } else {
                report = runSingleTest(testDir);
            }

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

        // Extract variant suffix if present (format: "path/403:a")
        std::string variantSuffix;
        size_t colonPos = testDirectory.find(':');
        if (colonPos != std::string::npos) {
            variantSuffix = testDirectory.substr(colonPos + 1);
        }

        // Set testId with variant suffix if present
        report.testId = std::to_string(report.metadata.id);
        if (!variantSuffix.empty()) {
            report.testId += variantSuffix;
        }

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

        // Convert all sub-TXML files in the test directory for invoke elements
        // Extract actual directory path (remove variant suffix if present)
        std::string actualTestDir = testDirectory;
        size_t colonPos2 = testDirectory.find(':');
        if (colonPos2 != std::string::npos) {
            actualTestDir = testDirectory.substr(0, colonPos2);
        }
        std::filesystem::path testDir(actualTestDir);
        for (const auto &entry : std::filesystem::directory_iterator(testDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txml") {
                std::string filename = entry.path().filename().string();
                // Skip main test file
                if (filename == "test" + report.testId + ".txml") {
                    continue;
                }

                // Convert sub-TXML file to SCXML (without W3C validation for sub-files)
                std::ifstream subTxmlFile(entry.path());
                std::string subTxml((std::istreambuf_iterator<char>(subTxmlFile)), std::istreambuf_iterator<char>());
                std::string subScxml = static_cast<RSM::W3C::TXMLConverter *>(converter_.get())
                                           ->convertTXMLToSCXMLWithoutValidation(subTxml);

                // Write converted SCXML file
                std::filesystem::path scxmlPath = entry.path();
                scxmlPath.replace_extension(".scxml");
                std::ofstream scxmlFile(scxmlPath);
                scxmlFile << subScxml;

                LOG_DEBUG("W3C Test {}: Converted sub-file {} to {}", report.testId, filename,
                          scxmlPath.filename().string());
            }
        }

        // Execute test
        LOG_DEBUG("W3C Single Test: Executing test {}", report.testId);
        report.executionContext = executor_->executeTest(scxml, report.metadata, txmlPath);

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
        // Extract testId from directory path (handle both normal and variant paths)
        // Variant paths: "path/403:a" -> extract 403
        // Normal paths: "path/403" -> extract 403
        std::string pathStr = testDir;
        size_t colonPos = pathStr.find(':');
        if (colonPos != std::string::npos) {
            pathStr = pathStr.substr(0, colonPos);
        }

        std::filesystem::path path(pathStr);
        std::string dirName = path.filename().string();

        int currentTestId = 0;
        try {
            currentTestId = std::stoi(dirName);
        } catch (...) {
            continue;
        }

        if (currentTestId == testId) {
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

std::vector<TestReport> W3CTestRunner::runAllMatchingTests(int testId) {
    std::vector<TestReport> matchingReports;
    auto testDirectories = testSuite_->discoverTests();

    for (const auto &testDir : testDirectories) {
        // Extract testId from directory path (handle both normal and variant paths)
        std::string pathStr = testDir;
        size_t colonPos = pathStr.find(':');
        if (colonPos != std::string::npos) {
            pathStr = pathStr.substr(0, colonPos);
        }

        std::filesystem::path path(pathStr);
        std::string dirName = path.filename().string();

        int currentTestId = 0;
        try {
            currentTestId = std::stoi(dirName);
        } catch (...) {
            continue;
        }

        if (currentTestId == testId) {
            try {
                // Special handling for HTTP tests that require bidirectional communication
                if (testId == 201) {
                    LOG_INFO("W3C Test {}: Starting HTTP server for bidirectional communication", testId);

                    W3CHttpTestServer httpServer(8080, "/test");

                    if (!httpServer.start()) {
                        LOG_ERROR("W3C Test {}: Failed to start HTTP server on port 8080", testId);
                        throw std::runtime_error("Failed to start HTTP server for test " + std::to_string(testId));
                    }

                    LOG_INFO("W3C Test {}: HTTP server started successfully on localhost:8080/test", testId);

                    try {
                        TestReport result = runSingleTestWithHttpServer(testDir, &httpServer);
                        httpServer.stop();
                        LOG_INFO("W3C Test {}: HTTP server stopped successfully", testId);
                        matchingReports.push_back(result);
                    } catch (const std::exception &e) {
                        httpServer.stop();
                        LOG_ERROR("W3C Test {}: Test execution failed, HTTP server stopped: {}", testId, e.what());
                        throw;
                    }
                } else {
                    TestReport report = runSingleTest(testDir);
                    matchingReports.push_back(report);
                }
            } catch (const std::exception &e) {
                LOG_ERROR("W3C Test Execution: Failed to run test in {}: {}", testDir, e.what());
                // Continue with other variants even if one fails
            }
        }
    }

    if (matchingReports.empty()) {
        throw std::runtime_error("Test " + std::to_string(testId) + " not found");
    }

    return matchingReports;
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

        // Extract variant suffix if present (format: "path/403:a")
        std::string variantSuffix;
        size_t colonPos = testDirectory.find(':');
        if (colonPos != std::string::npos) {
            variantSuffix = testDirectory.substr(colonPos + 1);
        }

        // Set testId with variant suffix if present
        report.testId = std::to_string(report.metadata.id);
        if (!variantSuffix.empty()) {
            report.testId += variantSuffix;
        }

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

        // Create shared resources using RAII factory pattern
        auto resources = TestComponentFactory::createResources();

        try {
            LOG_DEBUG("StateMachineTestExecutor (HTTP): Starting test execution for test {}", report.metadata.id);

            // Set up HTTP server eventCallback to use the EventRaiser
            httpServer->setEventCallback(
                [eventRaiser = resources->eventRaiser](const std::string &eventName, const std::string &eventData) {
                    LOG_INFO("W3CHttpTestServer: Receiving echoed event '{}' - raising to SCXML", eventName);
                    eventRaiser->raiseEvent(eventName, eventData);
                });

            // Build StateMachine with resource injection, then wrap in RAII context
            auto stateMachineUnique = RSM::StateMachineBuilder()
                                          .withEventDispatcher(resources->eventDispatcher)
                                          .withEventRaiser(resources->eventRaiser)
                                          .build();

            // Wrap in StateMachineContext for RAII cleanup
            auto smContext = std::make_unique<RSM::StateMachineContext>(std::move(stateMachineUnique));
            auto *stateMachine = smContext->get();

            // Load SCXML content
            if (!stateMachine->loadSCXMLFromString(scxml)) {
                LOG_ERROR("StateMachineTestExecutor (HTTP): Failed to load SCXML content");
                context.finalState = "error";
                context.errorMessage = "Failed to load SCXML content";
                report.executionContext = context;
                return report;
            }

            // Ensure EventRaiser callback is properly set after SCXML loading
            stateMachine->setEventRaiser(resources->eventRaiser);

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

            // Get final state - always read fresh state after loop exit
            context.finalState = stateMachine->getCurrentState();
            LOG_DEBUG("StateMachineTestExecutor (HTTP): Test completed with final state: {}", context.finalState);

            auto endTime = std::chrono::steady_clock::now();
            context.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            // RAII cleanup: StateMachineContext destructor handles all cleanup automatically
            LOG_DEBUG("StateMachineTestExecutor (HTTP): Automatic cleanup will occur on scope exit");

            report.executionContext = context;

        } catch (const std::exception &e) {
            LOG_ERROR("StateMachineTestExecutor (HTTP): Exception during test execution: {}", e.what());
            context.finalState = "error";
            context.errorMessage = "Exception: " + std::string(e.what());

            auto endTime = std::chrono::steady_clock::now();
            context.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            // RAII cleanup: StateMachineContext destructor handles cleanup on exception (HTTP version)
            LOG_DEBUG("StateMachineTestExecutor (HTTP): Automatic cleanup will occur on scope exit after exception");

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