// TDD: Minimal Static Compiler Tests
// Goal: Verify basic SCXML to C++ transformation

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "tools/codegen/StaticCodeGenerator.h"

using namespace RSM::Codegen;

namespace fs = std::filesystem;

class StaticCodeGenTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for tests
        testDir_ = fs::temp_directory_path() / "rsm_codegen_test";
        fs::create_directories(testDir_);
    }

    void TearDown() override {
        // Clean up test directory
        fs::remove_all(testDir_);
    }

    // Helper to create simple SCXML file
    std::string createSimpleSCXML(const std::string &filename) {
        std::string content = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" name="SimpleSM" initial="idle">
  <state id="idle">
    <transition event="start" target="active"/>
  </state>
  <state id="active">
    <transition event="stop" target="idle"/>
  </state>
</scxml>)";

        std::string path = (testDir_ / filename).string();
        std::ofstream file(path);
        if (!file) {
            throw std::runtime_error("Failed to create test SCXML file: " + path);
        }
        file << content;

        return path;
    }

    // Helper to create custom SCXML file
    std::string createCustomSCXML(const std::string &filename, const std::string &smName,
                                  const std::string &initialState, const std::vector<std::string> &states) {
        std::stringstream ss;
        ss << R"(<?xml version="1.0" encoding="UTF-8"?>)";
        ss << "\n<scxml xmlns=\"http://www.w3.org/2005/07/scxml\" version=\"1.0\" name=\"" << smName << "\" initial=\""
           << initialState << "\">\n";

        for (const auto &state : states) {
            ss << "  <state id=\"" << state << "\"/>\n";
        }

        ss << "</scxml>";

        std::string path = (testDir_ / filename).string();
        std::ofstream file(path);
        if (!file) {
            throw std::runtime_error("Failed to create test SCXML file: " + path);
        }
        file << ss.str();

        return path;
    }

    // Helper to read file content
    std::string readFile(const std::string &path) {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    fs::path testDir_;
};

TEST_F(StaticCodeGenTest, GeneratesStateEnum) {
    // Arrange: Prepare simple SCXML
    std::string scxmlPath = createSimpleSCXML("simple.scxml");
    std::string outputDir = testDir_.string();

    // Act: Generate code
    StaticCodeGenerator generator;
    ASSERT_TRUE(generator.generate(scxmlPath, outputDir));

    // Assert: Verify State enum is generated
    std::string generatedFile = (testDir_ / "SimpleSM_sm.h").string();
    ASSERT_TRUE(fs::exists(generatedFile)) << "Generated file should exist";

    std::string content = readFile(generatedFile);

    // Verify State enum
    EXPECT_TRUE(content.find("enum class State") != std::string::npos) << "State enum should be generated";
    EXPECT_TRUE(content.find("Idle") != std::string::npos) << "Idle state should be included";
    EXPECT_TRUE(content.find("Active") != std::string::npos) << "Active state should be included";
}

TEST_F(StaticCodeGenTest, GeneratesEventEnum) {
    // Arrange
    std::string scxmlPath = createSimpleSCXML("simple.scxml");
    std::string outputDir = testDir_.string();

    // Act: Generate code
    StaticCodeGenerator generator;
    ASSERT_TRUE(generator.generate(scxmlPath, outputDir));

    // Assert: Verify Event enum
    std::string generatedFile = (testDir_ / "SimpleSM_sm.h").string();
    ASSERT_TRUE(fs::exists(generatedFile)) << "Generated file should exist";

    std::string content = readFile(generatedFile);

    EXPECT_TRUE(content.find("enum class Event") != std::string::npos) << "Event enum should be generated";
    EXPECT_TRUE(content.find("Start") != std::string::npos) << "start event should be transformed to Start";
    EXPECT_TRUE(content.find("Stop") != std::string::npos) << "stop event should be transformed to Stop";
}

