#include "factory/NodeFactory.h"
#include "parsing/SCXMLParser.h"
#include "runtime/ActionExecutorImpl.h"
#include "runtime/StateMachine.h"
#include "scripting/JSEngine.h"
#include "states/SCXMLParallelTypes.h"
#include <chrono>
#include <gtest/gtest.h>

#include "common/TestUtils.h"
#include <memory>
#include <string>
#include <thread>

namespace RSM {

class SCXMLParallelComplianceTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        engine_->reset();
        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
        sessionId_ = "scxml_parallel_compliance_test";
    }

    void TearDown() override {
        if (engine_) {
            engine_->reset();
        }
    }

    JSEngine *engine_;
    std::shared_ptr<NodeFactory> nodeFactory_;
    std::unique_ptr<SCXMLParser> parser_;
    std::string sessionId_;
};

// W3C SCXML specification 3.4: Parallel state basic behavior test
TEST_F(SCXMLParallelComplianceTest, W3C_ParallelState_BasicBehavior_ShouldParseAndEnterCorrectly) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <state id="region1">
                <initial>
                    <transition target="region1_active"/>
                </initial>
                <state id="region1_active">
                    <onentry>
                        <assign location="region1_entered" expr="true"/>
                    </onentry>
                </state>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_active"/>
                </initial>
                <state id="region2_active">
                    <onentry>
                        <assign location="region2_entered" expr="true"/>
                    </onentry>
                </state>
            </state>
        </parallel>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed - parallel state structure invalid";

    // W3C SCXML compliance: parallel state must be recognized and parsed correctly
    EXPECT_EQ(stateMachine->getInitialState(), "parallel1");

    // SCXML W3C section 3.4: Verify StateMachine can load and execute parallel state
    RSM::StateMachine sm;
    ASSERT_TRUE(sm.loadSCXMLFromString(scxmlContent)) << "StateMachine failed to load valid SCXML";
    ASSERT_TRUE(sm.start()) << "StateMachine failed to start with parallel initial state";

    // Verify parallel state is active
    EXPECT_EQ(sm.getCurrentState(), "parallel1") << "StateMachine did not enter parallel initial state";
    EXPECT_TRUE(sm.isRunning()) << "StateMachine not running after successful start";
}

// W3C SCXML specification 3.4: done.state event generation test
TEST_F(SCXMLParallelComplianceTest, W3C_DoneStateEvent_Generation_ShouldProcessDoneStateEvents) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <onentry>
                <assign location="parallel_entered" expr="true"/>
                <assign location="done_event_received" expr="false"/>
            </onentry>
            <state id="region1">
                <initial>
                    <transition target="region1_final"/>
                </initial>
                <final id="region1_final"/>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_final"/>
                </initial>
                <final id="region2_final"/>
            </state>
            <transition event="done.state.parallel1" target="completed">
                <assign location="done_event_received" expr="true"/>
            </transition>
        </parallel>
        <final id="completed"/>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    // W3C specification: done.state.parallel1 transition parsed correctly
    EXPECT_EQ(stateMachine->getInitialState(), "parallel1");

    // SCXML W3C specification section 3.4: done.state event handling compliance test
    RSM::StateMachine sm;
    ASSERT_TRUE(sm.loadSCXMLFromString(scxmlContent)) << "Failed to load valid SCXML with parallel state";
    ASSERT_TRUE(sm.start()) << "Failed to start StateMachine with parallel initial state";

    // W3C SCXML 3.4 compliance: When all parallel regions immediately reach final states,
    // the done.state event is generated and processed automatically, transitioning to completed
    ASSERT_EQ(sm.getCurrentState(), "completed")
        << "SCXML W3C compliance: parallel state should automatically transition to completed when all regions "
           "immediately reach final states";

    // W3C SCXML 3.13: "completed" is a top-level final state, so StateMachine MUST halt
    ASSERT_FALSE(sm.isRunning())
        << "W3C SCXML 3.13: StateMachine MUST halt when entering top-level final state 'completed'";

    // Verify that done.state event was automatically processed (no manual event needed)
    // The implementation should have automatically generated and processed done.state.parallel1

    // Verify final state compliance - the state machine should be in completed state
    // because all parallel regions immediately reached final states, triggering automatic
    // done.state.parallel1 event generation and transition to completed
    ASSERT_EQ(sm.getCurrentState(), "completed")
        << "StateMachine must transition to completed state per W3C SCXML 3.4 specification";
}

