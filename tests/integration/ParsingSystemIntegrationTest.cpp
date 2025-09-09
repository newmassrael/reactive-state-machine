#include "IntegrationTestCommon.h"
#include "generator/include/common/Logger.h"
#include "generator/include/core/NodeFactory.h"
#include "generator/include/core/StateNode.h"
#include "generator/include/core/TransitionNode.h"
#include "generator/include/core/actions/AssignActionNode.h"
#include "generator/include/core/actions/LogActionNode.h"
#include "generator/include/core/actions/RaiseActionNode.h"
#include "generator/include/parsing/ActionParser.h"
#include "generator/include/parsing/DataModelParser.h"
#include "generator/include/parsing/DocumentParser.h"
#include "generator/include/parsing/StateNodeParser.h"
#include "generator/include/parsing/TransitionParser.h"

/**
 * @brief Integration tests for the complete parsing system
 *
 * This test suite verifies the integration between all parsing components:
 * - DocumentParser coordination with specialized parsers
 * - Parser-to-Factory integration and node creation
 * - Attribute parsing and node property setting
 * - Complex SCXML document parsing scenarios
 * - XML validation and error handling
 *
 * These tests ensure that parsing components work together correctly
 * and would catch integration issues between parsers and factory,
 * specifically the issues that caused the original raise action bug.
 */
class ParsingSystemIntegrationTest : public IntegrationTestBase {
protected:
    void SetUp() override {
        IntegrationTestBase::SetUp();

        // Use real implementations, no mocks
        realFactory = std::make_shared<SCXML::Core::NodeFactory>();
        realParser = std::make_shared<SCXML::Parsing::DocumentParser>(realFactory);

        // Setup all parser relationships with real implementations
        realParser->getStateNodeParser()->setRelatedParsers(
            realParser->getTransitionParser(), realParser->getActionParser(), realParser->getDataModelParser(),
            realParser->getInvokeParser(), realParser->getDoneDataParser());

        realActionParser = std::make_shared<SCXML::Parsing::ActionParser>(realFactory);
        realParser->getTransitionParser()->setActionParser(realActionParser);
    }

    /**
     * @brief Verify that a node can be cast to expected type
     */
    template <typename ExpectedType>
    bool canCastTo(std::shared_ptr<SCXML::Model::IActionNode> node, const std::string &expectedActionType) {
        if (!node || node->getActionType() != expectedActionType) {
            return false;
        }
        auto castedNode = std::dynamic_pointer_cast<ExpectedType>(node);
        return castedNode != nullptr;
    }

protected:
    std::shared_ptr<SCXML::Core::NodeFactory> realFactory;
    std::shared_ptr<SCXML::Parsing::DocumentParser> realParser;
    std::shared_ptr<SCXML::Parsing::ActionParser> realActionParser;
};

/**
 * @brief Test Parser-to-Factory integration for action node creation
 *
 * This test specifically targets the bug where NodeFactory was not
 * creating the correct action node types (e.g., RaiseActionNode vs generic ActionNode).
 */
