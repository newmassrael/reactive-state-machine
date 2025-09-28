#include <atomic>
#include <chrono>
#include <future>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>

#include "actions/CancelAction.h"
#include "actions/SendAction.h"
#include "common/Logger.h"
#include "events/EventDispatcherImpl.h"
#include "events/EventSchedulerImpl.h"
#include "events/EventTargetFactoryImpl.h"
#include "events/InternalEventTarget.h"
#include "mocks/MockEventRaiser.h"
#include "runtime/ActionExecutorImpl.h"
#include "runtime/ExecutionContextImpl.h"
#include "scripting/JSEngine.h"

namespace RSM {

/**
 * @brief Mock event target for testing
 */
class MockEventTarget : public IEventTarget {
public:
    MOCK_METHOD(std::future<SendResult>, send, (const EventDescriptor &event), (override));
    MOCK_METHOD(std::string, getTargetType, (), (const, override));
    MOCK_METHOD(bool, canHandle, (const std::string &targetUri), (const, override));
    MOCK_METHOD(std::vector<std::string>, validate, (), (const, override));
    MOCK_METHOD(std::string, getDebugInfo, (), (const, override));
};

/**
 * @brief Test fixture for SCXML event scheduling functionality
 */
class EventSchedulingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // JSEngine 리셋으로 테스트 간 격리 보장
        auto &jsEngine = JSEngine::instance();
        jsEngine.reset();

        jsEngine.createSession("test_session");

        // Create event execution callback (SCXML compliant - delegates to target)
        eventExecutionCallback_ = [this](const EventDescriptor &event, std::shared_ptr<IEventTarget> target,
                                         const std::string &sendId) -> bool {
            executedEvents_.push_back({event, target, sendId});

            // SCXML Compliance: Always delegate to target for proper event handling
            // InternalEventTarget will call ActionExecutor's callback which adds to raisedEvents_
            try {
                auto future = target->send(event);
                auto result = future.get();
                return result.isSuccess;
            } catch (...) {
                return false;
            }
        };

        // Create scheduler
        scheduler_ = std::make_shared<EventSchedulerImpl>(eventExecutionCallback_);

        // Create ActionExecutor first (without dispatcher)
        actionExecutor_ = std::make_shared<ActionExecutorImpl>("test_session");

        // Set up event raising with MockEventRaiser
        raisedEvents_.clear();
        mockEventRaiser_ = std::make_shared<RSM::Test::MockEventRaiser>(
            [this](const std::string &name, const std::string &data) -> bool {
                raisedEvents_.push_back({name, data});
                return true;
            });
        actionExecutor_->setEventRaiser(mockEventRaiser_);

        // Create target factory using MockEventRaiser
        targetFactory_ = std::make_shared<EventTargetFactoryImpl>(mockEventRaiser_);

        // Create dispatcher with proper target factory
        dispatcher_ = std::make_shared<EventDispatcherImpl>(scheduler_, targetFactory_);

        // Set EventDispatcher using the new setEventDispatcher method
        actionExecutor_->setEventDispatcher(dispatcher_);

        // Note: We use the same actionExecutor_ for tests - it has both callback and can use dispatcher
        // The InternalEventTarget created by targetFactory_ will use the same ActionExecutor with callback
    }

    void TearDown() override {
        if (scheduler_) {
            scheduler_->shutdown(true);
        }

        if (dispatcher_) {
            dispatcher_->shutdown();
        }

        // Clean up JSEngine sessions
        auto &jsEngine = JSEngine::instance();
        jsEngine.destroySession("test_session");

        executedEvents_.clear();
        raisedEvents_.clear();
    }

