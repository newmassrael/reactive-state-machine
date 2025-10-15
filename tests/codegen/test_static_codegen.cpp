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

    // Should have Policy struct
    EXPECT_TRUE(content.find("struct SimpleSMPolicy") != std::string::npos) << "Should generate Policy struct";

    // Should have processTransition method in Policy
    EXPECT_TRUE(content.find("static bool processTransition") != std::string::npos)
        << "Should have processTransition in Policy";

    // Should have switch statement (note: currentState is parameter, not member)
    EXPECT_TRUE(content.find("switch (currentState)") != std::string::npos)
        << "Should generate switch statement for states";

    // Should have case for Idle state
    EXPECT_TRUE(content.find("case State::Idle:") != std::string::npos) << "Should have case for Idle state";

    // Should have case for Active state
    EXPECT_TRUE(content.find("case State::Active:") != std::string::npos) << "Should have case for Active state";

    // Should have event check for Start
    EXPECT_TRUE(content.find("event == Event::Start") != std::string::npos) << "Should check for Start event";

    // Should have event check for Stop
    EXPECT_TRUE(content.find("event == Event::Stop") != std::string::npos) << "Should check for Stop event";

    // Should have state transition (note: currentState is parameter reference)
    EXPECT_TRUE(content.find("currentState = State::Active") != std::string::npos)
        << "Should transition to Active state";

    EXPECT_TRUE(content.find("currentState = State::Idle") != std::string::npos) << "Should transition to Idle state";

    // Should inherit from StaticExecutionEngine
    EXPECT_TRUE(content.find("StaticExecutionEngine<SimpleSMPolicy>") != std::string::npos)
        << "Should inherit from StaticExecutionEngine";
}

// This test is obsolete with CRTP pattern - Strategy Interface is no longer generated
TEST_F(StaticCodeGenTest, DISABLED_GeneratesStrategyInterface) {
    // CRTP pattern does not generate Strategy Interface
    // This test is disabled as it's no longer applicable
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

    // Should call derived() for guard check
    EXPECT_TRUE(content.find("derived().isReady()") != std::string::npos)
        << "Should call derived().isReady() for guard check";

    EXPECT_TRUE(content.find("derived().isValid()") != std::string::npos)
        << "Should call derived().isValid() for guard check";

    // Should have nested if structure (event check, then guard check)
    EXPECT_TRUE(content.find("if (event == Event::Start)") != std::string::npos) << "Should check for Start event";

    // Verify the guard is inside the event check
    size_t startPos = content.find("if (event == Event::Start)");
    size_t guardPos = content.find("derived().isReady()", startPos);
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

    // Should have action calls using derived()
    EXPECT_TRUE(content.find("derived().initialize()") != std::string::npos)
        << "Should call derived().initialize() action";

    EXPECT_TRUE(content.find("derived().cleanup()") != std::string::npos) << "Should call derived().cleanup() action";

    // Verify action is called before state transition
    size_t initializePos = content.find("derived().initialize()");
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

    // Should have executeEntryActions and executeExitActions in Policy
    EXPECT_TRUE(content.find("static void executeEntryActions") != std::string::npos)
        << "Should have executeEntryActions in Policy";

    EXPECT_TRUE(content.find("static void executeExitActions") != std::string::npos)
        << "Should have executeExitActions in Policy";

    // Should have cases for Idle and Active states in entry actions
    EXPECT_TRUE(content.find("case State::Idle:") != std::string::npos)
        << "Should have case for Idle state in entry/exit actions";

    EXPECT_TRUE(content.find("case State::Active:") != std::string::npos)
        << "Should have case for Active state in entry/exit actions";

    // Should have function calls (note: in Policy pattern, these are direct calls, not derived())
    EXPECT_TRUE(content.find("onEnterIdle()") != std::string::npos) << "Should call onEnterIdle()";

    EXPECT_TRUE(content.find("onExitIdle()") != std::string::npos) << "Should call onExitIdle()";

    EXPECT_TRUE(content.find("onEnterActive()") != std::string::npos) << "Should call onEnterActive()";

    EXPECT_TRUE(content.find("onExitActive()") != std::string::npos) << "Should call onExitActive()";

    EXPECT_TRUE(content.find("doTransition()") != std::string::npos) << "Should call doTransition()";

    // Note: Execution order is guaranteed by StaticExecutionEngine, not in generated code
    // The engine ensures: executeOnExit -> transition -> executeOnEntry
}

