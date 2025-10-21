#include "states/ExternalTransitionHandler.h"
#include "js_engine/JSEngine.h"
#include "parsing/NodeFactory.h"
#include "parsing/SCXMLParser.h"
#include "gtest/gtest.h"
#include <future>
#include <memory>
#include <string>
#include <thread>

namespace RSM {

class ExternalTransitionHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        engine_->reset();
        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
        handler_ = std::make_unique<ExternalTransitionHandler>(5);  // Max 5 concurrent transitions
        sessionId_ = "external_transition_handler_test";
    }

    void TearDown() override {
        if (engine_) {
            engine_->reset();
        }
    }

    JSEngine *engine_;
    std::shared_ptr<NodeFactory> nodeFactory_;
    std::unique_ptr<SCXMLParser> parser_;
    std::unique_ptr<ExternalTransitionHandler> handler_;
    std::string sessionId_;
};

// Basic external transition handling test
TEST_F(ExternalTransitionHandlerTest, BasicExternalTransitionHandling) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    // Perform external transition
    bool result = handler_->handleExternalTransition("parallel1", "target_state", "exit_event");
    EXPECT_TRUE(result) << "Basic external transition handling failed";
}

// Concurrent transition limit test
TEST_F(ExternalTransitionHandlerTest, ConcurrentTransitionLimit) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    // Attempt concurrent transitions
    std::vector<std::future<bool>> futures;

    for (int i = 0; i < 10; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            return handler_->handleExternalTransition("parallel1", "target_" + std::to_string(i),
                                                      "event_" + std::to_string(i));
        }));
    }

    int successCount = 0;
    for (auto &future : futures) {
        if (future.get()) {
            successCount++;
        }
    }

    // Only up to 5 should succeed
    EXPECT_LE(successCount, 5) << "Concurrent transition limit not enforced";
}

// Active transition count test
TEST_F(ExternalTransitionHandlerTest, ActiveTransitionCount) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    EXPECT_EQ(handler_->getActiveTransitionCount(), 0) << "Initial active transition count is not 0";

    // Start async transitions (actually complete immediately but check count)
    std::vector<std::thread> threads;
    std::atomic<bool> startFlag{false};

    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([this, &startFlag, i]() {
            while (!startFlag.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            handler_->handleExternalTransition("parallel1", "target_" + std::to_string(i),
                                               "event_" + std::to_string(i));
        });
    }

    startFlag.store(true);

    for (auto &thread : threads) {
        thread.join();
    }

    // Count should be 0 after all transitions complete
    EXPECT_EQ(handler_->getActiveTransitionCount(), 0) << "Active transition count is not 0 after completion";
}

// Transition processing status test
TEST_F(ExternalTransitionHandlerTest, TransitionProcessingStatus) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    EXPECT_FALSE(handler_->isProcessingTransitions()) << "Initially in transition processing state";

    // Perform transition
    handler_->handleExternalTransition("parallel1", "target_state", "exit_event");

    // Should not be processing after transition completes
    EXPECT_FALSE(handler_->isProcessingTransitions()) << "Still in processing state after transition completion";
}

// Invalid parameter handling test
TEST_F(ExternalTransitionHandlerTest, InvalidParameterHandling) {
    // Empty parallel state ID
    bool result = handler_->handleExternalTransition("", "target_state", "exit_event");
    EXPECT_FALSE(result) << "Transition succeeded with empty parallel state ID";

    // Empty target state ID
    result = handler_->handleExternalTransition("parallel1", "", "exit_event");
    EXPECT_FALSE(result) << "Transition succeeded with empty target state ID";

    // Empty transition event
    result = handler_->handleExternalTransition("parallel1", "target_state", "");
    EXPECT_FALSE(result) << "Transition succeeded with empty transition event";
}

// Unregistered parallel state handling test
TEST_F(ExternalTransitionHandlerTest, UnregisteredParallelStateHandling) {
    // Attempt transition on unregistered parallel state
    bool result = handler_->handleExternalTransition("unregistered_parallel", "target_state", "exit_event");
    EXPECT_FALSE(result) << "Transition succeeded for unregistered parallel state";
}