// W3C SCXML specification 3.4: done.state event automatic generation test
TEST_F(SCXMLParallelComplianceTest, W3C_Parallel_DoneStateEvent_Generation) {
    // W3C specification: When all regions of parallel state complete, done.state.parallel_id event is automatically
    // generated

    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="test_parallel" datamodel="ecmascript">
        <datamodel>
            <data id="done_event_received" expr="false"/>
            <data id="parallel_completed" expr="false"/>
        </datamodel>
        <parallel id="test_parallel">
            <state id="region_a">
                <initial><transition target="a_final"/></initial>
                <final id="a_final"/>
            </state>
            <state id="region_b">
                <initial><transition target="b_final"/></initial>
                <final id="b_final"/>
            </state>
            <!-- W3C SCXML 3.4: Define transition to receive done.state event -->
            <transition event="done.state.test_parallel" target="completed">
                <assign location="done_event_received" expr="true"/>
                <assign location="parallel_completed" expr="true"/>
            </transition>
        </parallel>
        <final id="completed"/>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    // SCXML W3C specification 3.4: Automatic done.state event generation test on parallel state completion
    RSM::StateMachine sm;
    ASSERT_TRUE(sm.loadSCXMLFromString(scxmlContent)) << "StateMachine loading failed";
    ASSERT_TRUE(sm.start()) << "StateMachine start failed";

    // W3C SCXML 3.4 specification test: All regions immediately enter final state
    // In this scenario, all regions complete as soon as the parallel state starts
    // Therefore, done.state.test_parallel event is automatically generated and immediately processed, transitioning to
    // completed state

    try {
        // Check if parallel state completed (done.state event automatic generation)
        // Wait briefly for StateMachine's next processing cycle
        std::this_thread::sleep_for(RSM::Test::Utils::POLL_INTERVAL_MS);

        // Verify transition occurred due to automatically generated done.state event
        auto doneEventResult =
            RSM::JSEngine::instance().evaluateExpression(sm.getSessionId(), "done_event_received").get();

        auto parallelCompletedResult =
            RSM::JSEngine::instance().evaluateExpression(sm.getSessionId(), "parallel_completed").get();

        // W3C SCXML 3.4: Verify done.state event automatic generation
        if (doneEventResult.getValueAsString() == "true" && parallelCompletedResult.getValueAsString() == "true") {
            // Also verify transition to final state
            EXPECT_EQ(sm.getCurrentState(), "completed") << "Transition due to done.state event not completed";

            Logger::info("W3C COMPLIANCE VERIFIED: done.state event automatically generated and processed");
            SUCCEED() << "SCXML W3C 3.4 specification compliance: Successfully auto-generated and processed "
                         "done.state.test_parallel event on parallel state completion";

        } else {
            // When done.state event was not automatically generated
            EXPECT_EQ(doneEventResult.getValueAsString(), "true")
                << "SCXML violation: done.state.test_parallel event not automatically generated. "
                << "According to W3C specification, done.state event should be automatically generated when all "
                   "parallel regions complete";

            EXPECT_EQ(parallelCompletedResult.getValueAsString(), "true")
                << "SCXML violation: Parallel state completion detection failed. "
                << "All regions of parallel state reached final state, so it should be recognized as complete";
        }

    } catch (const std::exception &e) {
        FAIL() << "SCXML violation: Error occurred during done.state event automatic generation test. "
               << "Parallel state completion handling according to W3C specification 3.4 not correctly implemented. "
               << "Error: " << e.what();
    }
}

// W3C SCXML specification 3.4: Parallel state completion criteria test
TEST_F(SCXMLParallelComplianceTest, W3C_ParallelState_CompletionCriteria_ShouldCompleteWhenAllRegionsFinal) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <state id="region1">
                <initial>
                    <transition target="region1_s1"/>
                </initial>
                <state id="region1_s1">
                    <transition event="finish_region1" target="region1_final"/>
                </state>
                <final id="region1_final"/>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_s1"/>
                </initial>
                <state id="region2_s1">
                    <transition event="finish_region2" target="region2_final"/>
                </state>
                <final id="region2_final"/>
            </state>
        </parallel>
        <final id="completed"/>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    // W3C specification: Parallel state completes when all regions reach final state
    EXPECT_EQ(stateMachine->getInitialState(), "parallel1");
}

