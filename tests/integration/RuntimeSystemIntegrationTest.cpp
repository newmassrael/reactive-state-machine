#include "IntegrationTestCommon.h"
#include "generator/include/runtime/ActionProcessor.h"
#include "generator/include/runtime/Processor.h"
#include "generator/include/runtime/RuntimeContext.h"
#include "generator/include/runtime/StateMachineFactory.h"

#include "generator/include/common/Logger.h"
#include "generator/include/core/NodeFactory.h"
#include "generator/include/parsing/DocumentParser.h"
#include "generator/include/runtime/ECMAScriptDataModelEngine.h"
#include <chrono>
#include <thread>

/**
 * @brief Integration tests for the complete runtime system
 *
 * This test suite verifies the integration between all runtime components:
 * - Processor initialization and state management
 * - Event queue processing and ordering
 * - State transitions and lifecycle management
 * - Data model integration
 * - Error handling and recovery
 *
 * These tests ensure that runtime components work together correctly
 * and would catch integration issues between processor, state manager,
 * and event handling systems.
 */
class RuntimeSystemIntegrationTest : public IntegrationTestBase {
protected:
    void SetUp() override {
        IntegrationTestBase::SetUp();

        // Use real implementations
        realFactory = std::make_shared<SCXML::Core::NodeFactory>();
        realParser = std::make_shared<SCXML::Parsing::DocumentParser>(realFactory);

        // Setup parser relationships
        realParser->getStateNodeParser()->setRelatedParsers(
            realParser->getTransitionParser(), realParser->getActionParser(), realParser->getDataModelParser(),
            realParser->getInvokeParser(), realParser->getDoneDataParser());

        auto actionParser = std::make_shared<SCXML::Parsing::ActionParser>(realFactory);
        realParser->getTransitionParser()->setActionParser(actionParser);
    }

    /**
     * @brief Create processor with real runtime components
     */
    std::shared_ptr<SCXML::Processor> createProcessorWithRealComponents(const std::string &scxmlContent) {
        auto model = realParser->parseContent(scxmlContent);
        if (!model || realParser->hasErrors()) {
            return nullptr;
        }

        auto options = SCXML::Runtime::StateMachineFactory::getDefaultOptions();
        options.name = "RuntimeIntegrationTest";
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
     * @brief Process all events until stable state or timeout
     */
    bool processUntilStable(std::shared_ptr<SCXML::Processor> processor,
                            std::chrono::milliseconds timeout = std::chrono::milliseconds(3000)) {
        // Use the safe waitForStable method instead of direct processNextEvent calls
        return processor->waitForStable(timeout.count());
    }

protected:
    std::shared_ptr<SCXML::Core::NodeFactory> realFactory;
    std::shared_ptr<SCXML::Parsing::DocumentParser> realParser;
};

/**
 * @brief Test processor initialization and state management integration
 *
 * Verifies that the processor properly initializes with the state manager
 * and correctly activates initial states.
 */
TEST_F(RuntimeSystemIntegrationTest, ProcessorStateManagerIntegration) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
        <state id="s1" initial="s1a">
            <state id="s1a">
                <transition event="go" target="s1b"/>
            </state>
            <state id="s1b">
                <transition event="next" target="s2"/>
            </state>
        </state>
        <state id="s2">
            <transition event="finish" target="end"/>
        </state>
        <final id="end"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting ProcessorStateManagerIntegration test");

    auto processor = createProcessorWithRealComponents(scxml);
    ASSERT_TRUE(processor != nullptr);

    // Test initial state before starting
    EXPECT_FALSE(processor->isRunning()) << "Processor should not be running before start";

    // Test processor initialization
    EXPECT_TRUE(processor->start()) << "Processor should start successfully";
    EXPECT_TRUE(processor->isRunning()) << "Processor should be running after start";

    // Test initial state configuration
    auto activeStates = processor->getActiveStates();
    EXPECT_FALSE(activeStates.empty()) << "Should have active states after initialization";

    // Debug: Print all active states
    SCXML::Common::Logger::info("Active states after start:");
    for (const auto &state : activeStates) {
        SCXML::Common::Logger::info("  - " + state);
    }

