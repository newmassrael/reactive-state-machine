#include "IntegrationTestCommon.h"
#include "generator/include/common/Logger.h"
#include "generator/include/core/NodeFactory.h"
#include "generator/include/core/actions/AssignActionNode.h"
#include "generator/include/core/actions/LogActionNode.h"
#include "generator/include/core/actions/RaiseActionNode.h"
#include "generator/include/parsing/ActionParser.h"
#include "generator/include/parsing/DocumentParser.h"
#include "generator/include/runtime/ActionProcessor.h"
#include "generator/include/runtime/StateMachineFactory.h"
#include "generator/include/runtime/executors/AssignActionExecutor.h"
#include "generator/include/runtime/executors/LogActionExecutor.h"
#include "generator/include/runtime/executors/RaiseActionExecutor.h"
#include <chrono>
#include <thread>

/**
 * @brief Integration tests for the complete action execution system
 *
 * This test suite verifies the entire action execution pipeline from
 * SCXML parsing to actual runtime execution, ensuring no component
 * is mocked and all integrations work correctly.
 *
 * This test would have caught the raise action execution bug where:
 * 1. ActionProcessor.getEntryActions() was empty
 * 2. NodeFactory wasn't creating RaiseActionNode instances
 * 3. ActionParser wasn't setting RaiseActionNode attributes
 */
class ActionSystemIntegrationTest : public IntegrationTestBase {
protected:
    void SetUp() override {
        IntegrationTestBase::SetUp();

        // Use real implementations, no mocks
        realFactory = std::make_shared<SCXML::Core::NodeFactory>();
        realParser = std::make_shared<SCXML::Parsing::DocumentParser>(realFactory);
        actionProcessor = std::make_shared<SCXML::Runtime::ActionProcessor>();

        // Setup related parsers with real implementations
        realParser->getStateNodeParser()->setRelatedParsers(
            realParser->getTransitionParser(), realParser->getActionParser(), realParser->getDataModelParser(),
            realParser->getInvokeParser(), realParser->getDoneDataParser());

        auto realActionParser = std::make_shared<SCXML::Parsing::ActionParser>(realFactory);
        realParser->getTransitionParser()->setActionParser(realActionParser);
    }

    /**
     * @brief Create a complete runtime processor from SCXML content
     */
    /**
     * @brief Create basic processor for simple tests without datamodel
     */
    std::shared_ptr<SCXML::Processor> createBasicProcessor(const std::string &scxmlContent) {
        auto model = realParser->parseContent(scxmlContent);
        if (!model || realParser->hasErrors()) {
            return nullptr;
        }

        auto options = SCXML::Runtime::StateMachineFactory::getDefaultOptions();
        options.name = "BasicIntegrationTest";
        options.enableLogging = true;
        options.enableEventTracing = true;
        options.validateModel = true;

        auto result = SCXML::Runtime::StateMachineFactory::create(model, options);
        if (!result.isValid()) {
            return nullptr;
        }

        return result.runtime;
    }

    /**
     * @brief Create processor with ECMAScript datamodel support (includes QuickJS)
     */
    std::shared_ptr<SCXML::Processor> createECMAScriptProcessor(const std::string &scxmlContent) {
        SCXML::Common::Logger::info("*** createECMAScriptProcessor CALLED ***");
        auto model = realParser->parseContent(scxmlContent);
        if (!model || realParser->hasErrors()) {
            return nullptr;
        }

        auto options = SCXML::Runtime::StateMachineFactory::getDefaultOptions();
        options.name = "ECMAScriptIntegrationTest";
        options.enableLogging = true;
        options.enableEventTracing = true;
        options.validateModel = true;

        // QuickJS engine should be automatically enabled for ECMAScript datamodel
        SCXML::Common::Logger::info("*** About to call StateMachineFactory::create ***");

        auto result = SCXML::Runtime::StateMachineFactory::create(model, options);
        if (!result.isValid()) {
            return nullptr;
        }

        return result.runtime;
    }