protected:
    struct ExecutedEvent {
        EventDescriptor event;
        std::shared_ptr<IEventTarget> target;
        std::string sendId;
    };

    struct RaisedEvent {
        std::string name;
        std::string data;
    };

    std::shared_ptr<ActionExecutorImpl> actionExecutor_;
    std::shared_ptr<EventTargetFactoryImpl> targetFactory_;
    std::shared_ptr<EventSchedulerImpl> scheduler_;
    std::shared_ptr<EventDispatcherImpl> dispatcher_;
    std::shared_ptr<RSM::Test::MockEventRaiser> mockEventRaiser_;
    EventExecutionCallback eventExecutionCallback_;

    std::vector<ExecutedEvent> executedEvents_;
    std::vector<RaisedEvent> raisedEvents_;
};

/**
 * @brief Debug test to isolate exact hanging point
 */
TEST_F(EventSchedulingTest, DebugHangingPoint) {
    RSM::Logger::debug("Test started");

    // Step 1: Create send action
    RSM::Logger::debug("Creating SendAction");
    SendAction sendAction("test.event");
    RSM::Logger::debug("SendAction created");

    // Step 2: Set target
    RSM::Logger::debug("Setting target");
    sendAction.setTarget("#_internal");
    RSM::Logger::debug("Target set");

    // Step 3: Set data
    RSM::Logger::debug("Setting data");
    sendAction.setData("'test data'");
    RSM::Logger::debug("Data set");

    // Step 4: Create execution context
    RSM::Logger::debug("Creating execution context");
    auto sharedExecutor = std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
    RSM::Logger::debug("Shared executor created");

    ExecutionContextImpl context(sharedExecutor, "test_session");
    RSM::Logger::debug("Execution context created");

    // Step 5: Execute send action (this is likely where it hangs)
    RSM::Logger::debug("About to execute send action");

    bool success = sendAction.execute(context);

    LOG_DEBUG("Send action executed, success={}", success);
    EXPECT_TRUE(success);
}

/**
 * @brief Test immediate event sending (delay = 0)
 */
TEST_F(EventSchedulingTest, ImmediateEventSending) {
    // Create send action with no delay
    SendAction sendAction("test.event");
    sendAction.setTarget("#_internal");
    sendAction.setData("'test data'");

    // Create execution context
    auto sharedExecutor = std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
    ExecutionContextImpl context(sharedExecutor, "test_session");

    // Execute send action
    bool success = sendAction.execute(context);

    // Verify immediate execution
    EXPECT_TRUE(success);

    // Give scheduler time to process (should be immediate)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify event was raised internally
    EXPECT_EQ(raisedEvents_.size(), 1);
    EXPECT_EQ(raisedEvents_[0].name, "test.event");
    // SCXML compliance: data is passed through without modification
    EXPECT_EQ(raisedEvents_[0].data, "test data");
}

/**
 * @brief Test delayed event sending
 */
TEST_F(EventSchedulingTest, DelayedEventSending) {
    // Create send action with delay
    SendAction sendAction("delayed.event");
    sendAction.setTarget("#_internal");
    sendAction.setDelay("100ms");
    sendAction.setSendId("delayed_001");

    // Create execution context
    auto sharedExecutor = std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
    ExecutionContextImpl context(sharedExecutor, "test_session");

    auto startTime = std::chrono::steady_clock::now();

    // Execute send action
    bool success = sendAction.execute(context);
    EXPECT_TRUE(success);

    // Verify event is NOT immediately executed
    EXPECT_EQ(raisedEvents_.size(), 0);

    // Wait for delay plus some buffer
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Verify event was executed after delay
    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    EXPECT_GE(elapsed.count(), 100);  // At least 100ms delay
    EXPECT_EQ(raisedEvents_.size(), 1);
    EXPECT_EQ(raisedEvents_[0].name, "delayed.event");
}

/**
 * @brief Test event cancellation
 */