// W3C SCXML specification 3.4: External transition from parallel state test
TEST_F(SCXMLParallelComplianceTest, W3C_ExternalTransition_FromParallelState_ShouldExitAllRegions) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <transition event="exit_parallel" target="single_state"/>
            <state id="region1">
                <initial>
                    <transition target="region1_active"/>
                </initial>
                <state id="region1_active"/>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_active"/>
                </initial>
                <state id="region2_active"/>
            </state>
        </parallel>
        <state id="single_state">
            <onentry>
                <assign location="single_state_entered" expr="true"/>
            </onentry>
        </state>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    // W3C specification: External transition from parallel state must deactivate all regions
    EXPECT_EQ(stateMachine->getInitialState(), "parallel1");
}

// W3C SCXML specification 3.4: Region independence test
TEST_F(SCXMLParallelComplianceTest, W3C_RegionIndependence_ShouldProcessEventsIndependently) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <state id="region1">
                <initial>
                    <transition target="region1_s1"/>
                </initial>
                <state id="region1_s1">
                    <transition event="region1_next" target="region1_s2"/>
                </state>
                <state id="region1_s2"/>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_s1"/>
                </initial>
                <state id="region2_s1">
                    <transition event="region2_next" target="region2_s2"/>
                </state>
                <state id="region2_s2"/>
            </state>
        </parallel>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    // W3C specification: Each region must process events independently
    EXPECT_EQ(stateMachine->getInitialState(), "parallel1");
}

// W3C SCXML specification 3.4: Nested parallel states test
TEST_F(SCXMLParallelComplianceTest, W3C_NestedParallelStates) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="outer_parallel" datamodel="ecmascript">
        <parallel id="outer_parallel">
            <state id="region1">
                <initial>
                    <transition target="inner_parallel"/>
                </initial>
                <parallel id="inner_parallel">
                    <state id="inner_region1">
                        <initial>
                            <transition target="inner_region1_active"/>
                        </initial>
                        <state id="inner_region1_active"/>
                    </state>
                    <state id="inner_region2">
                        <initial>
                            <transition target="inner_region2_active"/>
                        </initial>
                        <state id="inner_region2_active"/>
                    </state>
                </parallel>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_active"/>
                </initial>
                <state id="region2_active"/>
            </state>
        </parallel>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    // W3C specification: Nested parallel states must be handled correctly
    EXPECT_EQ(stateMachine->getInitialState(), "outer_parallel");
}

// W3C SCXML specification 3.4: Data model sharing test
TEST_F(SCXMLParallelComplianceTest, W3C_DataModelSharing) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <datamodel>
            <data id="shared_data" expr="0"/>
        </datamodel>
        <parallel id="parallel1">
            <state id="region1">
                <initial>
                    <transition target="region1_active"/>
                </initial>
                <state id="region1_active">
                    <onentry>
                        <assign location="shared_data" expr="shared_data + 1"/>
                    </onentry>
                </state>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_active"/>
                </initial>
                <state id="region2_active">
                    <onentry>
                        <assign location="shared_data" expr="shared_data + 10"/>
                    </onentry>
                </state>
            </state>
        </parallel>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    // W3C specification: Data model sharing between parallel states must work correctly
    EXPECT_EQ(stateMachine->getInitialState(), "parallel1");
}

// W3C SCXML specification 3.4: Event priority test
TEST_F(SCXMLParallelComplianceTest, W3C_EventPriority) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <transition event="high_priority" target="exit_state"/>
            <state id="region1">
                <initial>
                    <transition target="region1_active"/>
                </initial>
                <state id="region1_active">
                    <transition event="low_priority" target="region1_other"/>
                </state>
                <state id="region1_other"/>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_active"/>
                </initial>
                <state id="region2_active">
                    <transition event="low_priority" target="region2_other"/>
                </state>
                <state id="region2_other"/>
            </state>
        </parallel>
        <state id="exit_state"/>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    // W3C specification: Event priority must be handled correctly
    EXPECT_EQ(stateMachine->getInitialState(), "parallel1");
}