    /**
     * @brief Wait for processor to reach expected state or timeout
     */
    bool waitForState(std::shared_ptr<SCXML::Processor> processor, const std::string &expectedState,
                      std::chrono::milliseconds timeout = std::chrono::milliseconds(2000)) {
        auto startTime = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() - startTime < timeout) {
            // Use the safe waitForStable method instead of direct processNextEvent calls
            processor->waitForStable(100);  // Wait for processing to stabilize

            // Check if we reached final state after processing events
            if (processor->isInFinalState()) {
                auto finalStates = processor->getActiveStates();
                for (const auto &state : finalStates) {
                    if (state == expectedState || state.find(expectedState) != std::string::npos) {
                        return true;
                    }
                }
                return false;
            }

            // Also check current active states (not just final states)
            auto currentStates = processor->getActiveStates();
            for (const auto &state : currentStates) {
                if (state == expectedState || state.find(expectedState) != std::string::npos) {
                    return true;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        return false;
    }

protected:
    std::shared_ptr<SCXML::Core::NodeFactory> realFactory;
    std::shared_ptr<SCXML::Parsing::DocumentParser> realParser;
    std::shared_ptr<SCXML::Runtime::ActionProcessor> actionProcessor;
};

/**
 * @brief Test the complete raise action execution pipeline
 *
 * This is the critical test that would have detected the original bug.
 * It verifies:
 * 1. SCXML parsing creates correct document model
 * 2. NodeFactory creates RaiseActionNode (not generic ActionNode)
 * 3. ActionParser sets event attribute on RaiseActionNode
 * 4. ActionProcessor.getEntryActions() returns actual actions
 * 5. RaiseActionExecutor processes the action correctly
 * 6. Event is added to internal event queue
 * 7. State machine transitions based on raised event
 */
TEST_F(ActionSystemIntegrationTest, RaiseActionFullPipelineExecution) {
    // W3C Test 144 inspired - events should be processed in order
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s0">
        <state id="s0">
            <onentry>
                <raise event="foo"/>
                <raise event="bar"/>
            </onentry>
            <transition event="foo" target="s1"/>
            <transition event="*" target="fail"/>
        </state>
        <state id="s1">
            <transition event="bar" target="pass"/>
            <transition event="*" target="fail"/>
        </state>
        <final id="pass"/>
        <final id="fail"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting RaiseActionFullPipelineExecution test");

    // 1. Test parsing and model creation
    auto model = realParser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr) << "Failed to parse SCXML content";
    EXPECT_FALSE(realParser->hasErrors()) << "Parser should not have errors";

    // 2. Test ActionProcessor has entry actions
    EXPECT_TRUE(actionProcessor->hasEntryActions(model, "s0")) << "ActionProcessor should detect entry actions exist";

    // Since getEntryActions is protected, we'll test execution instead
    auto context = std::make_unique<SCXML::Runtime::RuntimeContext>();
    auto triggeringEvent = std::make_shared<SCXML::Events::Event>("start.event");
    auto result = actionProcessor->executeEntryActions(model, "s0", triggeringEvent, *context);
    EXPECT_TRUE(result.success) << "Entry actions should execute successfully";

    // 3. Verify events were raised (should be in event queue after execution)
    EXPECT_TRUE(context->getEventManager().hasInternalEvents())
        << "Raise actions should have generated events in the queue";

    // 4. Test complete runtime execution
    auto processor = createBasicProcessor(scxml);
    ASSERT_TRUE(processor != nullptr) << "Failed to create processor from model";

    // 5. Test state machine execution according to SCXML specification
    EXPECT_TRUE(processor->start()) << "Processor should start successfully";

    // Note: Processor may immediately reach final state during initialization
    // due to synchronous event processing, which is valid SCXML behavior

    // 6. According to SCXML spec, events are processed in macrosteps
    // Since events are raised but processor stays in s0, we need to trigger event processing
    std::chrono::milliseconds timeout(3000);
    auto startTime = std::chrono::steady_clock::now();
    bool reachedExpectedState = false;

    // Process events in SCXML-compliant manner: allow processor to run its event loop
    while (std::chrono::steady_clock::now() - startTime < timeout && processor->isRunning()) {
        // Check if we've reached a final state
        auto currentStates = processor->getActiveStates();
        for (const auto &state : currentStates) {
            if (state == "pass" || state == "fail") {
                reachedExpectedState = true;
                break;
            }
        }

        if (reachedExpectedState) {
            break;
        }

        // Allow the processor to continue and process events from its queue
        // The processor should automatically process the raised events in order
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // If no state change occurred, the processor may not be processing events automatically
    // In this case, we need to investigate why the SCXML event processing loop isn't working

    // 7. Verify SCXML-compliant event processing and state transitions
    auto finalStates = processor->getActiveStates();

    // Debug: Always print final states for diagnosis
    std::string stateInfo = "Final active states after SCXML execution: ";
    for (const auto &state : finalStates) {
        stateInfo += state + " ";
    }
    SCXML::Common::Logger::info(stateInfo);

    // According to SCXML W3C Test 144, events should be processed in order:
    // 1. "foo" event should trigger s0->s1 transition
    // 2. "bar" event should trigger s1->pass transition
    bool foundPassState = false;
    bool foundFailState = false;
    for (const auto &state : finalStates) {
        if (state == "pass") {
            foundPassState = true;
        } else if (state == "fail") {
            foundFailState = true;
        }
    }

    EXPECT_TRUE(foundPassState) << "Should reach 'pass' state via SCXML-compliant event ordering (foo->bar)";
    EXPECT_FALSE(foundFailState) << "Should NOT reach 'fail' state if events processed correctly";

    SCXML::Common::Logger::info("RaiseActionFullPipelineExecution test completed successfully");
}

/**
 * @brief Test assign action complete pipeline execution
 *
 * Verifies assign actions work through the complete pipeline
 * and data model changes are properly applied.
 */
TEST_F(ActionSystemIntegrationTest, AssignActionFullPipelineExecution) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start" datamodel="ecmascript">
        <datamodel>
            <data id="counter" expr="0"/>
        </datamodel>
        <state id="start">
            <onentry>
                <assign location="counter" expr="counter + 1"/>
                <assign location="counter" expr="counter + 5"/>
                <raise event="check"/>
            </onentry>
            <transition event="check" cond="counter == 6" target="pass"/>
            <transition event="check" target="fail"/>
        </state>
        <final id="pass"/>
        <final id="fail"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting AssignActionFullPipelineExecution test");

    // Test parsing
    auto model = realParser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(realParser->hasErrors());

    // Test ActionProcessor has entry actions
    EXPECT_TRUE(actionProcessor->hasEntryActions(model, "start")) << "Should have entry actions";

    // Test runtime execution with ECMAScript datamodel
    auto processor = createECMAScriptProcessor(scxml);
    ASSERT_TRUE(processor != nullptr);
    EXPECT_TRUE(processor->start());

    // Test execution reaches correct state based on data model changes
    bool reachedFinalState = waitForState(processor, "pass");
    EXPECT_TRUE(reachedFinalState) << "Should reach 'pass' state with counter = 6";

    SCXML::Common::Logger::info("AssignActionFullPipelineExecution test completed successfully");
}

/**
 * @brief Test log action complete pipeline execution
 *
 * Verifies log actions are properly created and executed.
 */
TEST_F(ActionSystemIntegrationTest, LogActionFullPipelineExecution) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
        <state id="start">
            <onentry>
                <log label="test" expr="'Log message from integration test'"/>
                <raise event="complete"/>
            </onentry>
            <transition event="complete" target="end"/>
        </state>
        <final id="end"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting LogActionFullPipelineExecution test");

