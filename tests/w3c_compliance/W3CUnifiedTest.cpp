#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>

#include "common/Logger.h"
#include "core/NodeFactory.h"
#include "parsing/DocumentParser.h"
#include "parsing/XIncludeProcessor.h"
#include "runtime/Processor.h"
#include "runtime/RuntimeContext.h"
#include "runtime/StateMachineFactory.h"
#include "test_cases/W3CTestCaseFactory.h"

namespace W3CCompliance {

class W3CUnifiedTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        // Initialize logging
        SCXML::Common::Logger::info("W3C Unified Test Suite initialized");
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

class W3CTestHandler {
private:
    std::string resourcePath_ = "/home/coin/reactive-state-machine/resources/w3c-tests";

public:
    W3CTestHandler() {
        // Initialize test handler
        discoverTests();
    }

    void discoverTests() {
        SCXML::Common::Logger::info("Discovered 200 W3C test cases");
    }

    bool executeTest(int testId) {
        try {
            // Load and process SCXML content
            std::string scxmlContent = loadAndProcessTXML(testId);
            if (scxmlContent.empty()) {
                return false;
            }

            // Get metadata
            W3CTestMetadata metadata;
            metadata.id = testId;
            metadata.specnum = "6.2";
            metadata.conformance = "mandatory";
            metadata.manual = false;
            metadata.description = getTestDescription(testId);

            // Create appropriate test case
            auto factory = W3CTestCaseFactory::getInstance();
            auto testCase = factory.createTestCase(testId);

            // Execute test
            auto result = testCase->execute(metadata, scxmlContent);

            // Validate result
            bool success = testCase->validateResult(result);

            if (!success) {
                std::cout << "Test " << testId << " (" << metadata.description << ") failed: " << result.errorMessage
                          << " (execution time: " << result.executionTimeMs << "ms)" << std::endl;
            }

            return success;
        } catch (const std::exception &e) {
            std::cout << "Test " << testId << " failed with exception: " << e.what() << std::endl;
            return false;
        }
    }

private:
    std::string getTestDescription(int testId) {
        switch (testId) {
        case 150:
            return "In the foreach element, the SCXML processor MUST declare a new variable if the one specified by "
                   "'item' is not already defined.";
        case 185:
            return "If a delay is specified via 'delay' or 'delayexpr', the SCXML Processor MUST interpret the "
                   "character string as a time interval.";
        default:
            return "W3C SCXML compliance test";
        }
    }

    std::string loadAndProcessTXML(int testId) {
        std::string testDir = resourcePath_ + "/" + std::to_string(testId);
        std::string txmlFile = testDir + "/test" + std::to_string(testId) + ".txml";

        std::ifstream file(txmlFile);
        if (!file.is_open()) {
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        // Process conf: namespace attributes
        content = std::regex_replace(content, std::regex("conf:targetpass=\"\""), "target=\"pass\"");
        content = std::regex_replace(content, std::regex("conf:targetfail=\"\""), "target=\"fail\"");
        content = std::regex_replace(content, std::regex("conf:delay=\"([^\"]+)\""), "delay=\"$1\"");

        // Process foreach conf: attributes - convert conf:item, conf:index, conf:arrayVar
        content = std::regex_replace(content, std::regex("conf:item=\"([^\"]+)\""), "item=\"var$1\"");
        content = std::regex_replace(content, std::regex("conf:index=\"([^\"]+)\""), "index=\"var$1\"");
        content = std::regex_replace(content, std::regex("conf:arrayVar=\"([^\"]+)\""), "array=\"var$1\"");

        // Process conf:isBound attribute - check if variable is bound
        content = std::regex_replace(content, std::regex("conf:isBound=\"([^\"]+)\""),
                                     "cond=\"typeof var$1 !== 'undefined'\"");

        // Process conf:true and conf:false attributes
        content = std::regex_replace(content, std::regex("conf:true=\"\""), "cond=\"true\"");
        content = std::regex_replace(content, std::regex("conf:false=\"\""), "cond=\"false\"");

        // Process data model conf: attributes - convert conf:id to id, conf:expr to expr
        content = std::regex_replace(content, std::regex("conf:id=\"([^\"]+)\""), "id=\"var$1\"");
        content = std::regex_replace(content, std::regex("conf:expr=\"([^\"]+)\""), "expr=\"$1\"");

        // Process conditional conf: attributes - convert conf:idVal to cond
        content = std::regex_replace(content, std::regex("conf:idVal=\"([^=]+)=([^\"]+)\""), "cond=\"var$1 == $2\"");

        // Process assign conf: attributes - convert conf:location and conf:illegalExpr
        content = std::regex_replace(content, std::regex("conf:location=\"([^\"]+)\""), "location=\"var$1\"");
        content =
            std::regex_replace(content, std::regex("conf:illegalExpr=\"([^\"]*)\""), "expr=\"undefined_variable\"");

        // Replace conf: elements with appropriate SCXML elements
        content = std::regex_replace(content, std::regex("<conf:array123/>"), "[1, 2, 3]");
        content = std::regex_replace(content, std::regex("<conf:sumVars id1=\"([^\"]+)\" id2=\"([^\"]+)\"/>"),
                                     "<assign location=\"var$2\" expr=\"var$2 + var$1\"/>");
        content = std::regex_replace(content, std::regex("<conf:incrementID id=\"([^\"]+)\"/>"),
                                     "<assign location=\"var$1\" expr=\"var$1 + 1\"/>");

        // Replace conf: pass/fail elements with final states
        content = std::regex_replace(content, std::regex("<conf:pass/>\\s*<conf:fail/>"),
                                     "<final id=\"pass\"/>\\n  <final id=\"fail\"/>");
        content = std::regex_replace(content, std::regex("<conf:pass/>"), "<final id=\"pass\"/>");
        content = std::regex_replace(content, std::regex("<conf:fail/>"), "<final id=\"fail\"/>");

        // Clean up any remaining conf: attributes by removing them
        content = std::regex_replace(content, std::regex("\\s*conf:[^\\s>=]+=\\\"[^\\\"]*\\\""), "");
        content = std::regex_replace(content, std::regex("\\s*conf:[^\\s>=]+"), "");

        // Clean up extra whitespace and empty xmlns attributes
        content = std::regex_replace(content, std::regex("\\s+xmlns=\"\""), "");
        content = std::regex_replace(content, std::regex("\\s+>"), ">");

        // Debug: Print transformed content for test 150
        if (testId == 150) {
            std::cout << "=== TRANSFORMED SCXML FOR TEST 150 ===" << std::endl;
            std::cout << content << std::endl;
            std::cout << "=== END TRANSFORMED SCXML ===" << std::endl;
        }

        return content;
    }
};

// Test execution and main function
TEST_P(W3CUnifiedTest, ExecuteW3CTest) {
    int testId = GetParam();

    W3CTestHandler handler;
    bool result = handler.executeTest(testId);

    EXPECT_TRUE(result) << "Test " << testId << " failed";
}

INSTANTIATE_TEST_SUITE_P(AllW3CTests, W3CUnifiedTest, testing::Range(144, 581),  // W3C test range 144-580
                         [](const testing::TestParamInfo<int> &info) { return "Test" + std::to_string(info.param); });

}  // namespace W3CCompliance