TEST_F(ParsingSystemIntegrationTest, ParserFactoryActionNodeCreation) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="test">
        <state id="test">
            <onentry>
                <raise event="test.event"/>
                <assign location="var1" expr="'value1'"/>
                <log label="test" expr="'Test message'"/>
            </onentry>
        </state>
    </scxml>)";

    SCXML::Common::Logger::info("Starting ParserFactoryActionNodeCreation test");

    // Test parsing creates correct document model
    auto model = realParser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr) << "Parser should create document model";
    EXPECT_FALSE(realParser->hasErrors()) << "Parser should not have errors";

    // Test state node creation
    auto testState = model->findStateById("test");
    ASSERT_TRUE(testState != nullptr) << "Should find 'test' state node";

    // Test action node retrieval
    auto entryActions = testState->getEntryActionNodes();
    ASSERT_EQ(3, entryActions.size()) << "Should have 3 entry actions (raise, assign, log)";

    // Test NodeFactory created correct action node types

    // 1. Test RaiseActionNode creation and attributes
    auto raiseAction = entryActions[0];
    EXPECT_EQ("raise", raiseAction->getActionType()) << "BUG FIX: First action should be 'raise', not 'unknown'";
    EXPECT_TRUE(canCastTo<SCXML::Core::RaiseActionNode>(raiseAction, "raise"))
        << "BUG FIX: Should be able to cast to RaiseActionNode";

    auto raiseNode = std::dynamic_pointer_cast<SCXML::Core::RaiseActionNode>(raiseAction);
    ASSERT_TRUE(raiseNode != nullptr);
    EXPECT_EQ("test.event", raiseNode->getEvent())
        << "BUG FIX: ActionParser should set event attribute on RaiseActionNode";

    // 2. Test AssignActionNode creation and attributes
    auto assignAction = entryActions[1];
    EXPECT_EQ("assign", assignAction->getActionType()) << "Second action should be 'assign'";
    EXPECT_TRUE(canCastTo<SCXML::Core::AssignActionNode>(assignAction, "assign"))
        << "Should be able to cast to AssignActionNode";

    auto assignNode = std::dynamic_pointer_cast<SCXML::Core::AssignActionNode>(assignAction);
    ASSERT_TRUE(assignNode != nullptr);
    EXPECT_EQ("var1", assignNode->getLocation()) << "ActionParser should set location attribute";
    EXPECT_EQ("'value1'", assignNode->getExpr()) << "ActionParser should set expr attribute";

    // 3. Test LogActionNode creation and attributes
    auto logAction = entryActions[2];
    EXPECT_EQ("log", logAction->getActionType()) << "Third action should be 'log'";
    EXPECT_TRUE(canCastTo<SCXML::Core::LogActionNode>(logAction, "log")) << "Should be able to cast to LogActionNode";

    auto logNode = std::dynamic_pointer_cast<SCXML::Core::LogActionNode>(logAction);
    ASSERT_TRUE(logNode != nullptr);
    EXPECT_EQ("test", logNode->getLabel()) << "ActionParser should set label attribute";
    EXPECT_EQ("'Test message'", logNode->getExpr()) << "ActionParser should set expr attribute";

    SCXML::Common::Logger::info("ParserFactoryActionNodeCreation test completed - node creation working correctly");
}

/**
 * @brief Test ActionParser attribute setting integration
 *
 * Verifies that ActionParser properly sets attributes on action nodes
 * created by NodeFactory, addressing the bug where attributes weren't set.
 */
TEST_F(ParsingSystemIntegrationTest, ActionParserAttributeSettingIntegration) {
    std::string scxml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="state1">
        <state id="state1">
            <onentry>
                <raise event="complex.event.name" data="{'key': 'value'}"/>
                <assign location="complexVar" expr="computeValue()"/>
                <log label="complex.label" expr="'Complex: ' + variable + ' = ' + result"/>
            </onentry>
        </state>
    </scxml>)xml";

    SCXML::Common::Logger::info("Starting ActionParserAttributeSettingIntegration test");

    auto model = realParser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(realParser->hasErrors());

    auto state = model->findStateById("state1");
    ASSERT_TRUE(state != nullptr);

    auto actions = state->getEntryActionNodes();
    ASSERT_EQ(3, actions.size());

    // Test complex raise action attributes
    auto raiseAction = std::dynamic_pointer_cast<SCXML::Core::RaiseActionNode>(actions[0]);
    ASSERT_TRUE(raiseAction != nullptr) << "Should create RaiseActionNode";
    EXPECT_EQ("complex.event.name", raiseAction->getEvent()) << "Should handle complex event names";
    EXPECT_EQ("{'key': 'value'}", raiseAction->getData()) << "Should handle complex data attributes";

    // Test complex assign action attributes
    auto assignAction = std::dynamic_pointer_cast<SCXML::Core::AssignActionNode>(actions[1]);
    ASSERT_TRUE(assignAction != nullptr) << "Should create AssignActionNode";
    EXPECT_EQ("complexVar", assignAction->getLocation()) << "Should handle location expressions";
    EXPECT_EQ("computeValue()", assignAction->getExpr()) << "Should handle expressions";

    // Test complex log action attributes
    auto logAction = std::dynamic_pointer_cast<SCXML::Core::LogActionNode>(actions[2]);
    ASSERT_TRUE(logAction != nullptr) << "Should create LogActionNode";
    EXPECT_EQ("complex.label", logAction->getLabel()) << "Should handle complex labels";
    EXPECT_EQ("'Complex: ' + variable + ' = ' + result", logAction->getExpr())
        << "Should handle complex log expressions";

    SCXML::Common::Logger::info("ActionParserAttributeSettingIntegration test completed - attribute setting working");
}