TEST_F(StaticCodeGenTest, GeneratesInitializeMethod) {
    // Arrange: SCXML with initial state having onentry action
    std::string scxmlContent = R"XML(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" name="InitSM" initial="idle">
  <state id="idle">
    <onentry>
      <script>onEnterIdle()</script>
    </onentry>
    <transition event="start" target="active"/>
  </state>
  <state id="active">
    <onentry>
      <script>onEnterActive()</script>
    </onentry>
  </state>
</scxml>)XML";

    std::string scxmlPath = (testDir_ / "init_test.scxml").string();
    std::ofstream file(scxmlPath);
    ASSERT_TRUE(file.is_open());
    file << scxmlContent;
    file.close();

    std::string outputDir = testDir_.string();

    // Act: Generate code
    StaticCodeGenerator generator;
    ASSERT_TRUE(generator.generate(scxmlPath, outputDir));

    // Assert: Verify initialize method is generated
    std::string generatedFile = (testDir_ / "InitSM_sm.h").string();
    ASSERT_TRUE(fs::exists(generatedFile));

    std::string content = readFile(generatedFile);

    // Should have Policy struct
    EXPECT_TRUE(content.find("struct InitSMPolicy") != std::string::npos) << "Should generate Policy struct";

    // Should have initialState method in Policy
    EXPECT_TRUE(content.find("static State initialState()") != std::string::npos)
        << "Policy should have initialState() method";

    // Should return Idle as initial state
    EXPECT_TRUE(content.find("return State::Idle") != std::string::npos) << "initialState() should return State::Idle";

    // Should have executeEntryActions in Policy
    EXPECT_TRUE(content.find("static void executeEntryActions") != std::string::npos)
        << "Policy should have executeEntryActions";

    // Should handle entry action for Idle state
    EXPECT_TRUE(content.find("case State::Idle:") != std::string::npos)
        << "executeEntryActions should have case for Idle state";

    EXPECT_TRUE(content.find("onEnterIdle()") != std::string::npos) << "Should call onEnterIdle() for Idle state entry";

    // Should inherit from StaticExecutionEngine (which provides initialize())
    EXPECT_TRUE(content.find("StaticExecutionEngine<InitSMPolicy>") != std::string::npos)
        << "Should inherit from StaticExecutionEngine";
}

TEST_F(StaticCodeGenTest, GeneratesPolicyPattern) {
    // Arrange: SCXML with guards and actions
    std::string scxmlContent = R"XML(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" name="PolicySM" initial="idle">
  <state id="idle">
    <onentry>
      <script>onEnter()</script>
    </onentry>
    <transition event="start" cond="isReady()" target="active">
      <script>doAction()</script>
    </transition>
  </state>
  <state id="active"/>
</scxml>)XML";

    std::string scxmlPath = (testDir_ / "policy_test.scxml").string();
    std::ofstream file(scxmlPath);
    ASSERT_TRUE(file.is_open());
    file << scxmlContent;
    file.close();

    std::string outputDir = testDir_.string();

    // Act: Generate code
    StaticCodeGenerator generator;
    ASSERT_TRUE(generator.generate(scxmlPath, outputDir));

    // Assert: Verify Policy pattern is generated
    std::string generatedFile = (testDir_ / "PolicySM_sm.h").string();
    ASSERT_TRUE(fs::exists(generatedFile));

    std::string content = readFile(generatedFile);

    // Should generate Policy struct
    EXPECT_TRUE(content.find("struct PolicySMPolicy") != std::string::npos) << "Should generate Policy struct";

    // Should have static methods in Policy
    EXPECT_TRUE(content.find("static bool processTransition") != std::string::npos)
        << "Policy should have processTransition method";

    EXPECT_TRUE(content.find("static void executeEntryActions") != std::string::npos)
        << "Policy should have executeEntryActions method";

    // Should call guards/actions directly (without derived() prefix)
    EXPECT_TRUE(content.find("isReady()") != std::string::npos) << "Should call isReady() guard";

    EXPECT_TRUE(content.find("doAction()") != std::string::npos) << "Should call doAction() action";

    EXPECT_TRUE(content.find("onEnter()") != std::string::npos) << "Should call onEnter() entry action";

    // Should inherit from StaticExecutionEngine
    EXPECT_TRUE(content.find("StaticExecutionEngine<PolicySMPolicy>") != std::string::npos)
        << "Should inherit from StaticExecutionEngine with Policy";

    // Should NOT have CRTP patterns
    EXPECT_TRUE(content.find("template<typename Derived>") == std::string::npos)
        << "Should NOT use CRTP template parameter";

    EXPECT_TRUE(content.find("Derived& derived()") == std::string::npos) << "Should NOT have derived() helper method";
}

