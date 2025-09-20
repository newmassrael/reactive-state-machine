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

        // Set up event raise callback
        raisedEvents_.clear();
        actionExecutor_->setEventRaiseCallback([this](const std::string &name, const std::string &data) -> bool {
            raisedEvents_.push_back({name, data});
            return true;
        });

        // Create target factory using the EventRaiser from ActionExecutor
        targetFactory_ = std::make_shared<EventTargetFactoryImpl>(actionExecutor_->getEventRaiser());

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
    EventExecutionCallback eventExecutionCallback_;

    std::vector<ExecutedEvent> executedEvents_;
    std::vector<RaisedEvent> raisedEvents_;
};

/**
 * @brief Debug test to isolate exact hanging point
 */
TEST_F(EventSchedulingTest, DebugHangingPoint) {
    std::cout << "DEBUG: Test started" << std::endl;

    // Step 1: Create send action
    std::cout << "DEBUG: Creating SendAction" << std::endl;
    SendAction sendAction("test.event");
    std::cout << "DEBUG: SendAction created" << std::endl;

    // Step 2: Set target
    std::cout << "DEBUG: Setting target" << std::endl;
    sendAction.setTarget("#_internal");
    std::cout << "DEBUG: Target set" << std::endl;

    // Step 3: Set data
    std::cout << "DEBUG: Setting data" << std::endl;
    sendAction.setData("'test data'");
    std::cout << "DEBUG: Data set" << std::endl;

    // Step 4: Create execution context
    std::cout << "DEBUG: Creating execution context" << std::endl;
    auto sharedExecutor = std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
    std::cout << "DEBUG: Shared executor created" << std::endl;

    ExecutionContextImpl context(sharedExecutor, "test_session");
    std::cout << "DEBUG: Execution context created" << std::endl;

    // Step 5: Execute send action (this is likely where it hangs)
    std::cout << "DEBUG: About to execute send action" << std::endl;
    std::cout.flush();

    bool success = sendAction.execute(context);

    std::cout << "DEBUG: Send action executed, success=" << success << std::endl;
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

}  // namespace RSM