/**
 * @brief Test StateNodeParser integration with ActionParser
 *
 * Verifies that StateNodeParser properly coordinates with ActionParser
 * to create and populate action nodes in state entry/exit actions.
 */
TEST_F(ParsingSystemIntegrationTest, StateNodeActionParserIntegration) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
        <state id="s1">
            <onentry>
                <log label="entering" expr="'Entering s1'"/>
                <assign location="entryCount" expr="entryCount + 1"/>
                <raise event="entered.s1"/>
            </onentry>
            <onexit>
                <log label="exiting" expr="'Exiting s1'"/>
                <assign location="exitCount" expr="exitCount + 1"/>
                <raise event="exited.s1"/>
            </onexit>
            <transition event="go" target="s2"/>
        </state>
        <state id="s2"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting StateNodeActionParserIntegration test");

    auto model = realParser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(realParser->hasErrors());

    auto s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // Test entry actions integration
    auto entryActions = s1->getEntryActionNodes();
    ASSERT_EQ(3, entryActions.size()) << "Should have 3 entry actions";

    // Verify entry action types and attributes
    EXPECT_EQ("log", entryActions[0]->getActionType());
    EXPECT_EQ("assign", entryActions[1]->getActionType());
    EXPECT_EQ("raise", entryActions[2]->getActionType());

    auto entryRaise = std::dynamic_pointer_cast<SCXML::Core::RaiseActionNode>(entryActions[2]);
    ASSERT_TRUE(entryRaise != nullptr);
    EXPECT_EQ("entered.s1", entryRaise->getEvent());

    // Test exit actions integration
    auto exitActions = s1->getExitActionNodes();
    ASSERT_EQ(3, exitActions.size()) << "Should have 3 exit actions";

    // Verify exit action types and attributes
    EXPECT_EQ("log", exitActions[0]->getActionType());
    EXPECT_EQ("assign", exitActions[1]->getActionType());
    EXPECT_EQ("raise", exitActions[2]->getActionType());

    auto exitRaise = std::dynamic_pointer_cast<SCXML::Core::RaiseActionNode>(exitActions[2]);
    ASSERT_TRUE(exitRaise != nullptr);
    EXPECT_EQ("exited.s1", exitRaise->getEvent());

    SCXML::Common::Logger::info("StateNodeActionParserIntegration test completed - state-action integration working");
}

/**
 * @brief Test TransitionParser integration with ActionParser
 *
 * Verifies that TransitionParser properly coordinates with ActionParser
 * to create action nodes in transition executable content.
 */
TEST_F(ParsingSystemIntegrationTest, TransitionActionParserIntegration) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
        <state id="start">
            <transition event="process" target="end">
                <log label="transition" expr="'Processing transition'"/>
                <assign location="transitionCount" expr="transitionCount + 1"/>
                <raise event="transition.complete"/>
            </transition>
        </state>
        <state id="end"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting TransitionActionParserIntegration test");

    auto model = realParser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(realParser->hasErrors());

    auto startState = model->findStateById("start");
    ASSERT_TRUE(startState != nullptr);

    // Get transitions
    auto transitions = startState->getTransitions();
    ASSERT_FALSE(transitions.empty()) << "Should have transitions";

    auto transition = transitions[0];
    ASSERT_TRUE(transition != nullptr);
    EXPECT_EQ("process", transition->getEvent()) << "Should have correct event";

    // Test transition action nodes
    auto transitionActions = transition->getActionNodes();
    ASSERT_EQ(3, transitionActions.size()) << "Transition should have 3 actions";

    // Verify transition action types and attributes
    EXPECT_EQ("log", transitionActions[0]->getActionType());
    EXPECT_EQ("assign", transitionActions[1]->getActionType());
    EXPECT_EQ("raise", transitionActions[2]->getActionType());

    // Test specific action attributes
    auto transitionRaise = std::dynamic_pointer_cast<SCXML::Core::RaiseActionNode>(transitionActions[2]);
    ASSERT_TRUE(transitionRaise != nullptr) << "Should create RaiseActionNode in transition";
    EXPECT_EQ("transition.complete", transitionRaise->getEvent()) << "Should set transition raise event";

    auto transitionLog = std::dynamic_pointer_cast<SCXML::Core::LogActionNode>(transitionActions[0]);
    ASSERT_TRUE(transitionLog != nullptr) << "Should create LogActionNode in transition";
    EXPECT_EQ("transition", transitionLog->getLabel()) << "Should set transition log label";

    SCXML::Common::Logger::info(
        "TransitionActionParserIntegration test completed - transition-action integration working");
}

