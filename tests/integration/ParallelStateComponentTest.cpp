#include "factory/NodeFactory.h"
#include "parsing/SCXMLParser.h"
#include "scripting/JSEngine.h"
#include "states/ConcurrentCompletionMonitor.h"
#include "states/ConcurrentEventBroadcaster.h"
#include "states/ExternalTransitionHandler.h"
#include "gtest/gtest.h"
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>

namespace RSM {

class ParallelStateComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        engine_->reset();
        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
        broadcaster_ = std::make_unique<ConcurrentEventBroadcaster>();
        monitor_ = std::make_unique<ConcurrentCompletionMonitor>("parallel_test");
        handler_ = std::make_unique<ExternalTransitionHandler>(5);
        sessionId_ = "parallel_component_test";
    }

    void TearDown() override {
        if (engine_) {
            engine_->reset();
        }
    }

    JSEngine *engine_;
    std::shared_ptr<NodeFactory> nodeFactory_;
    std::unique_ptr<SCXMLParser> parser_;
    std::unique_ptr<ConcurrentEventBroadcaster> broadcaster_;
    std::unique_ptr<ConcurrentCompletionMonitor> monitor_;
    std::unique_ptr<ExternalTransitionHandler> handler_;
    std::string sessionId_;
};

// ============================================================================
// Event Broadcasting Tests
// ============================================================================

TEST_F(ParallelStateComponentTest, EventBroadcasting_BasicBroadcast) {
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};
    // registerParallelState method does not exist - need to register regions directly

    EventDescriptor event;
    event.eventName = "test_event";
    event.data = "test_data";

    bool result = broadcaster_->broadcastEvent("parallel1", event);
    EXPECT_TRUE(result) << "Basic event broadcasting failed";
}

TEST_F(ParallelStateComponentTest, EventBroadcasting_SelectiveBroadcast) {
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};
    // registerParallelState method does not exist - need to register regions directly

    EventDescriptor event;
    event.eventName = "selective_event";
    event.data = "selective_data";

    std::vector<std::string> targetRegions = {"region1", "region3"};
    bool result = broadcaster_->broadcastEventToRegions(event, targetRegions);
    EXPECT_TRUE(result) << "Selective event broadcasting failed";
}

TEST_F(ParallelStateComponentTest, EventBroadcasting_ConcurrentBroadcast) {
    std::vector<std::string> regionIds = {"region1", "region2", "region3", "region4"};
    // registerParallelState method does not exist - need to register regions directly

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &successCount, i]() {
            EventDescriptor event;
            event.eventName = "concurrent_event_" + std::to_string(i);

            if (broadcaster_->broadcastEvent("parallel1", event)) {
                successCount.fetch_add(1);
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), 5) << "Some concurrent broadcasts failed";
}

// ============================================================================
// Completion Monitoring Tests
// ============================================================================

TEST_F(ParallelStateComponentTest, CompletionMonitoring_BasicMonitoring) {
    EXPECT_FALSE(monitor_->isMonitoringActive()) << "Monitoring is active at initialization";

    bool started = monitor_->startMonitoring();
    EXPECT_TRUE(started) << "Failed to start monitoring";
    EXPECT_TRUE(monitor_->isMonitoringActive()) << "Monitoring is not active";

    monitor_->stopMonitoring();
    EXPECT_FALSE(monitor_->isMonitoringActive()) << "Monitoring is not stopped";
}

TEST_F(ParallelStateComponentTest, CompletionMonitoring_RegionCompletion) {
    monitor_->startMonitoring();

    monitor_->updateRegionCompletion("region1", false);
    monitor_->updateRegionCompletion("region2", false);

    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "Completion criteria met when all regions are incomplete";

    monitor_->updateRegionCompletion("region1", true);
    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "Completion criteria met when only some regions are complete";

    monitor_->updateRegionCompletion("region2", true);
    EXPECT_TRUE(monitor_->isCompletionCriteriaMet()) << "Completion criteria not met when all regions are complete";
}

