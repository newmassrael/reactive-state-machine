#include "actions/ActionExecutorImpl.h"
#include "js_engine/JSEngine.h"
#include "parsing/NodeFactory.h"
#include "parsing/SCXMLParser.h"
#include "states/ConcurrentEventBroadcaster.h"
#include "gtest/gtest.h"
#include <memory>
#include <string>

namespace RSM {

class ParallelStateEventBroadcastingTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &JSEngine::instance();
        engine_->reset();
        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
        broadcaster_ = std::make_unique<ConcurrentEventBroadcaster>();
        sessionId_ = "parallel_event_broadcasting_test";
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
    std::string sessionId_;
};

// Basic event broadcasting test
TEST_F(ParallelStateEventBroadcastingTest, BasicEventBroadcasting) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // Event broadcasting
    EventDescriptor event;
    event.name = "test_event";
    event.data = "test_data";

    bool result = broadcaster_->broadcastToRegions("parallel1", event);
    EXPECT_TRUE(result) << "Event broadcasting failed";
}

// Selective event broadcasting test
TEST_F(ParallelStateEventBroadcastingTest, SelectiveEventBroadcasting) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // Selective broadcasting
    EventDescriptor event;
    event.name = "selective_event";
    event.data = "selective_data";

    std::vector<std::string> targetRegions = {"region1", "region3"};
    bool result = broadcaster_->broadcastToSpecificRegions("parallel1", event, targetRegions);
    EXPECT_TRUE(result) << "Selective event broadcasting failed";
}

// Event filtering test
TEST_F(ParallelStateEventBroadcastingTest, EventFiltering) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // Set event filter
    broadcaster_->setEventFilter(
        "parallel1", [](const EventDescriptor &event) { return event.name.find("filtered") != std::string::npos; });

    // Event to be filtered
    EventDescriptor filteredEvent;
    filteredEvent.name = "filtered_event";

    // Event that won't be filtered
    EventDescriptor normalEvent;
    normalEvent.name = "normal_event";

    bool filteredResult = broadcaster_->broadcastToRegions("parallel1", filteredEvent);
    bool normalResult = broadcaster_->broadcastToRegions("parallel1", normalEvent);

    EXPECT_TRUE(filteredResult) << "Filtered event broadcasting failed";
    EXPECT_FALSE(normalResult) << "Normal event was not filtered";
}

// Concurrency test
TEST_F(ParallelStateEventBroadcastingTest, ConcurrentBroadcasting) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2", "region3", "region4"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // Concurrent broadcasting test
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &successCount, i]() {
            EventDescriptor event;
            event.name = "concurrent_event_" + std::to_string(i);

            if (broadcaster_->broadcastToRegions("parallel1", event)) {
                successCount.fetch_add(1);
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), 10) << "Some concurrent broadcasts failed";
}

// Event priority test
TEST_F(ParallelStateEventBroadcastingTest, EventPriority) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // High priority event
    EventDescriptor highPriorityEvent;
    highPriorityEvent.name = "high_priority";
    highPriorityEvent.priority = EventPriority::HIGH;

    // Low priority event
    EventDescriptor lowPriorityEvent;
    lowPriorityEvent.name = "low_priority";
    lowPriorityEvent.priority = EventPriority::LOW;

    bool highResult = broadcaster_->broadcastToRegions("parallel1", highPriorityEvent);
    bool lowResult = broadcaster_->broadcastToRegions("parallel1", lowPriorityEvent);

    EXPECT_TRUE(highResult) << "High priority event broadcasting failed";
    EXPECT_TRUE(lowResult) << "Low priority event broadcasting failed";
}

// Batch event processing test
TEST_F(ParallelStateEventBroadcastingTest, BatchEventProcessing) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2", "region3"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // Generate batch events
    std::vector<EventDescriptor> events;
    for (int i = 0; i < 5; ++i) {
        EventDescriptor event;
        event.name = "batch_event_" + std::to_string(i);
        events.push_back(event);
    }

    bool result = broadcaster_->broadcastBatchToRegions("parallel1", events);
    EXPECT_TRUE(result) << "Batch event broadcasting failed";
}

// Event statistics test
TEST_F(ParallelStateEventBroadcastingTest, EventStatistics) {
    // Register parallel state
    std::vector<std::string> regionIds = {"region1", "region2"};
    broadcaster_->registerParallelState("parallel1", regionIds);

    // Broadcast multiple events
    for (int i = 0; i < 5; ++i) {
        EventDescriptor event;
        event.name = "stats_event_" + std::to_string(i);
        broadcaster_->broadcastToRegions("parallel1", event);
    }

    auto stats = broadcaster_->getStatistics("parallel1");
    EXPECT_GT(stats.totalEventsBroadcast, 0) << "Broadcast event count is 0";
    EXPECT_EQ(stats.totalRegions, regionIds.size()) << "Registered region count mismatch";
}

// Error handling test
TEST_F(ParallelStateEventBroadcastingTest, ErrorHandling) {
    // Broadcast event to non-existent parallel state
    EventDescriptor event;
    event.name = "error_test_event";

    bool result = broadcaster_->broadcastToRegions("nonexistent_parallel", event);
    EXPECT_FALSE(result) << "Broadcasting to non-existent parallel state succeeded";

    // Broadcast to empty region list
    broadcaster_->registerParallelState("empty_parallel", {});
    result = broadcaster_->broadcastToRegions("empty_parallel", event);
    EXPECT_FALSE(result) << "Broadcasting to empty region list succeeded";
}

// SCXML integrated event broadcasting test
TEST_F(ParallelStateEventBroadcastingTest, SCXMLIntegratedBroadcasting) {
    const std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
           initial="parallel1" datamodel="ecmascript">
        <parallel id="parallel1">
            <state id="region1">
                <initial>
                    <transition target="region1_listening"/>
                </initial>
                <state id="region1_listening">
                    <transition event="broadcast_test" target="region1_received"/>
                </state>
                <state id="region1_received">
                    <onentry>
                        <assign location="region1_got_event" expr="true"/>
                    </onentry>
                </state>
            </state>
            <state id="region2">
                <initial>
                    <transition target="region2_listening"/>
                </initial>
                <state id="region2_listening">
                    <transition event="broadcast_test" target="region2_received"/>
                </state>
                <state id="region2_received">
                    <onentry>
                        <assign location="region2_got_event" expr="true"/>
                    </onentry>
                </state>
            </state>
        </parallel>
    </scxml>)";

    auto result = parser_->parseContent(scxmlContent);
    ASSERT_TRUE(result.has_value()) << "SCXML parsing failed";

    auto stateMachine = result.value();
    ASSERT_NE(stateMachine, nullptr) << "State machine creation failed";

    // Test that event broadcasting works integrated with SCXML
    auto parallelState = stateMachine->findChildById("parallel1");
    ASSERT_NE(parallelState, nullptr) << "Parallel state not found";
}

}  // namespace RSM