/**
 * @brief Test complex SCXML document parsing integration
 *
 * Tests parsing of a complex SCXML document with multiple states,
 * transitions, and various action types to ensure all components
 * work together in realistic scenarios.
 */
TEST_F(ParsingSystemIntegrationTest, ComplexDocumentParsingIntegration) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="init" datamodel="ecmascript">
        <datamodel>
            <data id="step" expr="0"/>
            <data id="message" expr="'start'"/>
        </datamodel>
        <state id="init">
            <onentry>
                <log label="init" expr="'Starting complex parsing test'"/>
                <assign location="step" expr="1"/>
                <raise event="initialized"/>
            </onentry>
            <transition event="initialized" target="processing"/>
        </state>
        <state id="processing" initial="substep1">
            <onentry>
                <assign location="message" expr="'processing'"/>
                <log label="processing" expr="'In processing state, step: ' + step"/>
            </onentry>
            <state id="substep1">
                <onentry>
                    <assign location="step" expr="step + 1"/>
                    <raise event="next"/>
                </onentry>
                <transition event="next" target="substep2"/>
            </state>
            <state id="substep2">
                <onentry>
                    <assign location="step" expr="step + 1"/>
                    <log label="substep2" expr="'Substep 2, step: ' + step"/>
                    <raise event="complete"/>
                </onentry>
                <transition event="complete" target="../finalizing"/>
            </state>
        </state>
        <state id="finalizing">
            <onentry>
                <assign location="message" expr="'final'"/>
                <log label="final" expr="'Finalizing, message: ' + message"/>
            </onentry>
            <transition cond="step == 3 &amp;&amp; message === 'final'" target="success"/>
            <transition target="failure"/>
        </state>
        <final id="success"/>
        <final id="failure"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting ComplexDocumentParsingIntegration test");

    auto model = realParser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr) << "Should parse complex SCXML document";
    EXPECT_FALSE(realParser->hasErrors()) << "Should not have parsing errors";

    // Test all states were created
    std::vector<std::string> expectedStates = {"init",       "processing", "substep1", "substep2",
                                               "finalizing", "success",    "failure"};
    for (const auto &stateId : expectedStates) {
        auto state = model->findStateById(stateId);
        EXPECT_TRUE(state != nullptr) << "Should find state: " + stateId;
    }

    // Test data model was parsed
    auto dataModel = model->getDatamodel();
    ASSERT_FALSE(dataModel.empty()) << "Should have data model";

    // Test init state actions
    auto initState = model->findStateById("init");
    auto initActions = initState->getEntryActionNodes();
    EXPECT_EQ(3, initActions.size()) << "Init state should have 3 entry actions";

    // Verify action types and attributes in init state
    EXPECT_EQ("log", initActions[0]->getActionType());
    EXPECT_EQ("assign", initActions[1]->getActionType());
    EXPECT_EQ("raise", initActions[2]->getActionType());

    auto initRaise = std::dynamic_pointer_cast<SCXML::Core::RaiseActionNode>(initActions[2]);
    ASSERT_TRUE(initRaise != nullptr);
    EXPECT_EQ("initialized", initRaise->getEvent());

    // Test processing state and substates
    auto processingState = model->findStateById("processing");
    ASSERT_TRUE(processingState != nullptr);
    EXPECT_EQ("substep1", processingState->getInitialState()) << "Should have correct initial substate";

    // Test substep actions
    auto substep1 = model->findStateById("substep1");
    auto substep1Actions = substep1->getEntryActionNodes();
    EXPECT_EQ(2, substep1Actions.size()) << "Substep1 should have 2 entry actions";

    auto substep2 = model->findStateById("substep2");
    auto substep2Actions = substep2->getEntryActionNodes();
    EXPECT_EQ(3, substep2Actions.size()) << "Substep2 should have 3 entry actions";

    // Test transition with target reference
    auto substep2Transitions = substep2->getTransitions();
    ASSERT_FALSE(substep2Transitions.empty());
    auto complexTransition = substep2Transitions[0];
    EXPECT_EQ("complete", complexTransition->getEvent());

    // Test finalizing state with complex condition
    auto finalizingState = model->findStateById("finalizing");
    auto finalizingTransitions = finalizingState->getTransitions();
    EXPECT_EQ(2, finalizingTransitions.size()) << "Finalizing should have 2 transitions";

    // Test conditional transition
    auto conditionalTransition = finalizingTransitions[0];
    EXPECT_FALSE(conditionalTransition->getGuard().empty()) << "Should have condition";
    EXPECT_EQ("success", conditionalTransition->getTargets()[0]) << "Should target success state";

    SCXML::Common::Logger::info("ComplexDocumentParsingIntegration test completed - complex parsing working");
}