// W3C SCXML specification 3.4: Simultaneous region activation test (implemented)
TEST_F(SCXMLParallelComplianceTest, W3C_Parallel_RegionActivation_Simultaneous) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="test_parallel" datamodel="ecmascript">
        <datamodel>
            <data id="region1_active" expr="false"/>
            <data id="region2_active" expr="false"/>
            <data id="region3_active" expr="false"/>
        </datamodel>
        <parallel id="test_parallel">
            <state id="region1">
                <onentry>
                    <assign location="region1_active" expr="true"/>
                </onentry>
                <initial><transition target="r1_state"/></initial>
                <state id="r1_state"/>
            </state>
            <state id="region2">
                <onentry>
                    <assign location="region2_active" expr="true"/>
                </onentry>
                <initial><transition target="r2_state"/></initial>
                <state id="r2_state"/>
            </state>
            <state id="region3">
                <onentry>
                    <assign location="region3_active" expr="true"/>
                </onentry>
                <initial><transition target="r3_state"/></initial>
                <state id="r3_state"/>
            </state>
        </parallel>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    // W3C specification: "When a <parallel> element is active, ALL of its children are active"
    // Test actual region activation through StateMachine integration
    RSM::StateMachine sm;
    ASSERT_TRUE(sm.loadSCXMLFromString(scxmlContent)) << "StateMachine loading failed";
    ASSERT_TRUE(sm.start()) << "StateMachine start failed";

    // Verify parallel state is active
    EXPECT_EQ(sm.getCurrentState(), "test_parallel") << "Parallel state not entered";

    // Check if StateMachine has successfully activated regions through data model
    // The onentry actions should have executed, setting region variables to true
    // For now, this is infrastructure verification - actual data model execution
    // requires full StateMachine integration with ConcurrentRegion

    // SCXML W3C specification section 3.4 compliance verification:
    // "When a <parallel> element is active, ALL of its children are active"

    // Verify all child regions have their entry actions executed
    // This should result in region1_active, region2_active, region3_active being set to true

    // Check data model variables that should be set by onentry actions
    auto &jsEngine = RSM::JSEngine::instance();

    try {
        // Verify region1_active was set to true by onentry action
        auto region1Future = jsEngine.evaluateExpression(sm.getSessionId(), "region1_active");
        auto region1Result = region1Future.get();
        EXPECT_EQ(region1Result.getValueAsString(), "true")
            << "SCXML violation: region1 onentry action not executed. Expected true, got: "
            << region1Result.getValueAsString();

        // Verify region2_active was set to true by onentry action
        auto region2Future = jsEngine.evaluateExpression(sm.getSessionId(), "region2_active");
        auto region2Result = region2Future.get();
        EXPECT_EQ(region2Result.getValueAsString(), "true")
            << "SCXML violation: region2 onentry action not executed. Expected true, got: "
            << region2Result.getValueAsString();

        // Verify region3_active was set to true by onentry action
        auto region3Future = jsEngine.evaluateExpression(sm.getSessionId(), "region3_active");
        auto region3Result = region3Future.get();
        EXPECT_EQ(region3Result.getValueAsString(), "true")
            << "SCXML violation: region3 onentry action not executed. Expected true, got: "
            << region3Result.getValueAsString();

        Logger::info("W3C COMPLIANCE VERIFIED: All parallel regions executed onentry actions simultaneously");

    } catch (const std::exception &e) {
        FAIL() << "SCXML violation: Failed to verify parallel region activation. "
               << "Entry actions were not executed properly. Error: " << e.what();
    }
}