    // Should be in initial state s1a
    bool foundS1a = false;
    for (const auto &state : activeStates) {
        if (state == "s1a" || state.find("s1a") != std::string::npos) {
            foundS1a = true;
            break;
        }
    }
    EXPECT_TRUE(foundS1a) << "Should be in initial state s1a";

    // Test state transitions through event processing
    processor->sendEvent("go", "");
    EXPECT_TRUE(processUntilStable(processor));

    activeStates = processor->getActiveStates();
    SCXML::Common::Logger::info("Active states after 'go' event:");
    for (const auto &state : activeStates) {
        SCXML::Common::Logger::info("  - " + state);
    }
    bool foundS1b = false;
    for (const auto &state : activeStates) {
        if (state == "s1b" || state.find("s1b") != std::string::npos) {
            foundS1b = true;
            break;
        }
    }
    EXPECT_TRUE(foundS1b) << "Should transition to s1b after 'go' event";

    // Test transition to different parent state
    processor->sendEvent("next", "");
    EXPECT_TRUE(processUntilStable(processor));

    activeStates = processor->getActiveStates();
    SCXML::Common::Logger::info("Active states after 'next' event:");
    for (const auto &state : activeStates) {
        SCXML::Common::Logger::info("  - " + state);
    }
    bool foundS2 = false;
    for (const auto &state : activeStates) {
        if (state == "s2" || state.find("s2") != std::string::npos) {
            foundS2 = true;
            break;
        }
    }
    EXPECT_TRUE(foundS2) << "Should transition to s2 after 'next' event";

    // Test transition to final state
    processor->sendEvent("finish", "");
    EXPECT_TRUE(processUntilStable(processor));

    EXPECT_TRUE(processor->isInFinalState()) << "Should be in final state";
    EXPECT_FALSE(processor->isRunning()) << "Should not be running in final state";

    SCXML::Common::Logger::info("ProcessorStateManagerIntegration test completed successfully");
}

/**
 * @brief Test event queue processing and ordering integration
 *
 * Verifies that the runtime system correctly processes events in order
 * and maintains proper event queue behavior.
 */
TEST_F(RuntimeSystemIntegrationTest, EventQueueProcessingIntegration) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="collector" datamodel="ecmascript">
        <datamodel>
            <data id="events" expr="''"/>
        </datamodel>
        <state id="collector">
            <transition event="e1" target="collector">
                <assign location="events" expr="events + 'e1,'"/>
            </transition>
            <transition event="e2" target="collector">
                <assign location="events" expr="events + 'e2,'"/>
            </transition>
            <transition event="e3" target="collector">
                <assign location="events" expr="events + 'e3,'"/>
            </transition>
            <transition event="check" cond="events === 'e1,e2,e3,'" target="pass"/>
            <transition event="check" target="fail"/>
        </state>
        <final id="pass"/>
        <final id="fail"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting EventQueueProcessingIntegration test");

    auto processor = createProcessorWithRealComponents(scxml);
    ASSERT_TRUE(processor != nullptr);
    EXPECT_TRUE(processor->start());

    // Test event queue is initially empty
    EXPECT_TRUE(processor->isEventQueueEmpty()) << "Event queue should be empty initially";

    // Send multiple events in sequence
    processor->sendEvent("e1", "");
    processor->sendEvent("e2", "");
    processor->sendEvent("e3", "");
    processor->sendEvent("check", "");

    // Note: Events are processed synchronously, so queue may be empty after sending

    // Process all events and verify order
    EXPECT_TRUE(processUntilStable(processor));

    // Should reach pass state if events were processed in correct order
    EXPECT_TRUE(processor->isInFinalState()) << "Should reach final state after processing all events";

    auto finalStates = processor->getActiveStates();
    bool foundPass = false;
    for (const auto &state : finalStates) {
        if (state.find("pass") != std::string::npos) {
            foundPass = true;
            break;
        }
    }
    EXPECT_TRUE(foundPass) << "Should reach 'pass' state, indicating correct event order";

    SCXML::Common::Logger::info("EventQueueProcessingIntegration test completed successfully");
}

/**
 * @brief Test internal event generation and processing integration
 *
 * Verifies that internally generated events (raise actions) are properly
 * integrated with the event queue and processed in the correct order.
 */
