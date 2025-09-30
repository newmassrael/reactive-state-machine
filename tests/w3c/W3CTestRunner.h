#pragma once

#include "events/EventDispatcherImpl.h"
#include "events/EventSchedulerImpl.h"
#include "interfaces/ITestConverter.h"
#include "interfaces/ITestExecutor.h"
#include "interfaces/ITestMetadataParser.h"
#include "interfaces/ITestReporter.h"
#include "interfaces/ITestResultValidator.h"
#include "interfaces/ITestSuite.h"
#include "runtime/EventRaiserImpl.h"
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace RSM::W3C {

// Forward declarations
class W3CHttpTestServer;

/**
 * @brief Factory for creating W3C test components
 *
 * Dependency Inversion: Creates concrete implementations through interfaces
 * Single Responsibility: Only responsible for component creation
 */
/**
 * @brief RAII wrapper for shared test resources (EventRaiser, EventScheduler, EventDispatcher)
 *
 * This struct owns the lifecycle of shared resources that can be reused across multiple
 * StateMachine instances (e.g., in invoke scenarios). The destructor ensures proper
 * cleanup order with EventScheduler's deadlock protection.
 *
 * Separation of concerns
 * - TestResources: Owns EventRaiser/EventScheduler/EventDispatcher (can be shared)
 * - StateMachineContext: Owns only StateMachine (always exclusive)
 */
struct TestResources {
    const std::shared_ptr<RSM::EventRaiserImpl> eventRaiser;
    const std::shared_ptr<RSM::EventSchedulerImpl> scheduler;
    const std::shared_ptr<RSM::EventDispatcherImpl> eventDispatcher;

    TestResources(std::shared_ptr<RSM::EventRaiserImpl> er, std::shared_ptr<RSM::EventSchedulerImpl> sch,
                  std::shared_ptr<RSM::EventDispatcherImpl> ed)
        : eventRaiser(std::move(er)), scheduler(std::move(sch)), eventDispatcher(std::move(ed)) {}

    ~TestResources() {
        // Cleanup order: scheduler -> eventRaiser
        // EventScheduler's thread_local detection prevents deadlock
        if (scheduler) {
            scheduler->shutdown(true);
        }
        if (eventRaiser) {
            eventRaiser->shutdown();
        }
        // Small delay for graceful thread termination
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
};

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

    /**
     * @brief Create shared test resources with RAII lifecycle management
     *
     * Creates EventRaiser, EventScheduler, and EventDispatcher that can be shared
     * across multiple StateMachine instances. Resources are automatically cleaned up
     * when the returned unique_ptr goes out of scope.
     *
     * Enables resource sharing for W3C invoke scenarios while
     * maintaining clear ownership semantics through RAII.
     *
     * @return Unique pointer to TestResources with automatic cleanup
     */
    static std::unique_ptr<TestResources> createResources();
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
     * @brief Run a single test with HTTP server for bidirectional communication
     */
    TestReport runSingleTestWithHttpServer(const std::string &testDirectory, W3CHttpTestServer *httpServer);

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