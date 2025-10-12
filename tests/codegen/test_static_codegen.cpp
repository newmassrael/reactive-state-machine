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

// ============================================================================
// TEST 1: Verify basic enum generation
// ============================================================================

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

// ============================================================================
// TEST 2: Verify Event enum generation
// ============================================================================

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

// ============================================================================
// TEST 3: Parse actual SCXML file (TDD RED - Phase B)
// ============================================================================

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

// ============================================================================
// TEST 4: processEvent method and switch-case generation
// Status: DISABLED - Waiting for Phase C implementation
// Will be enabled when transition logic generation is implemented
// ============================================================================

TEST_F(StaticCodeGenTest, DISABLED_GeneratesProcessEventMethod) {
    // Arrange
    std::string scxmlPath = createSimpleSCXML("simple.scxml");
    std::string outputDir = testDir_.string();

    // Act
    // StaticCodeGenerator generator;
    // generator.generate(scxmlPath, outputDir);

    // Assert: Verify processEvent method structure
    std::string expectedCode = R"(
void processEvent(Event event) {
    switch(currentState_) {
        case State::Idle:
            if (event == Event::Start) {
                currentState_ = State::Active;
            }
            break;
        case State::Active:
            if (event == Event::Stop) {
                currentState_ = State::Idle;
            }
            break;
    }
})";

    // Compare with actual generated code
    // std::string content = readFile((testDir_ / "SimpleSM_sm.h").string());
    // EXPECT_TRUE(content.find("void processEvent(Event event)") != std::string::npos);
    // EXPECT_TRUE(content.find("switch(currentState_)") != std::string::npos);
}

// ============================================================================
// TEST 5: Class basic structure generation
// Status: DISABLED - Waiting for Phase C implementation
// Will be enabled when full class generation is implemented
// ============================================================================

TEST_F(StaticCodeGenTest, DISABLED_GeneratesClassStructure) {
    // Arrange
    std::string scxmlPath = createSimpleSCXML("simple.scxml");

    // Act
    // StaticCodeGenerator generator;
    // std::string generated = generator.generateClass(scxmlPath);

    // Assert: Expected class structure
    std::string expectedStructure = R"(
template<typename LogicType>
class SimpleSM {
private:
    State currentState_ = State::Idle;
    std::unique_ptr<LogicType> logic_;
    
public:
    void processEvent(Event event);
    State getCurrentState() const { return currentState_; }
};
)";

    // Verify (after actual implementation)
    // EXPECT_TRUE(generated.find("class SimpleSM") != std::string::npos);
    // EXPECT_TRUE(generated.find("State currentState_") != std::string::npos);
    // EXPECT_TRUE(generated.find("getCurrentState()") != std::string::npos);
}

// ============================================================================
// TEST 6: SCXML with Guard conditions
// Status: DISABLED - Phase C-1 task
// Will be enabled when Guard condition extraction and interface generation is implemented
// ============================================================================

TEST_F(StaticCodeGenTest, DISABLED_GeneratesGuardConditions) {
    // Arrange: SCXML with Guard conditions
    std::string scxmlContent = R"XML(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" name="GuardedSM" initial="idle">
  <state id="idle">
    <transition event="check" cond="isReady()" target="active"/>
  </state>
  <state id="active">
    <transition event="check" cond="!isReady()" target="idle"/>
  </state>
</scxml>)XML";

    std::string guardedScxmlPath = (testDir_ / "guarded.scxml").string();
    std::ofstream file(guardedScxmlPath);
    if (!file) {
        throw std::runtime_error("Failed to create test SCXML file: " + guardedScxmlPath);
    }
    file << scxmlContent;

    // Act
    // StaticCodeGenerator generator;
    // generator.generate(guardedScxmlPath, testDir_.string());

    // Assert: Verify Guard interface generation
    std::string interfaceFile = (testDir_ / "GuardedSM_interface.h").string();

    // Expected interface
    std::string expectedInterface = R"(
struct IGuardedSMLogic {
    virtual ~IGuardedSMLogic() = default;
    virtual bool isReady() const = 0;
};
)";

    // Verify after actual implementation
    // std::string content = readFile(interfaceFile);
    // EXPECT_TRUE(content.find("virtual bool isReady() const = 0") != std::string::npos);
}

// ============================================================================
// TEST 7: SCXML with Actions
// Status: DISABLED - Phase C-2 task
// Will be enabled when Action extraction and interface generation is implemented
// ============================================================================

TEST_F(StaticCodeGenTest, DISABLED_GeneratesActions) {
    // Arrange: SCXML with Actions
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
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
</scxml>)";

    std::string scxmlPath = (testDir_ / "action.scxml").string();
    std::ofstream file(scxmlPath);
    if (!file) {
        throw std::runtime_error("Failed to create test SCXML file: " + scxmlPath);
    }
    file << scxmlContent;

    // Act
    // StaticCodeGenerator generator;
    // generator.generate(scxmlPath, testDir_.string());

    // Assert: Verify Action interface generation
    std::string expectedInterface = R"(
struct IActionSMLogic {
    virtual void initialize() = 0;
    virtual void activate() = 0;
    virtual void deactivate() = 0;
};
)";

    // Verify after actual implementation
}

// ============================================================================
// TEST 8: End-to-end compilation test
// Status: DISABLED - Phase D task
// Will be enabled when full code generation pipeline is complete
// ============================================================================

TEST_F(StaticCodeGenTest, DISABLED_EndToEndCompilation) {
    // This test will be activated when full implementation is complete

    // 1. Create SCXML
    std::string scxmlPath = createSimpleSCXML("e2e.scxml");

    // 2. Generate code
    // StaticCodeGenerator generator;
    // generator.generate(scxmlPath, testDir_.string());

    // 3. Test compilation of generated code
    std::string testProgram = R"(
#include "SimpleSM_sm.h"

struct TestLogic {};

int main() {
    SimpleSM<TestLogic> sm;
    sm.processEvent(Event::Start);
    return sm.getCurrentState() == State::Active ? 0 : 1;
}
)";

    // 4. Compile and execute
    // Actually compile with g++ and verify execution result
}

// ============================================================================
// Main function
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}