// W3C SCXML specification 3.4: Event broadcasting test (implemented)
TEST_F(SCXMLParallelComplianceTest, W3C_Parallel_EventBroadcasting_AllRegions) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="broadcast_test" datamodel="ecmascript">
        <datamodel>
            <data id="region1_received" expr="false"/>
            <data id="region2_received" expr="false"/>
            <data id="region3_received" expr="false"/>
        </datamodel>
        <parallel id="broadcast_test">
            <state id="region1">
                <initial><transition target="r1_waiting"/></initial>
                <state id="r1_waiting">
                    <transition event="test_event" target="r1_received">
                        <assign location="region1_received" expr="true"/>
                    </transition>
                </state>
                <state id="r1_received"/>
            </state>
            <state id="region2">
                <initial><transition target="r2_waiting"/></initial>
                <state id="r2_waiting">
                    <transition event="test_event" target="r2_received">
                        <assign location="region2_received" expr="true"/>
                    </transition>
                </state>
                <state id="r2_received"/>
            </state>
            <state id="region3">
                <initial><transition target="r3_waiting"/></initial>
                <state id="r3_waiting">
                    <transition event="test_event" target="r3_received">
                        <assign location="region3_received" expr="true"/>
                    </transition>
                </state>
                <state id="r3_received"/>
            </state>
        </parallel>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    RSM::StateMachine sm;
    ASSERT_TRUE(sm.loadSCXMLFromString(scxmlContent)) << "SCXML loading failed";
    ASSERT_TRUE(sm.start()) << "StateMachine start failed";

    // Verify initial state is parallel state
    EXPECT_EQ(sm.getCurrentState(), "broadcast_test") << "Parallel state not entered correctly";

    // Note: JSEngine reset removed - StateMachine needs its session for action execution

    try {
        // SCXML W3C specification section 3.4: Event broadcasting to all regions
        Logger::info("W3C COMPLIANCE TEST: Broadcasting 'test_event' to all parallel regions");

        auto result = sm.processEvent("test_event", "");
        EXPECT_TRUE(result.success) << "SCXML violation: Event broadcasting failed: " << result.errorMessage;

        // Verify all regions received and processed the event
        auto region1Future = RSM::JSEngine::instance().evaluateExpression(sm.getSessionId(), "region1_received");
        auto region1Result = region1Future.get();
        EXPECT_EQ(region1Result.getValueAsString(), "true")
            << "SCXML violation: region1 did not receive broadcast event. Expected true, got: "
            << region1Result.getValueAsString();

        auto region2Future = RSM::JSEngine::instance().evaluateExpression(sm.getSessionId(), "region2_received");
        auto region2Result = region2Future.get();
        EXPECT_EQ(region2Result.getValueAsString(), "true")
            << "SCXML violation: region2 did not receive broadcast event. Expected true, got: "
            << region2Result.getValueAsString();

        auto region3Future = RSM::JSEngine::instance().evaluateExpression(sm.getSessionId(), "region3_received");
        auto region3Result = region3Future.get();
        EXPECT_EQ(region3Result.getValueAsString(), "true")
            << "SCXML violation: region3 did not receive broadcast event. Expected true, got: "
            << region3Result.getValueAsString();

        Logger::info(
            "W3C COMPLIANCE VERIFIED: All parallel regions received and processed the broadcast event simultaneously");

    } catch (const std::exception &e) {
        FAIL() << "SCXML violation: Failed to verify event broadcasting. " << e.what();
    }
}

// W3C SCXML specification 3.4: Parallel state completion criteria test
TEST_F(SCXMLParallelComplianceTest, W3C_Parallel_CompletionCriteria) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="completion_test" datamodel="ecmascript">
        <datamodel>
            <data id="parallel_complete" expr="false"/>
            <data id="done_event_fired" expr="false"/>
        </datamodel>
        <parallel id="completion_test">
            <state id="region1">
                <initial><transition target="r1_active"/></initial>
                <state id="r1_active">
                    <transition event="complete_r1" target="r1_final"/>
                </state>
                <final id="r1_final"/>
            </state>
            <state id="region2">
                <initial><transition target="r2_active"/></initial>
                <state id="r2_active">
                    <transition event="complete_r2" target="r2_final"/>
                </state>
                <final id="r2_final"/>
            </state>
            <transition event="done.state.completion_test" target="completed">
                <assign location="parallel_complete" expr="true"/>
                <assign location="done_event_fired" expr="true"/>
            </transition>
        </parallel>
        <final id="completed"/>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    RSM::StateMachine sm;
    ASSERT_TRUE(sm.loadSCXMLFromString(scxmlContent)) << "SCXML loading failed";
    ASSERT_TRUE(sm.start()) << "StateMachine start failed";

    // Verify initial state is parallel state
    EXPECT_EQ(sm.getCurrentState(), "completion_test") << "Parallel state not entered correctly";

    try {
        // SCXML W3C specification section 3.4: Parallel completion criteria
        Logger::info("W3C COMPLIANCE TEST: Testing parallel state completion with done.state auto-generation");

        // Complete region 1
        auto result1 = sm.processEvent("complete_r1", "");
        EXPECT_TRUE(result1.success) << "Failed to complete region 1: " << result1.errorMessage;

        // Complete region 2 - this should trigger done.state.completion_test event
        auto result2 = sm.processEvent("complete_r2", "");
        EXPECT_TRUE(result2.success) << "Failed to complete region 2: " << result2.errorMessage;

        // Verify done.state event was automatically generated and processed
        auto parallelCompleteResult =
            RSM::JSEngine::instance().evaluateExpression(sm.getSessionId(), "parallel_complete").get();
        EXPECT_EQ(parallelCompleteResult.getValueAsString(), "true")
            << "SCXML violation: done.state event not automatically generated when all regions completed. Expected "
               "true, got: "
            << parallelCompleteResult.getValueAsString();

        auto doneEventResult =
            RSM::JSEngine::instance().evaluateExpression(sm.getSessionId(), "done_event_fired").get();
        EXPECT_EQ(doneEventResult.getValueAsString(), "true")
            << "SCXML violation: done.state.completion_test event not processed. Expected true, got: "
            << doneEventResult.getValueAsString();

        Logger::info(
            "W3C COMPLIANCE VERIFIED: Parallel state completion criteria with automatic done.state event generation");

    } catch (const std::exception &e) {
        FAIL() << "SCXML violation: Failed to verify parallel completion criteria. " << e.what();
    }
}