TEST_F(RuntimeSystemIntegrationTest, InternalEventGenerationIntegration) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start" datamodel="ecmascript">
        <datamodel>
            <data id="sequence" expr="''"/>
        </datamodel>
        <state id="start">
            <onentry>
                <assign location="sequence" expr="sequence + 'start,'"/>
                <raise event="internal1"/>
                <raise event="internal2"/>
            </onentry>
            <transition event="internal1" target="middle"/>
        </state>
        <state id="middle">
            <onentry>
                <assign location="sequence" expr="sequence + 'middle,'"/>
            </onentry>
            <transition event="internal2" target="end"/>
        </state>
        <state id="end">
            <onentry>
                <assign location="sequence" expr="sequence + 'end,'"/>
            </onentry>
            <transition event="external" cond="sequence === 'start,middle,end,'" target="pass"/>
            <transition event="external" target="fail"/>
        </state>
        <final id="pass"/>
        <final id="fail"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting InternalEventGenerationIntegration test");

    auto processor = createProcessorWithRealComponents(scxml);
    ASSERT_TRUE(processor != nullptr);
    EXPECT_TRUE(processor->start());

    // Process internal events generated during startup
    EXPECT_TRUE(processUntilStable(processor));

    // Should be in 'end' state after processing internal events
    auto activeStates = processor->getActiveStates();
    bool foundEnd = false;
    for (const auto &state : activeStates) {
        if (state == "end" || state.find("end") != std::string::npos) {
            foundEnd = true;
            break;
        }
    }
    EXPECT_TRUE(foundEnd) << "Should be in 'end' state after internal event processing";

    // Send external event to verify sequence
    processor->sendEvent("external", "");
    EXPECT_TRUE(processUntilStable(processor));

    EXPECT_TRUE(processor->isInFinalState()) << "Should reach final state";

    auto finalStates = processor->getActiveStates();
    bool foundPass = false;
    for (const auto &state : finalStates) {
        if (state.find("pass") != std::string::npos) {
            foundPass = true;
            break;
        }
    }
    EXPECT_TRUE(foundPass) << "Should reach 'pass' state, indicating correct internal event processing";

    SCXML::Common::Logger::info("InternalEventGenerationIntegration test completed successfully");
}

/**
 * @brief Test data model integration with runtime system
 *
 * Verifies that data model changes are properly integrated with
 * state transitions and condition evaluation.
 */
TEST_F(RuntimeSystemIntegrationTest, DataModelRuntimeIntegration) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start" datamodel="ecmascript">
        <datamodel>
            <data id="counter" expr="0"/>
            <data id="threshold" expr="5"/>
            <data id="result" expr="'unknown'"/>
        </datamodel>
        <state id="start">
            <onentry>
                <assign location="counter" expr="counter + 1"/>
            </onentry>
            <transition cond="counter &lt; threshold" target="increment"/>
            <transition cond="counter >= threshold" target="finish"/>
        </state>
        <state id="increment">
            <onentry>
                <assign location="counter" expr="counter + 2"/>
            </onentry>
            <transition target="start"/>
        </state>
        <state id="finish">
            <onentry>
                <assign location="result" expr="counter >= threshold ? 'success' : 'failure'"/>
            </onentry>
            <transition cond="result === 'success'" target="pass"/>
            <transition target="fail"/>
        </state>
        <final id="pass"/>
        <final id="fail"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting DataModelRuntimeIntegration test");

    auto processor = createProcessorWithRealComponents(scxml);
    ASSERT_TRUE(processor != nullptr);
    EXPECT_TRUE(processor->start());

    // Process until stable (should loop through states based on data model)
    EXPECT_TRUE(processUntilStable(processor));

    // Should reach final state based on data model calculations
    EXPECT_TRUE(processor->isInFinalState()) << "Should reach final state based on data model conditions";

    auto finalStates = processor->getActiveStates();
    bool foundPass = false;
    for (const auto &state : finalStates) {
        if (state.find("pass") != std::string::npos) {
            foundPass = true;
            break;
        }
    }
    EXPECT_TRUE(foundPass) << "Should reach 'pass' state based on data model logic";

    SCXML::Common::Logger::info("DataModelRuntimeIntegration test completed successfully");
}