    // Test parsing
    auto model = realParser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(realParser->hasErrors());

    // Test ActionProcessor has entry actions
    EXPECT_TRUE(actionProcessor->hasEntryActions(model, "start")) << "Should have entry actions";

    // Execute entry actions and verify success
    auto context = std::make_unique<SCXML::Runtime::RuntimeContext>();
    auto triggeringEvent = std::make_shared<SCXML::Events::Event>("test.event");
    auto result = actionProcessor->executeEntryActions(model, "start", triggeringEvent, *context);
    EXPECT_TRUE(result.success) << "Entry actions should execute successfully";

    // Note: Log actions write to output but don't change state, so we verify execution success

    // Test runtime execution
    auto processor = createBasicProcessor(scxml);
    ASSERT_TRUE(processor != nullptr);
    EXPECT_TRUE(processor->start());

    bool reachedFinalState = waitForState(processor, "end");
    EXPECT_TRUE(reachedFinalState) << "Should reach 'end' state after log action";

    SCXML::Common::Logger::info("LogActionFullPipelineExecution test completed successfully");
}

/**
 * @brief Test complex action sequence execution
 *
 * Tests multiple different action types in sequence to ensure
 * the action processing pipeline handles complex scenarios.
 */
TEST_F(ActionSystemIntegrationTest, ComplexActionSequenceExecution) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start" datamodel="ecmascript">
        <datamodel>
            <data id="step" expr="0"/>
            <data id="message" expr="'initial'"/>
        </datamodel>
        <state id="start">
            <onentry>
                <log label="entry" expr="'Entering start state'"/>
                <assign location="step" expr="1"/>
                <assign location="message" expr="'first step'"/>
                <log label="progress" expr="'Step: ' + step + ', Message: ' + message"/>
                <assign location="step" expr="step + 1"/>
                <raise event="next"/>
            </onentry>
            <transition event="next" cond="step == 2" target="middle"/>
            <transition event="next" target="error"/>
        </state>
        <state id="middle">
            <onentry>
                <assign location="message" expr="'second step'"/>
                <log label="middle" expr="'In middle state: ' + message"/>
                <raise event="finish"/>
            </onentry>
            <transition event="finish" target="end"/>
        </state>
        <final id="end"/>
        <final id="error"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting ComplexActionSequenceExecution test");

    // Test parsing
    auto model = realParser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(realParser->hasErrors());

    // Test ActionProcessor has entry actions for both states
    EXPECT_TRUE(actionProcessor->hasEntryActions(model, "start")) << "Start state should have entry actions";
    EXPECT_TRUE(actionProcessor->hasEntryActions(model, "middle")) << "Middle state should have entry actions";

    // Test complete runtime execution
    auto processor = createBasicProcessor(scxml);
    ASSERT_TRUE(processor != nullptr);
    EXPECT_TRUE(processor->start());

    // Should reach end state through complex action sequence
    bool reachedFinalState = waitForState(processor, "end");
    EXPECT_TRUE(reachedFinalState) << "Should reach 'end' state after complex action sequence";

    SCXML::Common::Logger::info("ComplexActionSequenceExecution test completed successfully");
}