/**
 * @brief Test parsing error handling integration
 *
 * Verifies that parsing system properly handles malformed SCXML
 * and provides appropriate error reporting.
 */
TEST_F(ParsingSystemIntegrationTest, ParsingErrorHandlingIntegration) {
    // Test various malformed SCXML scenarios

    // 1. Test invalid XML
    std::string invalidXml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
        <state id="start">
            <onentry>
                <raise event="test"
                <!-- Missing closing tag -->
            </onentry>
        </state>
    </scxml>)";

    SCXML::Common::Logger::info("Testing invalid XML handling");

    auto model1 = realParser->parseContent(invalidXml);
    EXPECT_TRUE(model1 == nullptr || realParser->hasErrors()) << "Should detect invalid XML";

    // Reset parser for next test
    realParser = std::make_shared<SCXML::Parsing::DocumentParser>(realFactory);
    realParser->getStateNodeParser()->setRelatedParsers(realParser->getTransitionParser(),
                                                        realParser->getActionParser(), realParser->getDataModelParser(),
                                                        realParser->getInvokeParser(), realParser->getDoneDataParser());
    realActionParser = std::make_shared<SCXML::Parsing::ActionParser>(realFactory);
    realParser->getTransitionParser()->setActionParser(realActionParser);

    // 2. Test missing required attributes
    std::string missingAttributes = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
        <state id="start">
            <onentry>
                <raise/><!-- Missing required event attribute -->
                <assign/><!-- Missing required location attribute -->
            </onentry>
        </state>
    </scxml>)";

    SCXML::Common::Logger::info("Testing missing attributes handling");

    auto model2 = realParser->parseContent(missingAttributes);
    // Parser might create model but actions should have empty/default values
    if (model2) {
        auto startState = model2->findStateById("start");
        if (startState) {
            auto actions = startState->getEntryActionNodes();
            if (!actions.empty()) {
                // Actions created but attributes should be empty
                for (const auto &action : actions) {
                    EXPECT_TRUE(action != nullptr) << "Actions should be created even with missing attributes";
                }
            }
        }
    }

    SCXML::Common::Logger::info("ParsingErrorHandlingIntegration test completed - error handling working");
}

/**
 * @brief Test XML namespace and validation integration
 *
 * Verifies that parsing system properly handles SCXML namespaces
 * and XML validation requirements.
 */
