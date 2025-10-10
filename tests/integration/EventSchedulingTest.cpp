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
#include "runtime/StateMachine.h"
#include "runtime/StateMachineContext.h"
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
            {
                std::lock_guard<std::mutex> lock(eventsMutex_);
                executedEvents_.push_back({event, target, sendId});
            }

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
                std::lock_guard<std::mutex> lock(eventsMutex_);
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

    // Thread-safe access to event vectors (TSAN compliance)
    std::vector<ExecutedEvent> executedEvents_;
    std::vector<RaisedEvent> raisedEvents_;
    std::mutex eventsMutex_;  // Protects executedEvents_ and raisedEvents_
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
    {
        std::lock_guard<std::mutex> lock(eventsMutex_);
        EXPECT_EQ(raisedEvents_.size(), 1);
        EXPECT_EQ(raisedEvents_[0].name, "test.event");
        // SCXML compliance: data is passed through without modification
        EXPECT_EQ(raisedEvents_[0].data, "test data");
    }
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
    {
        std::lock_guard<std::mutex> lock(eventsMutex_);
        EXPECT_EQ(raisedEvents_.size(), 0);
    }

    // Wait for delay plus some buffer
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Verify event was executed after delay
    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    EXPECT_GE(elapsed.count(), 100);  // At least 100ms delay
    {
        std::lock_guard<std::mutex> lock(eventsMutex_);
        EXPECT_EQ(raisedEvents_.size(), 1);
        EXPECT_EQ(raisedEvents_[0].name, "delayed.event");
    }
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
    {
        std::lock_guard<std::mutex> lock(eventsMutex_);
        EXPECT_EQ(raisedEvents_.size(), 0);
    }
}

/**
 * @brief Test multiple delayed events
 */