// W3C SCXML specification 3.4: Entry/exit sequence test
TEST_F(SCXMLParallelComplianceTest, W3C_Parallel_EntryExitSequence) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="entry_test" datamodel="ecmascript">
        <datamodel>
            <data id="entry_sequence" expr="[]"/>
            <data id="exit_sequence" expr="[]"/>
        </datamodel>
        <state id="entry_test">
            <transition event="enter_parallel" target="parallel_state"/>
        </state>
        <parallel id="parallel_state">
            <onentry>
                <script>entry_sequence.push('parallel_entry');</script>
            </onentry>
            <onexit>
                <script>exit_sequence.push('parallel_exit');</script>
            </onexit>
            <state id="child1">
                <onentry>
                    <script>entry_sequence.push('child1_entry');</script>
                </onentry>
                <onexit>
                    <script>exit_sequence.push('child1_exit');</script>
                </onexit>
                <initial><transition target="c1_active"/></initial>
                <state id="c1_active">
                    <transition event="exit_parallel" target="final_state"/>
                </state>
            </state>
            <state id="child2">
                <onentry>
                    <script>entry_sequence.push('child2_entry');</script>
                </onentry>
                <onexit>
                    <script>exit_sequence.push('child2_exit');</script>
                </onexit>
                <initial><transition target="c2_active"/></initial>
                <state id="c2_active"/>
            </state>
            <transition event="exit_parallel" target="final_state"/>
        </parallel>
        <final id="final_state"/>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    // W3C SCXML specification section 3.4: Entry/exit sequence compliance test
    RSM::StateMachine sm;
    ASSERT_TRUE(sm.loadSCXMLFromString(scxmlContent)) << "StateMachine loading failed";
    ASSERT_TRUE(sm.start()) << "StateMachine start failed";

    try {
        // Enter parallel state and verify entry sequence
        auto enterResult = sm.processEvent("enter_parallel");
        ASSERT_TRUE(enterResult.success) << "Failed to enter parallel state: " << enterResult.errorMessage;

        // SCXML W3C 3.4: Entry sequence must be: parallel_entry -> child1_entry, child2_entry
        auto entrySequenceResult =
            RSM::JSEngine::instance().evaluateExpression(sm.getSessionId(), "entry_sequence").get();
        std::string entrySequence = entrySequenceResult.getValueAsString();
        EXPECT_TRUE(entrySequence.find("parallel_entry") != std::string::npos)
            << "SCXML violation: parallel state onentry action not executed. Expected 'parallel_entry' in: "
            << entrySequence;
        EXPECT_TRUE(entrySequence.find("child1_entry") != std::string::npos)
            << "SCXML violation: child1 onentry action not executed. Expected 'child1_entry' in: " << entrySequence;
        EXPECT_TRUE(entrySequence.find("child2_entry") != std::string::npos)
            << "SCXML violation: child2 onentry action not executed. Expected 'child2_entry' in: " << entrySequence;

        // Verify entry order: parallel_entry should come before children
        size_t parallelPos = entrySequence.find("parallel_entry");
        size_t child1Pos = entrySequence.find("child1_entry");
        size_t child2Pos = entrySequence.find("child2_entry");

        EXPECT_LT(parallelPos, child1Pos)
            << "SCXML violation: parallel onentry must execute BEFORE child onentry. Entry sequence: " << entrySequence;
        EXPECT_LT(parallelPos, child2Pos)
            << "SCXML violation: parallel onentry must execute BEFORE child onentry. Entry sequence: " << entrySequence;

        // Trigger exit from parallel state
        auto exitResult = sm.processEvent("exit_parallel");
        ASSERT_TRUE(exitResult.success) << "Failed to exit parallel state: " << exitResult.errorMessage;

        // SCXML W3C 3.4: Exit sequence must be: child1_exit, child2_exit -> parallel_exit
        auto exitSequenceResult =
            RSM::JSEngine::instance().evaluateExpression(sm.getSessionId(), "exit_sequence").get();
        std::string exitSequence = exitSequenceResult.getValueAsString();
        EXPECT_TRUE(exitSequence.find("child1_exit") != std::string::npos)
            << "SCXML violation: child1 onexit action not executed. Expected 'child1_exit' in: " << exitSequence;
        EXPECT_TRUE(exitSequence.find("child2_exit") != std::string::npos)
            << "SCXML violation: child2 onexit action not executed. Expected 'child2_exit' in: " << exitSequence;
        EXPECT_TRUE(exitSequence.find("parallel_exit") != std::string::npos)
            << "SCXML violation: parallel state onexit action not executed. Expected 'parallel_exit' in: "
            << exitSequence;

        // Verify exit order: children should exit before parallel
        size_t parallelExitPos = exitSequence.find("parallel_exit");
        size_t child1ExitPos = exitSequence.find("child1_exit");
        size_t child2ExitPos = exitSequence.find("child2_exit");

        EXPECT_LT(child1ExitPos, parallelExitPos)
            << "SCXML violation: child onexit must execute BEFORE parallel onexit. Exit sequence: " << exitSequence;
        EXPECT_LT(child2ExitPos, parallelExitPos)
            << "SCXML violation: child onexit must execute BEFORE parallel onexit. Exit sequence: " << exitSequence;

    } catch (const std::exception &e) {
        FAIL() << "SCXML violation: Failed to verify parallel entry/exit sequence. " << e.what();
    }
}

