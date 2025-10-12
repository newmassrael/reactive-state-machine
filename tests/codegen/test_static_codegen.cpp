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

TEST_F(StaticCodeGenTest, GeneratesTransitionLogic) {
    // Arrange: Simple SCXML with 2 states and 2 transitions
    std::string scxmlPath = createSimpleSCXML("transition.scxml");
    std::string outputDir = testDir_.string();

    // Act: Generate code
    StaticCodeGenerator generator;
    ASSERT_TRUE(generator.generate(scxmlPath, outputDir));

    // Assert: Verify transition logic in processEvent
    std::string generatedFile = (testDir_ / "SimpleSM_sm.h").string();
    ASSERT_TRUE(fs::exists(generatedFile));

    std::string content = readFile(generatedFile);

    // Should have switch statement
    EXPECT_TRUE(content.find("switch (currentState_)") != std::string::npos)
        << "Should generate switch statement for states";

    // Should have case for Idle state
    EXPECT_TRUE(content.find("case State::Idle:") != std::string::npos) << "Should have case for Idle state";

    // Should have case for Active state
    EXPECT_TRUE(content.find("case State::Active:") != std::string::npos) << "Should have case for Active state";

    // Should have event check for Start
    EXPECT_TRUE(content.find("event == Event::Start") != std::string::npos) << "Should check for Start event";

    // Should have event check for Stop
    EXPECT_TRUE(content.find("event == Event::Stop") != std::string::npos) << "Should check for Stop event";

    // Should have state transition
    EXPECT_TRUE(content.find("currentState_ = State::Active") != std::string::npos)
        << "Should transition to Active state";

    EXPECT_TRUE(content.find("currentState_ = State::Idle") != std::string::npos) << "Should transition to Idle state";
}

TEST_F(StaticCodeGenTest, GeneratesStrategyInterface) {
    // Arrange: SCXML with Guards and Actions
    std::string scxmlContent = R"XML(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" name="GuardedSM" initial="idle">
  <state id="idle">
    <transition event="start" cond="isReady()" target="active">
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
    <transition event="stop" cond="isValid()" target="idle"/>
  </state>
</scxml>)XML";

    std::string scxmlPath = (testDir_ / "guarded.scxml").string();
    std::ofstream file(scxmlPath);
    ASSERT_TRUE(file.is_open());
    file << scxmlContent;
    file.close();

    std::string outputDir = testDir_.string();

    // Act: Generate code
    StaticCodeGenerator generator;
    ASSERT_TRUE(generator.generate(scxmlPath, outputDir));

    // Assert: Verify strategy interface is generated
    std::string generatedFile = (testDir_ / "GuardedSM_sm.h").string();
    ASSERT_TRUE(fs::exists(generatedFile));

    std::string content = readFile(generatedFile);

    // Should have interface class declaration
    EXPECT_TRUE(content.find("class IGuardedSMLogic") != std::string::npos)
        << "Should generate interface class IGuardedSMLogic";

    // Should have virtual destructor
    EXPECT_TRUE(content.find("virtual ~IGuardedSMLogic()") != std::string::npos) << "Should have virtual destructor";

    // Should have Guard methods
    EXPECT_TRUE(content.find("virtual bool isReady()") != std::string::npos) << "Should have isReady guard method";

    EXPECT_TRUE(content.find("virtual bool isValid()") != std::string::npos) << "Should have isValid guard method";

    // Should have Action methods
    EXPECT_TRUE(content.find("virtual void initialize()") != std::string::npos)
        << "Should have initialize action method";

    EXPECT_TRUE(content.find("virtual void activate()") != std::string::npos) << "Should have activate action method";

    EXPECT_TRUE(content.find("virtual void deactivate()") != std::string::npos)
        << "Should have deactivate action method";

    // Should have pure virtual (= 0)
    EXPECT_TRUE(content.find("= 0") != std::string::npos) << "Should have pure virtual methods";
}

TEST_F(StaticCodeGenTest, GeneratesGuardConditions) {
    // Arrange: SCXML with Guard conditions
    std::string scxmlContent = R"XML(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" name="GuardedSM" initial="idle">
  <state id="idle">
    <transition event="start" cond="isReady()" target="active"/>
  </state>
  <state id="active">
    <transition event="stop" cond="isValid()" target="idle"/>
  </state>
</scxml>)XML";

    std::string scxmlPath = (testDir_ / "guarded_cond.scxml").string();
    std::ofstream file(scxmlPath);
    ASSERT_TRUE(file.is_open());
    file << scxmlContent;
    file.close();

    std::string outputDir = testDir_.string();

    // Act: Generate code
    StaticCodeGenerator generator;
    ASSERT_TRUE(generator.generate(scxmlPath, outputDir));

    // Assert: Verify guard conditions are generated in processEvent
    std::string generatedFile = (testDir_ / "GuardedSM_sm.h").string();
    ASSERT_TRUE(fs::exists(generatedFile));

    std::string content = readFile(generatedFile);

    // Should have logic_ pointer check
    EXPECT_TRUE(content.find("logic_->isReady()") != std::string::npos)
        << "Should call logic_->isReady() for guard check";

    EXPECT_TRUE(content.find("logic_->isValid()") != std::string::npos)
        << "Should call logic_->isValid() for guard check";

    // Should have nested if structure (event check, then guard check)
    EXPECT_TRUE(content.find("if (event == Event::Start)") != std::string::npos) << "Should check for Start event";

    // Verify the guard is inside the event check
    size_t startPos = content.find("if (event == Event::Start)");
    size_t guardPos = content.find("logic_->isReady()", startPos);
    size_t nextCasePos = content.find("case State::", startPos + 1);

    EXPECT_TRUE(guardPos < nextCasePos) << "Guard check should be inside the event check, before next case";
}