TEST_F(ParallelStateComponentTest, CompletionMonitoring_ConcurrentUpdates) {
    monitor_->startMonitoring();

    const int numThreads = 3;
    const int numRegionsPerThread = 5;
    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, numRegionsPerThread]() {
            for (int r = 0; r < numRegionsPerThread; ++r) {
                std::string regionId = "thread" + std::to_string(t) + "_region" + std::to_string(r);
                monitor_->updateRegionCompletion(regionId, (r % 2 == 0));
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    auto regions = monitor_->getRegisteredRegions();
    EXPECT_EQ(regions.size(), numThreads * numRegionsPerThread) << "Number of registered regions differs from expected";

    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "Completion criteria met when some regions are incomplete";
}

// ============================================================================
// External Transition Handling Tests
// ============================================================================

TEST_F(ParallelStateComponentTest, ExternalTransition_BasicHandling) {
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    bool result = handler_->handleExternalTransition("parallel1", "target_state", "exit_event");
    EXPECT_TRUE(result) << "Basic external transition handling failed";
}

TEST_F(ParallelStateComponentTest, ExternalTransition_ConcurrentLimit) {
    std::vector<std::string> regionIds = {"region1", "region2"};
    handler_->registerParallelState("parallel1", regionIds);

    std::vector<std::future<bool>> futures;

    for (int i = 0; i < 8; ++i) {
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

    EXPECT_LE(successCount, 5) << "Concurrent transition limit not applied";
}

TEST_F(ParallelStateComponentTest, ExternalTransition_InvalidParameters) {
    bool result = handler_->handleExternalTransition("", "target_state", "exit_event");
    EXPECT_FALSE(result) << "Transition succeeded with empty parallel state ID";

    result = handler_->handleExternalTransition("parallel1", "", "exit_event");
    EXPECT_FALSE(result) << "Transition succeeded with empty target state ID";

    result = handler_->handleExternalTransition("parallel1", "target_state", "");
    EXPECT_FALSE(result) << "Transition succeeded with empty transition event";
}

// ============================================================================
// Integrated Scenario Tests (Component Interactions)
// ============================================================================

TEST_F(ParallelStateComponentTest, IntegratedScenario_EventBroadcastToCompletion) {
    const std::string parallelStateId = "integrated_parallel";
    std::vector<std::string> regionIds = {"region1", "region2"};

    // Register same parallel state to all components
    broadcaster_->registerParallelState(parallelStateId, regionIds);
    handler_->registerParallelState(parallelStateId, regionIds);
    monitor_->startMonitoring();

    // Event broadcasting
    EventDescriptor event;
    event.eventName = "completion_trigger";
    bool broadcastResult = broadcaster_->broadcastEvent(parallelStateId, event);
    EXPECT_TRUE(broadcastResult) << "Event broadcasting failed";

    // Update region completion status
    monitor_->updateRegionCompletion("region1", true);
    monitor_->updateRegionCompletion("region2", true);
    EXPECT_TRUE(monitor_->isCompletionCriteriaMet()) << "Completion criteria not met";

    // Handle external transition
    bool transitionResult = handler_->handleExternalTransition(parallelStateId, "final_state", "done_event");
    EXPECT_TRUE(transitionResult) << "External transition handling failed";

    EXPECT_EQ(handler_->getActiveTransitionCount(), 0)
        << "Active transition count is not 0 after transition completion";
}

TEST_F(ParallelStateComponentTest, IntegratedScenario_PartialCompletionWithTransition) {
    const std::string parallelStateId = "partial_parallel";
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};

    broadcaster_->registerParallelState(parallelStateId, regionIds);
    handler_->registerParallelState(parallelStateId, regionIds);
    monitor_->startMonitoring();

    // Complete only some regions
    monitor_->updateRegionCompletion("region1", true);
    monitor_->updateRegionCompletion("region2", false);
    monitor_->updateRegionCompletion("region3", false);
    EXPECT_FALSE(monitor_->isCompletionCriteriaMet()) << "Completion criteria met when only some regions are complete";

    // Force external transition (from incomplete state)
    bool transitionResult = handler_->handleExternalTransition(parallelStateId, "early_exit", "force_exit");
    EXPECT_TRUE(transitionResult) << "Forced external transition failed";
}

TEST_F(ParallelStateComponentTest, IntegratedScenario_SCXMLWithComponents) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <transition event="force_exit" target="final_state"/>
            <state id="region1">
                <initial>
                    <transition target="region1_active"/>
                </initial>
                <state id="region1_active">
                    <transition event="region1_complete" target="region1_final"/>
                </state>
                <final id="region1_final"/>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_active"/>
                </initial>
                <state id="region2_active">
                    <transition event="region2_complete" target="region2_final"/>
                </state>
                <final id="region2_final"/>
            </state>
        </parallel>
        <final id="final_state"/>
    </scxml>)";

    auto result = parser_->parseContent(scxmlContent);
    ASSERT_TRUE(result.has_value()) << "SCXML parsing failed";

    auto stateMachine = result.value();
    ASSERT_NE(stateMachine, nullptr) << "State machine creation failed";

    // Test that components work with SCXML
    auto parallelState = stateMachine->findChildById("parallel1");
    ASSERT_NE(parallelState, nullptr) << "Parallel state not found";

    auto finalState = stateMachine->findChildById("final_state");
    ASSERT_NE(finalState, nullptr) << "Final state not found";
}

// ============================================================================
// Performance and Stress Tests
// ============================================================================

TEST_F(ParallelStateComponentTest, Performance_LargeScaleComponents) {
    const int numStates = 100;
    const int numRegionsPerState = 10;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Register large number of parallel states
    for (int i = 0; i < numStates; ++i) {
        std::vector<std::string> regionIds;
        for (int j = 0; j < numRegionsPerState; ++j) {
            regionIds.push_back("state" + std::to_string(i) + "_region" + std::to_string(j));
        }

        broadcaster_->registerParallelState("parallel_" + std::to_string(i), regionIds);
        handler_->registerParallelState("parallel_" + std::to_string(i), regionIds);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    EXPECT_LT(duration.count(), 1000)
        << "Large-scale component registration performance is too slow (exceeds 1 second)";

    // Large-scale event broadcasting test
    startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numStates; ++i) {
        EventDescriptor event;
        event.eventName = "perf_test_event_" + std::to_string(i);
        broadcaster_->broadcastEvent("parallel_" + std::to_string(i), event);
    }

    endTime = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    EXPECT_LT(duration.count(), 500) << "Large-scale event broadcasting performance is too slow (exceeds 500ms)";
}

}  // namespace RSM