/**
 * @brief Test action execution error handling
 *
 * Verifies that action execution errors are properly handled
 * and don't crash the runtime system.
 */
TEST_F(ActionSystemIntegrationTest, ActionExecutionErrorHandling) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start" datamodel="ecmascript">
        <datamodel>
            <data id="counter" expr="0"/>
        </datamodel>
        <state id="start">
            <onentry>
                <assign location="counter" expr="counter + 1"/>
                <!-- This should cause an error - assigning to non-existent location -->
                <assign location="nonexistent" expr="invalid expression"/>
                <assign location="counter" expr="counter + 1"/>
                <raise event="check"/>
            </onentry>
            <transition event="check" target="end"/>
        </state>
        <final id="end"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting ActionExecutionErrorHandling test");

    // Test parsing still works with potentially problematic actions
    auto model = realParser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(realParser->hasErrors());

    // Test ActionProcessor has entry actions
    EXPECT_TRUE(actionProcessor->hasEntryActions(model, "start")) << "Should have entry actions";

    // Execute entry actions and verify handling of potential errors
    auto context = std::make_unique<SCXML::Runtime::RuntimeContext>();
    auto triggeringEvent = std::make_shared<SCXML::Events::Event>("test.event");
    auto result = actionProcessor->executeEntryActions(model, "start", triggeringEvent, *context);
    // Note: Result may be false if actions have errors, which is expected for this test

    // Test runtime execution handles errors gracefully
    auto processor = createBasicProcessor(scxml);
    ASSERT_TRUE(processor != nullptr);
    EXPECT_TRUE(processor->start()) << "Processor should start even with potentially problematic actions";

    // Should still reach final state despite action errors
    bool reachedFinalState = waitForState(processor, "end");
    EXPECT_TRUE(reachedFinalState) << "Should reach 'end' state even with action execution errors";

    SCXML::Common::Logger::info("ActionExecutionErrorHandling test completed successfully");
}