/**
 * @brief Test runtime error handling and recovery integration
 *
 * Verifies that the runtime system properly handles errors
 * in various components and maintains system stability.
 */
TEST_F(RuntimeSystemIntegrationTest, RuntimeErrorHandlingIntegration) {
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start" datamodel="ecmascript">
        <datamodel>
            <data id="counter" expr="0"/>
        </datamodel>
        <state id="start">
            <onentry>
                <assign location="counter" expr="counter + 1"/>
                <!-- This will cause an error but should not crash the system -->
                <assign location="nonexistent" expr="this.will.cause.error"/>
                <assign location="counter" expr="counter + 1"/>
            </onentry>
            <transition cond="counter >= 2" target="recovery"/>
            <transition target="fail"/>
        </state>
        <state id="recovery">
            <onentry>
                <!-- Test recovery after error -->
                <assign location="counter" expr="counter + 10"/>
            </onentry>
            <transition cond="counter >= 12" target="pass"/>
            <transition target="fail"/>
        </state>
        <final id="pass"/>
        <final id="fail"/>
    </scxml>)";

    SCXML::Common::Logger::info("Starting RuntimeErrorHandlingIntegration test");

    auto processor = createProcessorWithRealComponents(scxml);
    ASSERT_TRUE(processor != nullptr);

    // Should start successfully even with potentially problematic SCXML
    EXPECT_TRUE(processor->start()) << "Runtime should start despite potential errors in SCXML";

    // Process events (should handle errors gracefully)
    EXPECT_TRUE(processUntilStable(processor)) << "Runtime should process to stable state despite errors";

    // Note: Processor may stop after reaching final state, which is correct behavior

    // Should still reach a final state
    EXPECT_TRUE(processor->isInFinalState()) << "Runtime should reach final state despite errors";

    auto finalStates = processor->getActiveStates();
    bool foundPass = false;
    for (const auto &state : finalStates) {
        if (state.find("pass") != std::string::npos) {
            foundPass = true;
            break;
        }
    }
    EXPECT_TRUE(foundPass) << "Runtime should recover from errors and reach success state";

    SCXML::Common::Logger::info("RuntimeErrorHandlingIntegration test completed successfully");
}

/**
 * @brief Test parallel state runtime integration
 *
 * Verifies that parallel states are properly handled by the runtime
 * system with correct state management and event processing.
 */
TEST_F(RuntimeSystemIntegrationTest, ParallelStateRuntimeIntegration) {
    std::string scxml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="parallel">
        <parallel id="parallel">
            <state id="region1" initial="r1s1">
                <state id="r1s1">
                    <transition event="r1next" target="r1s2"/>
                </state>
                <state id="r1s2">
                    <onentry>
                        <raise event="r1done"/>
                    </onentry>
                </state>
            </state>
            <state id="region2" initial="r2s1">
                <state id="r2s1">
                    <transition event="r2next" target="r2s2"/>
                </state>
                <state id="r2s2">
                    <onentry>
                        <raise event="r2done"/>
                    </onentry>
                </state>
            </state>
        </parallel>
        <state id="end">
            <transition event="finish" target="final"/>
        </state>
        <final id="final"/>
        
        <!-- Transitions from parallel state when both regions are done -->
        <transition event="r1done" cond="In('r2s2')" target="end"/>
        <transition event="r2done" cond="In('r1s2')" target="end"/>
    </scxml>)xml";

    SCXML::Common::Logger::info("Starting ParallelStateRuntimeIntegration test");

    auto processor = createProcessorWithRealComponents(scxml);
    ASSERT_TRUE(processor != nullptr);
    EXPECT_TRUE(processor->start());

    // Should be in both parallel regions initially
    auto activeStates = processor->getActiveStates();
    bool foundR1S1 = false, foundR2S1 = false;
    for (const auto &state : activeStates) {
        if (state.find("r1s1") != std::string::npos) {
            foundR1S1 = true;
        }
        if (state.find("r2s1") != std::string::npos) {
            foundR2S1 = true;
        }
    }
    EXPECT_TRUE(foundR1S1 && foundR2S1) << "Should be in both parallel regions initially";

    // Advance region 1
    processor->sendEvent("r1next", "");
    EXPECT_TRUE(processUntilStable(processor));

    // Advance region 2
    processor->sendEvent("r2next", "");
    EXPECT_TRUE(processUntilStable(processor));

    // Should have transitioned out of parallel state to end state
    activeStates = processor->getActiveStates();
    bool foundEnd = false;
    for (const auto &state : activeStates) {
        if (state == "end" || state.find("end") != std::string::npos) {
            foundEnd = true;
            break;
        }
    }
    EXPECT_TRUE(foundEnd) << "Should transition to end state when both parallel regions complete";

    // Complete the state machine
    processor->sendEvent("finish", "");
    EXPECT_TRUE(processUntilStable(processor));

    EXPECT_TRUE(processor->isInFinalState()) << "Should reach final state";

    SCXML::Common::Logger::info("ParallelStateRuntimeIntegration test completed successfully");
}

