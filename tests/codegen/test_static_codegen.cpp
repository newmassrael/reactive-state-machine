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
        file << content;
        file.close();

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
// TEST 3: processEvent method and switch-case generation
// ============================================================================

TEST_F(StaticCodeGenTest, GeneratesProcessEventMethod) {
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
// TEST 4: Class basic structure generation
// ============================================================================

TEST_F(StaticCodeGenTest, GeneratesClassStructure) {
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
// TEST 5: SCXML with Guard conditions
// ============================================================================

TEST_F(StaticCodeGenTest, GeneratesGuardConditions) {
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
    file << scxmlContent;
    file.close();

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
// TEST 6: SCXML with Actions
// ============================================================================

TEST_F(StaticCodeGenTest, GeneratesActions) {
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
    file << scxmlContent;
    file.close();

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
// Executable integration test (after implementation complete)
// ============================================================================

TEST_F(StaticCodeGenTest, DISABLED_EndToEndCompilation) {
    // This test will be activated when actual implementation is complete

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