TEST_F(StaticCodeGenTest, GeneratesTransitionActions) {
    // Arrange: SCXML with transition actions
    std::string scxmlContent = R"XML(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" name="ActionSM" initial="idle">
  <state id="idle">
    <transition event="start" target="active">
      <script>initialize()</script>
    </transition>
  </state>
  <state id="active">
    <transition event="stop" target="idle">
      <script>cleanup()</script>
    </transition>
  </state>
</scxml>)XML";

    std::string scxmlPath = (testDir_ / "action_transition.scxml").string();
    std::ofstream file(scxmlPath);
    ASSERT_TRUE(file.is_open());
    file << scxmlContent;
    file.close();

    std::string outputDir = testDir_.string();

    // Act: Generate code
    StaticCodeGenerator generator;
    ASSERT_TRUE(generator.generate(scxmlPath, outputDir));

    // Assert: Verify transition actions are generated
    std::string generatedFile = (testDir_ / "ActionSM_sm.h").string();
    ASSERT_TRUE(fs::exists(generatedFile));

    std::string content = readFile(generatedFile);

    // Should have action calls
    EXPECT_TRUE(content.find("logic_->initialize()") != std::string::npos) << "Should call logic_->initialize() action";

    EXPECT_TRUE(content.find("logic_->cleanup()") != std::string::npos) << "Should call logic_->cleanup() action";

    // Verify action is called before state transition
    size_t initializePos = content.find("logic_->initialize()");
    size_t transitionPos = content.find("currentState_ = State::Active", initializePos);

    EXPECT_TRUE(initializePos < transitionPos) << "Action should be called before state transition";
}

TEST_F(StaticCodeGenTest, GeneratesEntryExitActions) {
    // Arrange: SCXML with onentry/onexit actions
    std::string scxmlContent = R"XML(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" name="EntrySM" initial="idle">
  <state id="idle">
    <onentry>
      <script>onEnterIdle()</script>
    </onentry>
    <onexit>
      <script>onExitIdle()</script>
    </onexit>
    <transition event="start" target="active">
      <script>doTransition()</script>
    </transition>
  </state>
  <state id="active">
    <onentry>
      <script>onEnterActive()</script>
    </onentry>
    <onexit>
      <script>onExitActive()</script>
    </onexit>
    <transition event="stop" target="idle"/>
  </state>
</scxml>)XML";

    std::string scxmlPath = (testDir_ / "entry_exit.scxml").string();
    std::ofstream file(scxmlPath);
    ASSERT_TRUE(file.is_open());
    file << scxmlContent;
    file.close();

    std::string outputDir = testDir_.string();

    // Act: Generate code
    StaticCodeGenerator generator;
    ASSERT_TRUE(generator.generate(scxmlPath, outputDir));

    // Assert: Verify entry/exit actions are generated
    std::string generatedFile = (testDir_ / "EntrySM_sm.h").string();
    ASSERT_TRUE(fs::exists(generatedFile));

    std::string content = readFile(generatedFile);

    // Should have all entry/exit action calls
    EXPECT_TRUE(content.find("logic_->onEnterIdle()") != std::string::npos) << "Should call logic_->onEnterIdle()";

    EXPECT_TRUE(content.find("logic_->onExitIdle()") != std::string::npos) << "Should call logic_->onExitIdle()";

    EXPECT_TRUE(content.find("logic_->onEnterActive()") != std::string::npos) << "Should call logic_->onEnterActive()";

    EXPECT_TRUE(content.find("logic_->onExitActive()") != std::string::npos) << "Should call logic_->onExitActive()";

    // Verify execution order for idle -> active transition
    size_t exitIdlePos = content.find("logic_->onExitIdle()");
    size_t transitionPos = content.find("logic_->doTransition()");
    size_t enterActivePos = content.find("logic_->onEnterActive()");
    size_t stateChangePos = content.find("currentState_ = State::Active", exitIdlePos);

    EXPECT_TRUE(exitIdlePos < transitionPos) << "onexit should be called before transition action";

    EXPECT_TRUE(transitionPos < enterActivePos) << "Transition action should be called before onentry";

    EXPECT_TRUE(enterActivePos < stateChangePos) << "onentry should be called before state variable update";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}