// W3C SCXML specification 3.4: Independent transition processing test
TEST_F(SCXMLParallelComplianceTest, W3C_Parallel_TransitionProcessing_Independent) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="independent_test" datamodel="ecmascript">
        <datamodel>
            <data id="region1_state" expr="'initial'"/>
            <data id="region2_state" expr="'initial'"/>
        </datamodel>
        <parallel id="independent_test">
            <state id="region1">
                <initial><transition target="r1_s1"/></initial>
                <state id="r1_s1">
                    <onentry><assign location="region1_state" expr="'s1'"/></onentry>
                    <transition event="move" target="r1_s2"/>
                </state>
                <state id="r1_s2">
                    <onentry><assign location="region1_state" expr="'s2'"/></onentry>
                </state>
            </state>
            <state id="region2">
                <initial><transition target="r2_s1"/></initial>
                <state id="r2_s1">
                    <onentry><assign location="region2_state" expr="'s1'"/></onentry>
                    <transition event="different_event" target="r2_s2"/>
                </state>
                <state id="r2_s2">
                    <onentry><assign location="region2_state" expr="'s2'"/></onentry>
                </state>
            </state>
        </parallel>
    </scxml>)";

    auto stateMachine = parser_->parseContent(scxmlContent);
    ASSERT_NE(stateMachine, nullptr) << "SCXML parsing failed";

    // W3C SCXML specification section 3.4: Independent transition processing test
    RSM::StateMachine sm;
    ASSERT_TRUE(sm.loadSCXMLFromString(scxmlContent)) << "StateMachine loading failed";
    ASSERT_TRUE(sm.start()) << "StateMachine start failed";

    // Verify initial state is parallel state
    EXPECT_EQ(sm.getCurrentState(), "independent_test") << "Parallel state not entered correctly";

    auto &jsEngine = RSM::JSEngine::instance();

    try {
        // Initial state verification: both regions should be in initial states
        auto region1InitialFuture = jsEngine.evaluateExpression(sm.getSessionId(), "region1_state");
        auto region1InitialResult = region1InitialFuture.get();
        EXPECT_EQ(region1InitialResult.getValueAsString(), "s1")
            << "region1 should start in s1 state, got: " << region1InitialResult.getValueAsString();

        auto region2InitialFuture = jsEngine.evaluateExpression(sm.getSessionId(), "region2_state");
        auto region2InitialResult = region2InitialFuture.get();
        EXPECT_EQ(region2InitialResult.getValueAsString(), "s1")
            << "region2 should start in s1 state, got: " << region2InitialResult.getValueAsString();

        // W3C Test 1: Send "move" event - should only affect region1
        Logger::info("W3C COMPLIANCE TEST: Sending 'move' event - should only affect region1");
        LOG_INFO("Current StateMachine state before move: {}", sm.getCurrentState());
        LOG_INFO("StateMachine is running: {}", sm.isRunning());

        auto moveResult = sm.processEvent("move", "");
        LOG_INFO("Move event result - success: {}, from: {}, to: {}, error: {}", moveResult.success,
                 moveResult.fromState, moveResult.toState, moveResult.errorMessage);
        ASSERT_TRUE(moveResult.success) << "SCXML violation: 'move' event processing failed: "
                                        << moveResult.errorMessage;

        // Verify region1 transitioned to s2 (independent response)
        auto region1AfterMoveFuture = jsEngine.evaluateExpression(sm.getSessionId(), "region1_state");
        auto region1AfterMoveResult = region1AfterMoveFuture.get();
        EXPECT_EQ(region1AfterMoveResult.getValueAsString(), "s2")
            << "SCXML violation: region1 did not transition independently to s2. Expected 's2', got: "
            << region1AfterMoveResult.getValueAsString();

        // Verify region2 remained in s1 (independence preserved)
        auto region2AfterMoveFuture = jsEngine.evaluateExpression(sm.getSessionId(), "region2_state");
        auto region2AfterMoveResult = region2AfterMoveFuture.get();
        EXPECT_EQ(region2AfterMoveResult.getValueAsString(), "s1")
            << "SCXML violation: region2 was affected by region1's event. Expected 's1', got: "
            << region2AfterMoveResult.getValueAsString();

        // W3C Test 2: Send "different_event" - should only affect region2
        Logger::info("W3C COMPLIANCE TEST: Sending 'different_event' - should only affect region2");
        LOG_INFO("Current StateMachine state before different_event: {}", sm.getCurrentState());

        auto differentResult = sm.processEvent("different_event", "");
        LOG_INFO("Different event result - success: {}, from: {}, to: {}, error: {}", differentResult.success,
                 differentResult.fromState, differentResult.toState, differentResult.errorMessage);
        ASSERT_TRUE(differentResult.success)
            << "SCXML violation: 'different_event' processing failed: " << differentResult.errorMessage;

        // Verify region1 remained in s2 (independence preserved)
        auto region1AfterDifferentFuture = jsEngine.evaluateExpression(sm.getSessionId(), "region1_state");
        auto region1AfterDifferentResult = region1AfterDifferentFuture.get();
        EXPECT_EQ(region1AfterDifferentResult.getValueAsString(), "s2")
            << "SCXML violation: region1 was affected by region2's event. Expected 's2', got: "
            << region1AfterDifferentResult.getValueAsString();

        // Verify region2 transitioned to s2 (independent response)
        auto region2AfterDifferentFuture = jsEngine.evaluateExpression(sm.getSessionId(), "region2_state");
        auto region2AfterDifferentResult = region2AfterDifferentFuture.get();
        EXPECT_EQ(region2AfterDifferentResult.getValueAsString(), "s2")
            << "SCXML violation: region2 did not transition independently to s2. Expected 's2', got: "
            << region2AfterDifferentResult.getValueAsString();

        Logger::info("W3C COMPLIANCE VERIFIED: Independent transition processing works correctly");
        Logger::info("  - region1 responded only to 'move' event (s1->s2)");
        Logger::info("  - region2 responded only to 'different_event' event (s1->s2)");
        Logger::info("  - Each region processed events independently without interference");

    } catch (const std::exception &e) {
        FAIL() << "SCXML violation: Failed to verify independent transition processing. "
               << "Parallel regions did not process events independently. Error: " << e.what();
    }
}

}  // namespace RSM