/**
 * @brief Test complex runtime scenario integration
 *
 * A comprehensive test that exercises multiple runtime system components
 * together in a complex scenario.
 */
TEST_F(RuntimeSystemIntegrationTest, ComplexRuntimeScenarioIntegration) {
    std::string scxml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="init" datamodel="ecmascript">
        <datamodel>
            <data id="step" expr="0"/>
            <data id="data1" expr="'initial'"/>
            <data id="data2" expr="100"/>
        </datamodel>
        <state id="init">
            <onentry>
                <assign location="step" expr="1"/>
                <log label="init" expr="'Initializing with step: ' + step"/>
                <raise event="start"/>
            </onentry>
            <transition event="start" target="processing"/>
        </state>
        <state id="processing">
            <onentry>
                <assign location="step" expr="step + 1"/>
                <assign location="data1" expr="'processing'"/>
                <log label="processing" expr="'Processing step: ' + step"/>
            </onentry>
            <transition cond="step >= 2" target="parallel"/>
        </state>
        <parallel id="parallel">
            <state id="worker1" initial="w1s1">
                <state id="w1s1">
                    <onentry>
                        <assign location="data2" expr="data2 + 50"/>
                        <log label="w1" expr="'Worker1 processing: ' + data2"/>
                        <raise event="w1done"/>
                    </onentry>
                </state>
            </state>
            <state id="worker2" initial="w2s1">
                <state id="w2s1">
                    <onentry>
                        <assign location="data1" expr="'worker2'"/>
                        <log label="w2" expr="'Worker2 processing: ' + data1"/>
                        <raise event="w2done"/>
                    </onentry>
                </state>
            </state>
        </parallel>
        <state id="finalizing">
            <onentry>
                <log label="final" expr="'Finalizing with data1=' + data1 + ', data2=' + data2"/>
                <raise event="complete"/>
            </onentry>
            <transition event="complete" target="finished"/>
        </state>
        <final id="finished"/>
        
        <!-- Transition from parallel when both workers are done -->
        <transition event="w1done" cond="In('w2s1')" target="finalizing"/>
        <transition event="w2done" cond="In('w1s1')" target="finalizing"/>
    </scxml>)xml";

    SCXML::Common::Logger::info("Starting ComplexRuntimeScenarioIntegration test");

    auto processor = createProcessorWithRealComponents(scxml);
    ASSERT_TRUE(processor != nullptr);
    EXPECT_TRUE(processor->start());

    // Process through the entire complex scenario
    EXPECT_TRUE(processUntilStable(processor)) << "Should process through complex scenario";

    // Should reach final state
    EXPECT_TRUE(processor->isInFinalState()) << "Should reach final state after complex processing";

    // Should reach the finished final state
    auto finalStates = processor->getActiveStates();
    bool foundFinished = false;
    for (const auto &state : finalStates) {
        if (state.find("finished") != std::string::npos) {
            foundFinished = true;
        }
    }

    EXPECT_TRUE(foundFinished) << "Should reach 'finished' state after complex runtime integration";

    SCXML::Common::Logger::info(
        "ComplexRuntimeScenarioIntegration test completed successfully - runtime integration working!");
}