TEST_F(StaticCodeGenTest, GeneratesSendWithContent) {
    // Arrange: SCXML with <send><content> (W3C SCXML 5.10, test179)
    std::string scxmlContent = R"XML(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" datamodel="ecmascript" name="ContentSM" initial="s0">
  <state id="s0">
    <onentry>
      <send event="event1">
        <content>123</content>
      </send>
    </onentry>
    <transition event="event1" cond="_event.data == 123" target="pass"/>
    <transition event="*" target="fail"/>
  </state>
  <final id="pass"/>
  <final id="fail"/>
</scxml>)XML";

    std::string scxmlPath = (testDir_ / "content_test.scxml").string();
    std::ofstream file(scxmlPath);
    ASSERT_TRUE(file.is_open());
    file << scxmlContent;
    file.close();

    std::string outputDir = testDir_.string();

    // Act: Generate code
    StaticCodeGenerator generator;
    ASSERT_TRUE(generator.generate(scxmlPath, outputDir));

    // Assert: Verify content support is generated
    std::string generatedFile = (testDir_ / "ContentSM_sm.h").string();
    ASSERT_TRUE(fs::exists(generatedFile));

    std::string content = readFile(generatedFile);

    // Should generate stateful policy (content requires event data support)
    EXPECT_TRUE(content.find("mutable ::std::string pendingEventData_") != std::string::npos)
        << "Should have pendingEventData_ for event data storage";

    // Should have setEventDataInJSEngine helper
    EXPECT_TRUE(content.find("setEventDataInJSEngine") != std::string::npos)
        << "Should have setEventDataInJSEngine helper method";

    // Should pass content as event data in raise call
    EXPECT_TRUE(content.find("engine.raise(Event::Event1, \"123\")") != std::string::npos)
        << "Should pass content data \"123\" to raise()";

    // Should detect _event in guard condition and use JSEngine
    EXPECT_TRUE(content.find("::RSM::GuardHelper::evaluateGuard") != std::string::npos)
        << "Should use GuardHelper for _event.data condition";

    EXPECT_TRUE(content.find("\"_event.data == 123\"") != std::string::npos)
        << "Should evaluate condition via JSEngine";

    // Should have JSEngine initialization
    EXPECT_TRUE(content.find("ensureJSEngine()") != std::string::npos)
        << "Should call ensureJSEngine() for JSEngine setup";
}

TEST_F(StaticCodeGenTest, GeneratesSendWithParams) {
    // Arrange: SCXML with <send><param> (W3C SCXML 5.10, test176)
    std::string scxmlContent = R"XML(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" datamodel="ecmascript" name="ParamSM" initial="s0">
  <datamodel>
    <data id="Var1" expr="42"/>
  </datamodel>
  <state id="s0">
    <onentry>
      <send event="event1">
        <param name="aParam" expr="Var1"/>
      </send>
    </onentry>
    <transition event="event1" target="pass"/>
  </state>
  <final id="pass"/>
</scxml>)XML";

    std::string scxmlPath = (testDir_ / "param_test.scxml").string();
    std::ofstream file(scxmlPath);
    ASSERT_TRUE(file.is_open());
    file << scxmlContent;
    file.close();

    std::string outputDir = testDir_.string();

    // Act: Generate code
    StaticCodeGenerator generator;
    ASSERT_TRUE(generator.generate(scxmlPath, outputDir));

    // Assert: Verify param support is generated
    std::string generatedFile = (testDir_ / "ParamSM_sm.h").string();
    ASSERT_TRUE(fs::exists(generatedFile));

    std::string content = readFile(generatedFile);

    // Should use EventDataHelper for param JSON construction
    EXPECT_TRUE(content.find("::RSM::EventDataHelper::buildJsonFromParams") != std::string::npos)
        << "Should use EventDataHelper::buildJsonFromParams()";

    // Should have params map
    EXPECT_TRUE(content.find("std::map<std::string, std::vector<std::string>> params") != std::string::npos)
        << "Should create params map for event data";

    // Should add param to map
    EXPECT_TRUE(content.find("params[\"aParam\"]") != std::string::npos) << "Should add aParam to params map";

    // Should pass eventData to raise
    EXPECT_TRUE(content.find("engine.raise(Event::Event1, eventData)") != std::string::npos)
        << "Should pass eventData from params to raise()";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}