/**
 * @brief Test raise action event ordering according to W3C SCXML specification
 *
 * Based on W3C Test 144: Events should be inserted into queue in order raised.
 * Tests SCXML-compliant event processing where raised events (foo, bar) are
 * processed in sequence according to W3C specification, not forced manually.
 */
TEST_F(ActionSystemIntegrationTest, RaiseActionEventOrderingIntegration) {
    // This scenario tests event ordering from W3C Test 144
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s0">
        <state id="s0">
            <onentry>
                <raise event="foo"/>
                <raise event="bar"/>
            </onentry>
            <transition event="foo" target="s1"/>
            <transition event="*" target="fail"/>
        </state>
        <state id="s1">
            <transition event="bar" target="pass"/>
            <transition event="*" target="fail"/>
        </state>
        <final id="pass"/>
        <final id="fail"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting RaiseActionEventOrderingIntegration test");

    // Test the specific bug conditions that were failing:

    // 1. Test that ActionProcessor has entry actions (was returning empty)
    auto model = realParser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_TRUE(actionProcessor->hasEntryActions(model, "s0")) << "ActionProcessor must detect entry actions exist";

    // 2. Test that actions can be executed (verifies NodeFactory created proper nodes)
    auto context = std::make_unique<SCXML::Runtime::RuntimeContext>();
    auto triggeringEvent = std::make_shared<SCXML::Events::Event>("start.event");
    auto result = actionProcessor->executeEntryActions(model, "s0", triggeringEvent, *context);
    EXPECT_TRUE(result.success) << "RaiseActionNode execution must succeed";

    // 3. Verify that raise events were actually generated
    EXPECT_TRUE(context->getEventManager().hasInternalEvents()) << "Raise actions must generate events in queue";

    // 3. Verify action parsing worked by testing complete execution pipeline

    // 4. Complete runtime execution must work
    auto processor = createBasicProcessor(scxml);
    ASSERT_TRUE(processor != nullptr);
    EXPECT_TRUE(processor->start());

    // 5. SCXML-compliant event processing: events must be processed in correct order (foo then bar)
    // Wait for the processor to complete its natural execution cycle according to SCXML spec

    // Give the processor time to complete natural event processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    bool reachedFinalState = processor->isInFinalState();
    EXPECT_TRUE(reachedFinalState) << "Must reach final state via SCXML-compliant event processing";

    // Verify SCXML-compliant final state according to W3C Test 144 specification
    auto finalStates = processor->getActiveStates();

    // Debug: Print final states for diagnosis
    std::string stateInfo = "Final states after SCXML-compliant processing: ";
    for (const auto &state : finalStates) {
        stateInfo += state + " ";
    }
    SCXML::Common::Logger::info(stateInfo);

    bool foundPass = false, foundFail = false;
    for (const auto &state : finalStates) {
        if (state == "pass") {
            foundPass = true;
        }
        if (state == "fail") {
            foundFail = true;
        }
    }

    EXPECT_TRUE(foundPass) << "W3C Test 144: Must reach 'pass' when events processed in correct order (foo->bar)";
    EXPECT_FALSE(foundFail) << "W3C Test 144: Must NOT reach 'fail' when SCXML event ordering is correct";

    SCXML::Common::Logger::info("RaiseActionEventOrderingIntegration test completed successfully!");
}