TEST_F(EventSchedulingTest, EventCancellation) {
    // Create send action with delay
    SendAction sendAction("cancellable.event");
    sendAction.setTarget("#_internal");
    sendAction.setDelay("500ms");
    sendAction.setSendId("cancel_test_001");

    // Create execution context
    auto sharedExecutor = std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
    ExecutionContextImpl context(sharedExecutor, "test_session");

    // Execute send action
    bool sendSuccess = sendAction.execute(context);
    EXPECT_TRUE(sendSuccess);

    // Verify event is scheduled
    EXPECT_TRUE(scheduler_->hasEvent("cancel_test_001"));

    // Wait a bit but not full delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Cancel the event
    CancelAction cancelAction("cancel_test_001");
    bool cancelSuccess = cancelAction.execute(context);
    EXPECT_TRUE(cancelSuccess);

    // Verify event is no longer scheduled
    EXPECT_FALSE(scheduler_->hasEvent("cancel_test_001"));

    // Wait for original delay time
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify event was NOT executed
    EXPECT_EQ(raisedEvents_.size(), 0);
}

/**
 * @brief Test multiple delayed events
 */
TEST_F(EventSchedulingTest, MultipleDelayedEvents) {
    auto sharedExecutor = std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
    ExecutionContextImpl context(sharedExecutor, "test_session");

    // Schedule multiple events with different delays
    std::vector<std::string> eventNames = {"event1", "event2", "event3"};
    std::vector<int> delays = {50, 100, 150};  // ms

    for (size_t i = 0; i < eventNames.size(); ++i) {
        SendAction sendAction(eventNames[i]);
        sendAction.setTarget("#_internal");
        sendAction.setDelay(std::to_string(delays[i]) + "ms");
        sendAction.setSendId("multi_" + std::to_string(i));

        bool success = sendAction.execute(context);
        EXPECT_TRUE(success);
    }

    // Verify all events are scheduled
    EXPECT_EQ(scheduler_->getScheduledEventCount(), 3);

    // Wait for all events to execute with polling to avoid race conditions
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(500);  // Generous timeout

    while (raisedEvents_.size() < 3 && std::chrono::steady_clock::now() - start < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Verify all events were executed
    EXPECT_EQ(raisedEvents_.size(), 3) << "Expected 3 events but got " << raisedEvents_.size();

    // Verify no events are still scheduled
    EXPECT_EQ(scheduler_->getScheduledEventCount(), 0);
}

/**
 * @brief Test scheduler statistics and status
 */
TEST_F(EventSchedulingTest, SchedulerStatistics) {
    // Verify initial state
    EXPECT_TRUE(scheduler_->isRunning());
    EXPECT_EQ(scheduler_->getScheduledEventCount(), 0);

    // Schedule some events
    auto sharedExecutor = std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
    ExecutionContextImpl context(sharedExecutor, "test_session");

    SendAction sendAction1("stats.event1");
    sendAction1.setTarget("#_internal");
    sendAction1.setDelay("1000ms");  // Long delay
    sendAction1.setSendId("stats_001");
    sendAction1.execute(context);

    SendAction sendAction2("stats.event2");
    sendAction2.setTarget("#_internal");
    sendAction2.setDelay("2000ms");  // Longer delay
    sendAction2.setSendId("stats_002");
    sendAction2.execute(context);

    // Check statistics
    EXPECT_EQ(scheduler_->getScheduledEventCount(), 2);
    EXPECT_TRUE(scheduler_->hasEvent("stats_001"));
    EXPECT_TRUE(scheduler_->hasEvent("stats_002"));

    // Check dispatcher statistics
    std::string dispatcherStats = dispatcher_->getStatistics();
    EXPECT_FALSE(dispatcherStats.empty());
    EXPECT_NE(dispatcherStats.find("Running"), std::string::npos);
    EXPECT_NE(dispatcherStats.find("Pending Events: 2"), std::string::npos);
}

/**
 * @brief Test error handling for invalid send IDs
 */
TEST_F(EventSchedulingTest, InvalidSendIdHandling) {
    auto sharedExecutor = std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
    ExecutionContextImpl context(sharedExecutor, "test_session");

    // Try to cancel non-existent event
    CancelAction cancelAction("non_existent_id");
    bool success = cancelAction.execute(context);

    // Cancel should succeed even if event doesn't exist (W3C SCXML spec)
    EXPECT_TRUE(success);

    // Try to cancel with empty send ID (should fail validation)
    CancelAction emptyCancelAction("");
    bool emptySuccess = emptyCancelAction.execute(context);
    EXPECT_FALSE(emptySuccess);
}

/**
 * @brief Test graceful shutdown with pending events
 */
TEST_F(EventSchedulingTest, ShutdownWithPendingEvents) {
    auto sharedExecutor = std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
    ExecutionContextImpl context(sharedExecutor, "test_session");

    // Schedule events with long delays
    SendAction sendAction("shutdown.event");
    sendAction.setTarget("#_internal");
    sendAction.setDelay("5000ms");
    sendAction.setSendId("shutdown_001");
    sendAction.execute(context);

    EXPECT_EQ(scheduler_->getScheduledEventCount(), 1);

    // Shutdown scheduler
    scheduler_->shutdown(false);  // Don't wait for completion

    // Verify scheduler stopped
    EXPECT_FALSE(scheduler_->isRunning());
    EXPECT_EQ(scheduler_->getScheduledEventCount(), 0);

    // Verify event was not executed
    EXPECT_EQ(raisedEvents_.size(), 0);
}

/**
 * @brief Test session-aware delayed event cancellation (W3C SCXML 6.2 compliance)
 *
 * This test validates our implementation of W3C SCXML 6.2 requirement:
 * "When a session terminates, all delayed events scheduled by that session must be cancelled"
 */
TEST_F(EventSchedulingTest, SessionAwareDelayedEventCancellation) {
    auto &jsEngine = JSEngine::instance();

    // Create additional sessions for testing
    jsEngine.createSession("session_1");
    jsEngine.createSession("session_2");
    jsEngine.createSession("session_3");

    // Create ActionExecutors for each session
    auto actionExecutor1 = std::make_shared<ActionExecutorImpl>("session_1");
    auto actionExecutor2 = std::make_shared<ActionExecutorImpl>("session_2");
    auto actionExecutor3 = std::make_shared<ActionExecutorImpl>("session_3");

    // Set up event raising for each session
    std::vector<std::string> session1Events, session2Events, session3Events;

    auto mockEventRaiser1 = std::make_shared<RSM::Test::MockEventRaiser>(
        [&session1Events](const std::string &name, const std::string &data) -> bool {
            (void)data;  // Suppress unused parameter warning
            session1Events.push_back(name);
            return true;
        });

    auto mockEventRaiser2 = std::make_shared<RSM::Test::MockEventRaiser>(
        [&session2Events](const std::string &name, const std::string &data) -> bool {
            (void)data;  // Suppress unused parameter warning
            session2Events.push_back(name);
            return true;
        });

    auto mockEventRaiser3 = std::make_shared<RSM::Test::MockEventRaiser>(
        [&session3Events](const std::string &name, const std::string &data) -> bool {
            (void)data;  // Suppress unused parameter warning
            session3Events.push_back(name);
            return true;
        });

    actionExecutor1->setEventRaiser(mockEventRaiser1);
    actionExecutor2->setEventRaiser(mockEventRaiser2);
    actionExecutor3->setEventRaiser(mockEventRaiser3);

    // Create separate dispatchers for each session to ensure proper event routing
    auto targetFactory1 = std::make_shared<EventTargetFactoryImpl>(mockEventRaiser1);
    auto targetFactory2 = std::make_shared<EventTargetFactoryImpl>(mockEventRaiser2);
    auto targetFactory3 = std::make_shared<EventTargetFactoryImpl>(mockEventRaiser3);

    auto dispatcher1 = std::make_shared<EventDispatcherImpl>(scheduler_, targetFactory1);
    auto dispatcher2 = std::make_shared<EventDispatcherImpl>(scheduler_, targetFactory2);
    auto dispatcher3 = std::make_shared<EventDispatcherImpl>(scheduler_, targetFactory3);

    // Set EventDispatcher for each session (this registers them with JSEngine)
    actionExecutor1->setEventDispatcher(dispatcher1);
    actionExecutor2->setEventDispatcher(dispatcher2);
    actionExecutor3->setEventDispatcher(dispatcher3);

    // Schedule delayed events from each session
    SendAction sendAction1("session1.event");
    sendAction1.setTarget("#_internal");
    sendAction1.setDelay("300ms");
    sendAction1.setSendId("session1_event");

    SendAction sendAction2("session2.event");
    sendAction2.setTarget("#_internal");
    sendAction2.setDelay("300ms");
    sendAction2.setSendId("session2_event");

    SendAction sendAction3("session3.event");
    sendAction3.setTarget("#_internal");
    sendAction3.setDelay("300ms");
    sendAction3.setSendId("session3_event");

    // Create execution contexts with proper shared_ptr management
    auto sharedExecutor1 = std::static_pointer_cast<IActionExecutor>(actionExecutor1);
    auto sharedExecutor2 = std::static_pointer_cast<IActionExecutor>(actionExecutor2);
    auto sharedExecutor3 = std::static_pointer_cast<IActionExecutor>(actionExecutor3);

    ExecutionContextImpl context1(sharedExecutor1, "session_1");
    ExecutionContextImpl context2(sharedExecutor2, "session_2");
    ExecutionContextImpl context3(sharedExecutor3, "session_3");

    // Execute send actions - all should succeed
    auto startTime = std::chrono::steady_clock::now();
    EXPECT_TRUE(sendAction1.execute(context1));
    EXPECT_TRUE(sendAction2.execute(context2));
    EXPECT_TRUE(sendAction3.execute(context3));

    // Verify all events are scheduled
    EXPECT_TRUE(scheduler_->hasEvent("session1_event"));
    EXPECT_TRUE(scheduler_->hasEvent("session2_event"));
    EXPECT_TRUE(scheduler_->hasEvent("session3_event"));

    // Wait 100ms, then destroy session_2 (W3C SCXML 6.2: should cancel its delayed events)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    LOG_INFO("TEST: Destroying session_2 - should cancel its delayed events (W3C SCXML 6.2)");
    jsEngine.destroySession("session_2");

    // Session 2's event should now be cancelled
    EXPECT_FALSE(scheduler_->hasEvent("session2_event"));

    // Session 1 and 3 events should still be scheduled
    EXPECT_TRUE(scheduler_->hasEvent("session1_event"));
    EXPECT_TRUE(scheduler_->hasEvent("session3_event"));

    // Wait for remaining events to execute (300ms total - 100ms already passed = 200ms more)
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Verify timing (should be around 300ms)
    EXPECT_GE(elapsed.count(), 300);

    // Verify session 1 and 3 events executed
    EXPECT_EQ(session1Events.size(), 1);
    if (session1Events.size() > 0) {
        EXPECT_EQ(session1Events[0], "session1.event");
    }

    EXPECT_EQ(session3Events.size(), 1);
    if (session3Events.size() > 0) {
        EXPECT_EQ(session3Events[0], "session3.event");
    }

    // Verify session 2 event was cancelled and never executed
    EXPECT_EQ(session2Events.size(), 0);

    // Verify no events are still scheduled
    EXPECT_FALSE(scheduler_->hasEvent("session1_event"));
    EXPECT_FALSE(scheduler_->hasEvent("session2_event"));
    EXPECT_FALSE(scheduler_->hasEvent("session3_event"));

    LOG_INFO("TEST: Session-aware delayed event cancellation validated successfully");

    // Clean up remaining sessions
    jsEngine.destroySession("session_1");
    jsEngine.destroySession("session_3");
}

}  // namespace RSM