// Self-transition test (should be considered internal transition)
TEST_F(ExternalTransitionHandlerTest, SelfTransitionHandling) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    // Attempt self-transition
    bool result = handler_->handleExternalTransition("parallel1", "parallel1", "self_event");
    EXPECT_FALSE(result) << "Self-transition was handled as external transition";
}

// Parallel state registration test
TEST_F(ExternalTransitionHandlerTest, ParallelStateRegistration) {
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};

    // Normal registration
    EXPECT_NO_THROW(handler_->registerParallelState("parallel1", regionIds))
        << "Exception occurred during normal parallel state registration";

    // Attempt registration with empty ID
    EXPECT_THROW(handler_->registerParallelState("", regionIds), std::invalid_argument)
        << "No exception thrown when registering parallel state with empty ID";
}

// Empty region list registration test
TEST_F(ExternalTransitionHandlerTest, EmptyRegionListRegistration) {
    std::vector<std::string> emptyRegionIds;

    // Register with empty region list
    EXPECT_NO_THROW(handler_->registerParallelState("parallel_empty", emptyRegionIds))
        << "Exception occurred during empty region list registration";

    // Attempt transition with empty region list
    bool result = handler_->handleExternalTransition("parallel_empty", "target_state", "exit_event");
    EXPECT_FALSE(result) << "Transition succeeded for parallel state with empty region list";
}

// Exception test for max concurrent transitions set to 0
TEST_F(ExternalTransitionHandlerTest, ZeroMaxConcurrentTransitions) {
    EXPECT_THROW(ExternalTransitionHandler(0), std::invalid_argument)
        << "No exception thrown when creating with max concurrent transitions of 0";
}

// Region deactivation test
TEST_F(ExternalTransitionHandlerTest, RegionDeactivation) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};
    handler_->registerParallelState("parallel1", regionIds);

    // Deactivate regions through external transition
    bool result = handler_->handleExternalTransition("parallel1", "external_target", "exit_event");
    EXPECT_TRUE(result) << "External transition including region deactivation failed";
}

// SCXML integrated external transition test
TEST_F(ExternalTransitionHandlerTest, SCXMLIntegratedExternalTransition) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0"
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <transition event="exit_parallel" target="single_state"/>
            <state id="region1">
                <initial>
                    <transition target="region1_active"/>
                </initial>
                <state id="region1_active">
                    <onexit>
                        <assign location="region1_exited" expr="true"/>
                    </onexit>
                </state>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_active"/>
                </initial>
                <state id="region2_active">
                    <onexit>
                        <assign location="region2_exited" expr="true"/>
                    </onexit>
                </state>
            </state>
        </parallel>
        <state id="single_state">
            <onentry>
                <assign location="single_state_entered" expr="true"/>
            </onentry>
        </state>
    </scxml>)";

    auto result = parser_->parseContent(scxmlContent);
    ASSERT_TRUE(result.has_value()) << "SCXML parsing failed";

    auto stateMachine = result.value();
    ASSERT_NE(stateMachine, nullptr) << "State machine creation failed";

    // Test that external transition handler works integrated with SCXML
    auto parallelState = stateMachine->findChildById("parallel1");
    ASSERT_NE(parallelState, nullptr) << "Parallel state not found";

    auto singleState = stateMachine->findChildById("single_state");
    ASSERT_NE(singleState, nullptr) << "Single state not found";
}

// Performance test - large volume transition handling
TEST_F(ExternalTransitionHandlerTest, PerformanceTest) {
    // Register multiple parallel states
    for (int i = 0; i < 100; ++i) {
        std::vector<std::string> regionIds = {"region1_" + std::to_string(i), "region2_" + std::to_string(i)};
        handler_->registerParallelState("parallel_" + std::to_string(i), regionIds);
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Perform large volume of transitions
    int successCount = 0;
    for (int i = 0; i < 100; ++i) {
        if (handler_->handleExternalTransition("parallel_" + std::to_string(i), "target_" + std::to_string(i),
                                               "event_" + std::to_string(i))) {
            successCount++;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    EXPECT_GT(successCount, 0) << "No transitions succeeded";
    EXPECT_LT(duration.count(), 1000) << "Large volume transition handling performance too slow (exceeds 1 second)";
}

}  // namespace RSM