TEST_F(StaticCodeGenTest, ParsesActualSCXMLFile) {
    // Arrange: Create custom SCXML with different name and states
    std::string scxmlPath = createCustomSCXML("robot.scxml", "RobotSM", "waiting", {"waiting", "moving", "stopped"});
    std::string outputDir = testDir_.string();

    // Act: Generate code
    StaticCodeGenerator generator;
    ASSERT_TRUE(generator.generate(scxmlPath, outputDir));

    // Assert: Verify it uses actual SCXML content, not hardcoded values
    std::string generatedFile = (testDir_ / "RobotSM_sm.h").string();
    ASSERT_TRUE(fs::exists(generatedFile)) << "Generated file should exist with SCXML name";

    std::string content = readFile(generatedFile);

    // Should use actual SCXML name "RobotSM", not hardcoded "SimpleSM"
    EXPECT_TRUE(content.find("class RobotSM") != std::string::npos) << "Should use SCXML name 'RobotSM'";
    EXPECT_FALSE(content.find("class SimpleSM") != std::string::npos) << "Should NOT use hardcoded 'SimpleSM'";

    // Should use actual states from SCXML
    EXPECT_TRUE(content.find("Waiting") != std::string::npos) << "Should include 'waiting' state";
    EXPECT_TRUE(content.find("Moving") != std::string::npos) << "Should include 'moving' state";
    EXPECT_TRUE(content.find("Stopped") != std::string::npos) << "Should include 'stopped' state";

    // Should NOT have hardcoded states
    EXPECT_FALSE(content.find("Idle") != std::string::npos) << "Should NOT have hardcoded 'idle' state";
    EXPECT_FALSE(content.find("Active") != std::string::npos) << "Should NOT have hardcoded 'active' state";
}

TEST_F(StaticCodeGenTest, ExtractsGuardFunctions) {
    // Arrange: SCXML with Guard conditions
    std::string scxmlContent = R"XML(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" name="GuardedSM" initial="idle">
  <state id="idle">
    <transition event="check" cond="isReady()" target="active"/>
    <transition event="verify" cond="isValid()" target="active"/>
  </state>
  <state id="active">
    <transition event="check" cond="!isReady()" target="idle"/>
  </state>
</scxml>)XML";

    std::string scxmlPath = (testDir_ / "guarded.scxml").string();
    std::ofstream file(scxmlPath);
    if (!file) {
        throw std::runtime_error("Failed to create test SCXML file: " + scxmlPath);
    }
    file << scxmlContent;
    file.close();  // Ensure file is flushed to disk

    // Act
    StaticCodeGenerator generator;
    auto guards = generator.extractGuards(scxmlPath);

    // Assert: Should extract unique guard function names
    ASSERT_EQ(guards.size(), 2) << "Should extract 2 unique guard functions";
    EXPECT_TRUE(guards.count("isReady") > 0) << "Should extract isReady guard";
    EXPECT_TRUE(guards.count("isValid") > 0) << "Should extract isValid guard";
}

TEST_F(StaticCodeGenTest, ExtractsActionFunctions) {
    // Arrange: SCXML with Actions
    std::string scxmlContent = R"XML(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" name="ActionSM" initial="idle">
  <state id="idle">
    <transition event="start" target="active">
      <script>initialize()</script>
    </transition>
  </state>
  <state id="active">
    <onentry>
      <script>activate()</script>
    </onentry>
    <onexit>
      <script>deactivate()</script>
    </onexit>
  </state>
</scxml>)XML";

    std::string scxmlPath = (testDir_ / "action.scxml").string();
    std::ofstream file(scxmlPath);
    if (!file) {
        throw std::runtime_error("Failed to create test SCXML file: " + scxmlPath);
    }
    file << scxmlContent;
    file.close();  // Ensure file is flushed to disk

    // Act
    StaticCodeGenerator generator;
    auto actions = generator.extractActions(scxmlPath);

    // Assert: Should extract unique action function names
    ASSERT_EQ(actions.size(), 3) << "Should extract 3 unique action functions";
    EXPECT_TRUE(actions.count("initialize") > 0) << "Should extract initialize action";
    EXPECT_TRUE(actions.count("activate") > 0) << "Should extract activate action";
    EXPECT_TRUE(actions.count("deactivate") > 0) << "Should extract deactivate action";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}