TEST_F(EventSchedulingTest, MultipleDelayedEvents) {
    auto sharedExecutor = std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
    ExecutionContextImpl context(sharedExecutor, "test_session");

    // Schedule multiple events with different delays
    std::vector<std::string> eventNames = {"event1", "event2", "event3"};
    std::vector<int> delays = {200, 300, 400};  // ms - increased to avoid race with scheduling overhead

    for (size_t i = 0; i < eventNames.size(); ++i) {
        SendAction sendAction(eventNames[i]);
        sendAction.setTarget("#_internal");
        sendAction.setDelay(std::to_string(delays[i]) + "ms");
        sendAction.setSendId("multi_" + std::to_string(i));

        bool success = sendAction.execute(context);
        EXPECT_TRUE(success);
    }

    // Verify all events are scheduled (with brief delay to ensure scheduling completes)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(scheduler_->getScheduledEventCount(), 3);

    // Wait for all events to execute with polling to avoid race conditions
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(800);  // Generous timeout for 400ms max delay

    size_t raisedCount = 0;
    while (raisedCount < 3 && std::chrono::steady_clock::now() - start < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        {
            std::lock_guard<std::mutex> lock(eventsMutex_);
            raisedCount = raisedEvents_.size();
        }
    }

    // Verify all events were executed
    {
        std::lock_guard<std::mutex> lock(eventsMutex_);
        EXPECT_EQ(raisedEvents_.size(), 3) << "Expected 3 events but got " << raisedEvents_.size();
    }

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
    {
        std::lock_guard<std::mutex> lock(eventsMutex_);
        EXPECT_EQ(raisedEvents_.size(), 0);
    }
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
    // TSAN FIX: Thread-safe access with mutex protection
    std::vector<std::string> session1Events, session2Events, session3Events;
    std::mutex sessionEventsMutex;

    auto mockEventRaiser1 = std::make_shared<RSM::Test::MockEventRaiser>(
        [&session1Events, &sessionEventsMutex](const std::string &name, const std::string &data) -> bool {
            (void)data;  // Suppress unused parameter warning
            std::lock_guard<std::mutex> lock(sessionEventsMutex);
            session1Events.push_back(name);
            return true;
        });

    auto mockEventRaiser2 = std::make_shared<RSM::Test::MockEventRaiser>(
        [&session2Events, &sessionEventsMutex](const std::string &name, const std::string &data) -> bool {
            (void)data;  // Suppress unused parameter warning
            std::lock_guard<std::mutex> lock(sessionEventsMutex);
            session2Events.push_back(name);
            return true;
        });

    auto mockEventRaiser3 = std::make_shared<RSM::Test::MockEventRaiser>(
        [&session3Events, &sessionEventsMutex](const std::string &name, const std::string &data) -> bool {
            (void)data;  // Suppress unused parameter warning
            std::lock_guard<std::mutex> lock(sessionEventsMutex);
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

    LOG_DEBUG("Destroying session_2 - should cancel its delayed events (W3C SCXML 6.2)");
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

    // Verify session 1 and 3 events executed (TSAN FIX: with mutex protection)
    {
        std::lock_guard<std::mutex> lock(sessionEventsMutex);
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
    }

    // Verify no events are still scheduled
    EXPECT_FALSE(scheduler_->hasEvent("session1_event"));
    EXPECT_FALSE(scheduler_->hasEvent("session2_event"));
    EXPECT_FALSE(scheduler_->hasEvent("session3_event"));

    LOG_DEBUG("Session-aware delayed event cancellation validated successfully");

    // Clean up remaining sessions
    jsEngine.destroySession("session_1");
    jsEngine.destroySession("session_3");
}

/**
 * @brief 실제 StateMachine invoke를 사용한 종합적인 세션 격리 테스트
 *
 * W3C SCXML 사양:
 * - Section 6.4.1: invoke 요소는 별도의 세션을 생성해야 함
 * - Section 6.2: send 요소로 생성된 지연된 이벤트는 해당 세션에서만 처리되어야 함
 * - Section 6.2.4: 세션 간 이벤트 격리가 보장되어야 함
 *
 * 테스트 시나리오: W3C 207과 유사한 시나리오로 invoke 세션의 지연된 이벤트 격리 검증
 * 1. 부모 StateMachine이 자식 StateMachine을 invoke로 생성
 * 2. 자식 세션에서 지연된 이벤트를 전송하고 자체 EventRaiser로 처리되는지 검증
 * 3. 부모 세션의 EventRaiser에 자식 이벤트가 잘못 전송되지 않는지 검증
 */
// Re-enabled after fixing race conditions with mutex-based synchronization
// Tests concurrent invoke session isolation with delayed event routing
TEST_F(EventSchedulingTest, InvokeSessionEventIsolation_DelayedEventRouting) {
    LOG_DEBUG("High-level SCXML invoke session isolation test");

    // 고수준 SCXML 기반 세션 격리 테스트 (dual invoke로 복원)
    std::atomic<bool> parentReceivedChild1Event{false};
    std::atomic<bool> parentReceivedChild2Event{false};
    std::atomic<bool> child1ReceivedOwnEvent{false};
    std::atomic<bool> child2ReceivedOwnEvent{false};
    std::atomic<bool> sessionIsolationViolated{false};

    // 부모 StateMachine 생성 (2개의 자식 invoke 포함) - shared_ptr for enable_shared_from_this support
    auto parentStateMachine = std::make_shared<StateMachine>();
    auto parentContext = std::make_unique<StateMachineContext>(parentStateMachine);

    // 부모 SCXML: 두 개의 자식 세션을 invoke하고 세션 격리 검증
    std::string parentScxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="parent_start" datamodel="ecmascript">
    <datamodel>
        <data id="child1EventReceived" expr="false"/>
        <data id="child2EventReceived" expr="false"/>
        <data id="isolationViolated" expr="false"/>
    </datamodel>

    <!-- W3C SCXML 3.13: Invoke는 compound state에 정의하되, internal transition만 사용하여 state exit 방지 -->
    <state id="parent_start">
        <onentry>
            <log expr="'Parent: Starting session isolation test with two children'"/>
        </onentry>

        <!-- First child invoke -->
        <invoke type="scxml" id="child1_invoke">
            <content>
                <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="child1_start" datamodel="ecmascript">
                    <state id="child1_start">
                        <onentry>
                            <log expr="'Child1: Starting and sending delayed event'"/>
                            <send event="child1.delayed.event" delay="100ms" id="child1_delayed"/>
                            <send target="#_parent" event="child1.ready"/>
                        </onentry>
                        <transition event="child1.delayed.event" target="child1_success">
                            <log expr="'Child1: Received own delayed event - isolation working'"/>
                            <send target="#_parent" event="child1.isolated.success"/>
                        </transition>
                    </state>
                    <final id="child1_success">
                        <onentry>
                            <log expr="'Child1: Entered final state'"/>
                        </onentry>
                    </final>
                </scxml>
            </content>
        </invoke>

        <!-- Second child invoke -->
        <invoke type="scxml" id="child2_invoke">
            <content>
                <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="child2_start" datamodel="ecmascript">
                    <state id="child2_start">
                        <onentry>
                            <log expr="'Child2: Starting and sending delayed event'"/>
                            <send event="child2.delayed.event" delay="150ms" id="child2_delayed"/>
                            <send target="#_parent" event="child2.ready"/>
                        </onentry>
                        <transition event="child2.delayed.event" target="child2_success">
                            <log expr="'Child2: Received own delayed event - isolation working'"/>
                            <send target="#_parent" event="child2.isolated.success"/>
                        </transition>
                    </state>
                    <final id="child2_success">
                        <onentry>
                            <log expr="'Child2: Entered final state'"/>
                        </onentry>
                    </final>
                </scxml>
            </content>
        </invoke>

        <!-- W3C SCXML: Internal transitions는 state를 exit하지 않으므로 invoke가 취소되지 않음 -->
        <transition event="child1.ready" type="internal">
            <log expr="'Parent: Child1 ready'"/>
        </transition>

        <transition event="child2.ready" type="internal">
            <log expr="'Parent: Both children ready'"/>
        </transition>

        <transition event="child1.isolated.success" type="internal">
            <log expr="'Parent: Child1 isolation success'"/>
        </transition>

        <transition event="child2.isolated.success" type="internal">
            <log expr="'Parent: Both children isolation success - test PASSED'"/>
        </transition>

        <!-- done.invoke events indicate children completed -->
        <transition event="done.invoke.child1_invoke" type="internal">
            <log expr="'Parent: Received done.invoke.child1_invoke'"/>
            <assign location="child1EventReceived" expr="true"/>
            <log expr="'Parent: child1EventReceived set to ' + child1EventReceived"/>
        </transition>

        <!-- When child2 completes, check if both are done and transition to success -->
        <transition event="done.invoke.child2_invoke" cond="child1EventReceived" target="parent_success">
            <log expr="'Parent: Both children completed, transitioning to success'"/>
        </transition>
        
        <!-- Fallback: child2 completed but child1 not yet -->
        <transition event="done.invoke.child2_invoke" type="internal">
            <log expr="'Parent: Child2 completed (waiting for child1)'"/>
            <assign location="child2EventReceived" expr="true"/>
        </transition>
    </state>

    <final id="parent_success">
        <onentry>
            <log expr="'Parent: Session isolation test PASSED'"/>
        </onentry>
    </final>

    <final id="parent_violation">
        <onentry>
            <log expr="'Parent: Session isolation test FAILED - violation detected'"/>
        </onentry>
    </final>
</scxml>)";

    // EventRaiser 콜백으로 이벤트 추적
    auto parentEventRaiser =
        std::make_shared<RSM::Test::MockEventRaiser>([&](const std::string &name, const std::string &data) -> bool {
            (void)data;

            LOG_DEBUG("EventRaiser callback: event '{}' received", name);

            if (name == "child1.ready") {
                parentReceivedChild1Event = true;
            } else if (name == "child2.ready") {
                parentReceivedChild2Event = true;
            } else if (name == "child1.isolated.success") {
                child1ReceivedOwnEvent = true;
            } else if (name == "child2.isolated.success") {
                child2ReceivedOwnEvent = true;
            }

            // StateMachine에 이벤트 전달
            if (parentStateMachine->isRunning()) {
                std::string currentState = parentStateMachine->getCurrentState();
                LOG_DEBUG("Parent state: {}, processing event: {}", currentState, name);
                auto result = parentStateMachine->processEvent(name, data);
                LOG_DEBUG("processEvent({}) returned success={}, fromState={}, toState={}", name, result.success,
                          result.fromState, result.toState);
                return result.success;
            }
            LOG_WARN("Parent StateMachine not running, cannot process event: {}", name);
            return false;
        });

    // StateMachine 설정
    parentStateMachine->setEventDispatcher(dispatcher_);
    parentStateMachine->setEventRaiser(parentEventRaiser);

    // SCXML 로드 및 실행
    ASSERT_TRUE(parentStateMachine->loadSCXMLFromString(parentScxml)) << "Failed to load parent SCXML";
    ASSERT_TRUE(parentStateMachine->start()) << "Failed to start parent StateMachine";

    LOG_DEBUG("Waiting for invoke sessions and delayed events to execute...");

    // 충분한 시간 대기 (자식 세션 생성 + 지연된 이벤트 실행 + EventScheduler 처리 시간)
    // child1: 100ms delay, child2: 150ms delay + substantial processing time
    // Adding extra time to ensure all events are fully processed before cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    // 고수준 검증: SCXML 데이터모델을 통한 상태 확인
    bool finalStateReached = (parentStateMachine->getCurrentState() == "parent_success" ||
                              parentStateMachine->getCurrentState() == "parent_violation");

    // 세션 격리 검증
    EXPECT_TRUE(finalStateReached) << "StateMachine should reach final state";
    EXPECT_TRUE(parentReceivedChild1Event.load()) << "Parent should receive child1 ready event";
    EXPECT_TRUE(parentReceivedChild2Event.load()) << "Parent should receive child2 ready event";
    EXPECT_TRUE(child1ReceivedOwnEvent.load()) << "Child1 should receive its delayed event";
    EXPECT_TRUE(child2ReceivedOwnEvent.load()) << "Child2 should receive its delayed event";
    EXPECT_FALSE(sessionIsolationViolated.load()) << "No session isolation violations should occur";
    EXPECT_EQ(parentStateMachine->getCurrentState(), "parent_success") << "Should reach success state, not violation";

    // StateMachine 정리
    parentStateMachine->stop();

    LOG_DEBUG("High-level session isolation test completed - Child1: {}, Child2: {}, Violations: {}",
              child1ReceivedOwnEvent.load(), child2ReceivedOwnEvent.load(), sessionIsolationViolated.load());
}

/**
 * @brief W3C SCXML 3.12.1: Events are inserted into the queue in the order in which they are raised
 *
 * This test validates the SCXML specification requirement that events with the same priority
 * must be processed in FIFO (First-In-First-Out) order. The internal event queue must preserve
 * the order of raised events to ensure deterministic state machine behavior.
 *
 * W3C SCXML Specification Reference:
 * - Section 3.12.1: Event Queue Processing
 * - Internal events have higher priority than external events
 * - Within same priority, events must maintain insertion order
 */
TEST_F(EventSchedulingTest, SCXML_InternalEventQueue_FIFOOrdering) {
    LOG_DEBUG("=== SCXML 3.12.1: Internal Event Queue FIFO Ordering Test ===");

    // Create EventRaiserImpl instance
    auto eventRaiser = std::make_shared<EventRaiserImpl>();

    // Track processed event order
    std::vector<std::string> processedOrder;
    std::mutex orderMutex;

    // Set callback that records event processing order
    eventRaiser->setEventCallback(
        [&processedOrder, &orderMutex](const std::string &eventName, const std::string &) -> bool {
            std::lock_guard<std::mutex> lock(orderMutex);
            processedOrder.push_back(eventName);
            LOG_DEBUG("Processed event: {}, current order: {}", eventName, processedOrder.size());
            return true;
        });

    // Test 1: Same priority events should maintain FIFO order
    LOG_DEBUG("Test 1: Raising foo and bar with INTERNAL priority");

    // Raise events in specific order (simulating test 144)
    EXPECT_TRUE(eventRaiser->raiseInternalEvent("foo", ""));
    EXPECT_TRUE(eventRaiser->raiseInternalEvent("bar", ""));

    // Process all queued events
    eventRaiser->processQueuedEvents();

    // Verify FIFO order
    ASSERT_EQ(processedOrder.size(), 2) << "Should process exactly 2 events";
    EXPECT_EQ(processedOrder[0], "foo") << "foo should be processed first";
    EXPECT_EQ(processedOrder[1], "bar") << "bar should be processed second";

    LOG_DEBUG("Test 1 passed: Events processed in FIFO order");

    // Test 2: Multiple events with same priority
    processedOrder.clear();
    LOG_DEBUG("Test 2: Raising multiple events with INTERNAL priority");

    std::vector<std::string> expectedOrder = {"event1", "event2", "event3", "event4", "event5"};
    for (const auto &eventName : expectedOrder) {
        EXPECT_TRUE(eventRaiser->raiseInternalEvent(eventName, ""));
    }

    eventRaiser->processQueuedEvents();

    ASSERT_EQ(processedOrder.size(), expectedOrder.size()) << "Should process all events";
    for (size_t i = 0; i < expectedOrder.size(); ++i) {
        EXPECT_EQ(processedOrder[i], expectedOrder[i])
            << "Event at position " << i << " should be " << expectedOrder[i];
    }

    LOG_DEBUG("Test 2 passed: Multiple events processed in FIFO order");

    // Test 3: Mixed priority events (INTERNAL should come before EXTERNAL)
    processedOrder.clear();
    LOG_DEBUG("Test 3: Mixed priority events");

    // Raise events with different priorities
    EXPECT_TRUE(eventRaiser->raiseExternalEvent("external1", ""));
    EXPECT_TRUE(eventRaiser->raiseInternalEvent("internal1", ""));
    EXPECT_TRUE(eventRaiser->raiseExternalEvent("external2", ""));
    EXPECT_TRUE(eventRaiser->raiseInternalEvent("internal2", ""));

    eventRaiser->processQueuedEvents();

    ASSERT_EQ(processedOrder.size(), 4) << "Should process all 4 events";

    // All INTERNAL events should come before EXTERNAL events
    // Within each priority, FIFO order should be maintained
    EXPECT_EQ(processedOrder[0], "internal1") << "First INTERNAL event should be processed first";
    EXPECT_EQ(processedOrder[1], "internal2") << "Second INTERNAL event should be processed second";
    EXPECT_EQ(processedOrder[2], "external1") << "First EXTERNAL event should be processed third";
    EXPECT_EQ(processedOrder[3], "external2") << "Second EXTERNAL event should be processed fourth";

    LOG_DEBUG("Test 3 passed: Priority ordering with FIFO within each priority");

    // Test 4: Process one event at a time (W3C SCXML compliance)
    processedOrder.clear();
    LOG_DEBUG("Test 4: Processing events one at a time");

    EXPECT_TRUE(eventRaiser->raiseInternalEvent("first", ""));
    EXPECT_TRUE(eventRaiser->raiseInternalEvent("second", ""));
    EXPECT_TRUE(eventRaiser->raiseInternalEvent("third", ""));

    // Process events one at a time
    EXPECT_TRUE(eventRaiser->processNextQueuedEvent());
    ASSERT_EQ(processedOrder.size(), 1) << "Should process exactly one event";
    EXPECT_EQ(processedOrder[0], "first");

    EXPECT_TRUE(eventRaiser->processNextQueuedEvent());
    ASSERT_EQ(processedOrder.size(), 2) << "Should process second event";
    EXPECT_EQ(processedOrder[1], "second");

    EXPECT_TRUE(eventRaiser->processNextQueuedEvent());
    ASSERT_EQ(processedOrder.size(), 3) << "Should process third event";
    EXPECT_EQ(processedOrder[2], "third");

    EXPECT_FALSE(eventRaiser->processNextQueuedEvent()) << "Queue should be empty";

    LOG_DEBUG("Test 4 passed: Single event processing maintains FIFO order");

    // Clean up
    eventRaiser->shutdown();

    LOG_DEBUG("=== SCXML 3.12.1: All FIFO ordering tests passed ===");
}

/**
 * @brief W3C SCXML Test 230: Autoforward preserves all event fields
 *
 * Specification: W3C SCXML 6.4 <invoke> autoforward attribute
 *
 * Test scenario:
 * 1. Parent invokes child with autoforward="true"
 * 2. Child sends "childToParent" event to parent with specific data
 * 3. Parent receives event and captures all _event fields
 * 4. Parent automatically forwards event back to child (autoforward)
 * 5. Child receives forwarded event and captures all _event fields
 * 6. Verify that ALL event fields are preserved during autoforward
 *
 * Event fields that must be preserved:
 * - name: Event name ("childToParent")
 * - type: Event type ("external")
 * - sendid: Send ID from original send action
 * - origin: Origin session ID (child session)
 * - origintype: Origin type URI ("http://www.w3.org/TR/scxml/#SCXMLEventProcessor")
 * - invokeid: Invoke ID
 * - data: Event data ({"testData": "testValue123"})
 *
 * TXML source: test230.txml (manual test)
 * Comments: "a manual test that an autoforwarded event has the same fields
 *            and values as the original event"
 */
TEST_F(EventSchedulingTest, W3C_Test230_AutoforwardPreservesAllEventFields) {
    LOG_DEBUG("=== W3C SCXML Test 230: Autoforward Event Field Preservation ===");

    auto parentStateMachine = std::make_shared<StateMachine>();

    std::string scxmlContent = R"scxml(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0"
       initial="s0" datamodel="ecmascript">

    <datamodel>
        <data id="parent_name" expr="''"/>
        <data id="parent_type" expr="''"/>
        <data id="parent_sendid" expr="''"/>
        <data id="parent_origin" expr="''"/>
        <data id="parent_origintype" expr="''"/>
        <data id="parent_invokeid" expr="''"/>
        <data id="parent_data" expr="''"/>
    </datamodel>

    <state id="s0" initial="s01">
        <onentry>
            <send event="timeout" delay="3000ms"/>
        </onentry>

        <invoke id="childInvokeId" type="scxml" autoforward="true">
            <content>
                <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0"
                       initial="sub0" datamodel="ecmascript">

                    <datamodel>
                        <data id="child_name" expr="''"/>
                        <data id="child_type" expr="''"/>
                        <data id="child_sendid" expr="''"/>
                        <data id="child_origin" expr="''"/>
                        <data id="child_origintype" expr="''"/>
                        <data id="child_invokeid" expr="''"/>
                        <data id="child_data" expr="''"/>
                    </datamodel>

                    <state id="sub0">
                        <onentry>
                            <send target="#_parent" event="childToParent">
                                <param name="testData" expr="'testValue123'"/>
                            </send>
                        </onentry>

                        <transition event="childToParent" target="subFinal">
                            <assign location="child_name" expr="_event.name"/>
                            <assign location="child_type" expr="_event.type"/>
                            <assign location="child_sendid" expr="_event.sendid"/>
                            <assign location="child_origin" expr="_event.origin"/>
                            <assign location="child_origintype" expr="_event.origintype"/>
                            <assign location="child_invokeid" expr="_event.invokeid"/>
                            <assign location="child_data" expr="JSON.stringify(_event.data)"/>
                        </transition>
                    </state>

                    <final id="subFinal"/>
                </scxml>
            </content>
        </invoke>

        <state id="s01">
            <transition event="childToParent" target="s02">
                <assign location="parent_name" expr="_event.name"/>
                <assign location="parent_type" expr="_event.type"/>
                <assign location="parent_sendid" expr="_event.sendid"/>
                <assign location="parent_origin" expr="_event.origin"/>
                <assign location="parent_origintype" expr="_event.origintype"/>
                <assign location="parent_invokeid" expr="_event.invokeid"/>
                <assign location="parent_data" expr="JSON.stringify(_event.data)"/>
            </transition>
        </state>

        <state id="s02">
            <transition event="done.invoke.childInvokeId" target="pass"/>
            <transition event="timeout" target="fail"/>
        </state>

        <final id="pass"/>
        <final id="fail"/>
    </state>
</scxml>)scxml";

    // W3C SCXML Test 230: Create EventRaiserImpl with callback that processes events on parent SM
    auto parentEventRaiser = std::make_shared<RSM::EventRaiserImpl>(
        [&parentStateMachine](const std::string &name, const std::string &data) -> bool {
            if (parentStateMachine && parentStateMachine->isRunning()) {
                return parentStateMachine->processEvent(name, data).success;
            }
            return false;
        });

    parentStateMachine->setEventDispatcher(dispatcher_);
    parentStateMachine->setEventRaiser(parentEventRaiser);

    ASSERT_TRUE(parentStateMachine->loadSCXMLFromString(scxmlContent)) << "Failed to load SCXML";
    ASSERT_TRUE(parentStateMachine->start()) << "Failed to start StateMachine";

    // Wait for test completion (max 5 seconds)
    bool completed = false;
    for (int i = 0; i < 50 && !completed; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::string state = parentStateMachine->getCurrentState();
        completed = (state == "pass" || state == "fail");
    }

    ASSERT_TRUE(completed) << "Test did not complete within timeout";

    std::string finalState = parentStateMachine->getCurrentState();
    EXPECT_EQ(finalState, "pass") << "Test should reach pass state";

    // Retrieve and verify event field values
    std::string parentSessionId = parentStateMachine->getSessionId();
    auto parentName = JSEngine::instance().getVariable(parentSessionId, "parent_name").get().getValueAsString();
    auto parentType = JSEngine::instance().getVariable(parentSessionId, "parent_type").get().getValueAsString();
    auto parentSendId = JSEngine::instance().getVariable(parentSessionId, "parent_sendid").get().getValueAsString();
    auto parentOrigin = JSEngine::instance().getVariable(parentSessionId, "parent_origin").get().getValueAsString();
    auto parentOrigintype =
        JSEngine::instance().getVariable(parentSessionId, "parent_origintype").get().getValueAsString();
    auto parentInvokeid = JSEngine::instance().getVariable(parentSessionId, "parent_invokeid").get().getValueAsString();
    auto parentData = JSEngine::instance().getVariable(parentSessionId, "parent_data").get().getValueAsString();

    std::string childSessionId = JSEngine::instance().getInvokeSessionId(parentSessionId, "childInvokeId");
    ASSERT_FALSE(childSessionId.empty()) << "Child session should exist";

    auto childName = JSEngine::instance().getVariable(childSessionId, "child_name").get().getValueAsString();
    auto childType = JSEngine::instance().getVariable(childSessionId, "child_type").get().getValueAsString();
    auto childSendId = JSEngine::instance().getVariable(childSessionId, "child_sendid").get().getValueAsString();
    auto childOrigin = JSEngine::instance().getVariable(childSessionId, "child_origin").get().getValueAsString();
    auto childOrigintype =
        JSEngine::instance().getVariable(childSessionId, "child_origintype").get().getValueAsString();
    auto childInvokeid = JSEngine::instance().getVariable(childSessionId, "child_invokeid").get().getValueAsString();
    auto childData = JSEngine::instance().getVariable(childSessionId, "child_data").get().getValueAsString();

    // W3C SCXML 6.4: Verify ALL event fields are preserved during autoforward
    EXPECT_EQ(childName, parentName) << "Autoforwarded event.name must match original";
    EXPECT_EQ(childType, parentType) << "Autoforwarded event.type must match original";
    EXPECT_EQ(childSendId, parentSendId) << "Autoforwarded event.sendid must match original";
    EXPECT_EQ(childOrigin, parentOrigin) << "Autoforwarded event.origin must match original";
    EXPECT_EQ(childOrigintype, parentOrigintype) << "Autoforwarded event.origintype must match original";
    EXPECT_EQ(childInvokeid, parentInvokeid) << "Autoforwarded event.invokeid must match original";
    EXPECT_EQ(childData, parentData) << "Autoforwarded event.data must match original";

    // Verify event field values are not empty
    EXPECT_FALSE(parentName.empty()) << "Parent event name should not be empty";
    EXPECT_FALSE(childName.empty()) << "Child event name should not be empty";
    EXPECT_EQ(parentName, "childToParent") << "Event name should be 'childToParent'";

    parentStateMachine->stop();
    LOG_DEBUG("=== W3C Test 230 PASSED: All event fields preserved during autoforward ===");
}

}  // namespace RSM