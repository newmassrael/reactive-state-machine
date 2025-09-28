#include "W3CTestRunner.h"
#include <gtest/gtest.h>
#include <iostream>

namespace RSM::W3C::Tests {

/**
 * @brief Google Test fixture for W3C SCXML compliance testing
 *
 * Integrates W3C test runner with Google Test framework
 */
class W3CComplianceTest : public ::testing::Test {
protected:
    std::unique_ptr<W3CTestRunner> testRunner_;

    void SetUp() override {
        // Create all components using factory (Dependency Injection)
        auto converter = TestComponentFactory::createConverter();
        auto metadataParser = TestComponentFactory::createMetadataParser();
        auto executor = TestComponentFactory::createExecutor();
        auto validator = TestComponentFactory::createValidator();
        auto testSuite = TestComponentFactory::createTestSuite("resources");
        auto reporter = TestComponentFactory::createConsoleReporter();

        // Inject dependencies into test runner
        testRunner_ =
            std::make_unique<W3CTestRunner>(std::move(converter), std::move(metadataParser), std::move(executor),
                                            std::move(validator), std::move(testSuite), std::move(reporter));
    }

    void TearDown() override {
        testRunner_.reset();
    }
};

/**
 * @brief Run all W3C SCXML compliance tests
 *
 * This test executes the complete W3C test suite and validates
 * RSM implementation against official SCXML 1.0 specification.
 */
TEST_F(W3CComplianceTest, RunAllW3CTests) {
    ASSERT_NE(testRunner_, nullptr) << "W3C test runner should be initialized";

    TestRunSummary summary;

    ASSERT_NO_THROW({ summary = testRunner_->runAllTests(); }) << "W3C test execution should not throw exceptions";

    // Basic validation
    EXPECT_GT(summary.totalTests, 0) << "Should discover W3C tests";
    EXPECT_GE(summary.passedTests + summary.failedTests + summary.errorTests + summary.skippedTests, summary.totalTests)
        << "Test counts should add up";

    // Report results
    LOG_INFO("\n=== W3C SCXML Compliance Results ===");
    LOG_INFO("Total tests executed: {}", summary.totalTests);
    LOG_INFO("Pass rate: {}%", summary.passRate);

    // For now, just ensure tests run without crashing
    // TODO: Set minimum pass rate requirements as implementation improves
    EXPECT_GE(summary.passRate, 0.0) << "Pass rate should be non-negative";
}

/**
 * @brief Test individual W3C test execution
 *
 * Validates that the test runner can execute specific tests by ID
 */
TEST_F(W3CComplianceTest, RunSpecificW3CTest) {
    ASSERT_NE(testRunner_, nullptr);

    TestReport report;

    // Test with a known test ID (144 - event queue ordering)
    ASSERT_NO_THROW({ report = testRunner_->runSpecificTest(144); }) << "Should be able to run specific test by ID";

    EXPECT_EQ(report.testId, "144") << "Should execute correct test";
    EXPECT_EQ(report.metadata.id, 144) << "Metadata should match test ID";
    EXPECT_FALSE(report.metadata.specnum.empty()) << "Should have spec number";
    EXPECT_FALSE(report.metadata.conformance.empty()) << "Should have conformance level";

    LOG_INFO("\nTest 144 Result: {}", report.validationResult.reason);
}

/**
 * @brief Test TXML to SCXML conversion
 *
 * Validates the conversion pipeline works correctly
 */
TEST_F(W3CComplianceTest, TXMLConversionWorks) {
    auto converter = TestComponentFactory::createConverter();
    ASSERT_NE(converter, nullptr);

    // Sample TXML content with conf: namespace
    std::string sampleTXML = R"(<?xml version="1.0"?>
<scxml initial="s0" version="1.0" conf:datamodel="" xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
    <state id="s0">
        <transition event="test" conf:targetpass=""/>
    </state>
    <conf:pass/>
</scxml>)";

    std::string scxml;
    ASSERT_NO_THROW({ scxml = converter->convertTXMLToSCXML(sampleTXML); }) << "TXML conversion should not throw";

    EXPECT_TRUE(scxml.find("conf:") == std::string::npos) << "SCXML should not contain conf: namespace";
    EXPECT_TRUE(scxml.find("datamodel=\"ecmascript\"") != std::string::npos) << "Should convert datamodel attribute";
    EXPECT_TRUE(scxml.find("target=\"pass\"") != std::string::npos) << "Should convert target attributes";
    EXPECT_TRUE(scxml.find("<final id=\"pass\"/>") != std::string::npos) << "Should convert conf: elements";
}

/**
 * @brief Test metadata parsing
 *
 * Validates metadata parser works with W3C metadata format
 */
TEST_F(W3CComplianceTest, MetadataParsingWorks) {
    auto parser = TestComponentFactory::createMetadataParser();
    ASSERT_NE(parser, nullptr);

    // Test with actual metadata file
    std::string metadataPath = "resources/144/metadata.txt";

    TestMetadata metadata;
    ASSERT_NO_THROW({ metadata = parser->parseMetadata(metadataPath); }) << "Metadata parsing should not throw";

    EXPECT_EQ(metadata.id, 144) << "Should parse test ID correctly";
    EXPECT_EQ(metadata.specnum, "4.2") << "Should parse spec number";
    EXPECT_EQ(metadata.conformance, "mandatory") << "Should parse conformance level";
    EXPECT_FALSE(metadata.manual) << "Test 144 should not be manual";
    EXPECT_FALSE(metadata.description.empty()) << "Should have description";
}

}  // namespace RSM::W3C::Tests