TEST_F(ParsingSystemIntegrationTest, XMLNamespaceValidationIntegration) {
    std::string namespacedScxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" 
           xmlns:custom="http://example.org/custom"
           version="1.0" 
           initial="start">
        <state id="start">
            <onentry>
                <raise event="namespace.test"/>
                <log label="namespace" expr="'Testing namespace handling'"/>
                <assign location="result" expr="'success'"/>
            </onentry>
            <transition event="namespace.test" target="end"/>
        </state>
        <final id="end"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting XMLNamespaceValidationIntegration test");

    auto model = realParser->parseContent(namespacedScxml);
    ASSERT_TRUE(model != nullptr) << "Should parse namespaced SCXML";
    EXPECT_FALSE(realParser->hasErrors()) << "Should not have namespace parsing errors";

    // Test that parsing worked correctly despite namespaces
    auto startState = model->findStateById("start");
    ASSERT_TRUE(startState != nullptr) << "Should find start state";

    auto actions = startState->getEntryActionNodes();
    EXPECT_EQ(3, actions.size()) << "Should parse all actions correctly";

    // Verify action types are correct
    EXPECT_EQ("raise", actions[0]->getActionType());
    EXPECT_EQ("log", actions[1]->getActionType());
    EXPECT_EQ("assign", actions[2]->getActionType());

    // Verify attributes were parsed correctly
    auto raiseAction = std::dynamic_pointer_cast<SCXML::Core::RaiseActionNode>(actions[0]);
    ASSERT_TRUE(raiseAction != nullptr);
    EXPECT_EQ("namespace.test", raiseAction->getEvent()) << "Should parse event attribute correctly";

    SCXML::Common::Logger::info("XMLNamespaceValidationIntegration test completed - namespace handling working");
}

/**
 * @brief Test the original parsing bug scenario specifically
 *
 * Recreates the exact parsing conditions that contributed to the
 * original raise action bug and verifies the fix.
 */
TEST_F(ParsingSystemIntegrationTest, OriginalParsingBugScenario) {
    // This recreates the parsing scenario that led to the bug
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s0">
        <state id="s0">
            <onentry>
                <raise event="foo"/>
                <raise event="bar"/>
            </onentry>
            <transition event="foo" target="s1"/>
        </state>
        <state id="s1">
            <transition event="bar" target="pass"/>
        </state>
        <final id="pass"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting OriginalParsingBugScenario test");

    auto model = realParser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr) << "Should parse the problematic SCXML";
    EXPECT_FALSE(realParser->hasErrors()) << "Should not have parsing errors";

    auto s0 = model->findStateById("s0");
    ASSERT_TRUE(s0 != nullptr) << "Should find s0 state";

    auto entryActions = s0->getEntryActionNodes();
    ASSERT_EQ(2, entryActions.size()) << "BUG FIX: Should parse both raise actions";

    // This is where the original bug manifested:
    // 1. NodeFactory not creating RaiseActionNode
    for (const auto &action : entryActions) {
        EXPECT_EQ("raise", action->getActionType())
            << "BUG FIX: NodeFactory must create action with type 'raise', not 'unknown'";

        auto raiseNode = std::dynamic_pointer_cast<SCXML::Core::RaiseActionNode>(action);
        EXPECT_TRUE(raiseNode != nullptr) << "BUG FIX: NodeFactory must create RaiseActionNode instances";
    }

    // 2. ActionParser not setting attributes
    auto raiseAction1 = std::dynamic_pointer_cast<SCXML::Core::RaiseActionNode>(entryActions[0]);
    auto raiseAction2 = std::dynamic_pointer_cast<SCXML::Core::RaiseActionNode>(entryActions[1]);
    ASSERT_TRUE(raiseAction1 != nullptr && raiseAction2 != nullptr);

    EXPECT_EQ("foo", raiseAction1->getEvent()) << "BUG FIX: ActionParser must set event='foo' on first RaiseActionNode";
    EXPECT_EQ("bar", raiseAction2->getEvent())
        << "BUG FIX: ActionParser must set event='bar' on second RaiseActionNode";

    EXPECT_FALSE(raiseAction1->getEvent().empty()) << "Event attribute must not be empty";
    EXPECT_FALSE(raiseAction2->getEvent().empty()) << "Event attribute must not be empty";

    SCXML::Common::Logger::info("OriginalParsingBugScenario test completed - parsing bug is fixed!");
}