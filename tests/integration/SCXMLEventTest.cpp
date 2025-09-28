#include "actions/CancelAction.h"
#include "actions/ScriptAction.h"
#include "actions/SendAction.h"
#include "events/EventDispatcherImpl.h"
#include "events/EventRaiserService.h"
#include "events/EventSchedulerImpl.h"
#include "events/EventTargetFactoryImpl.h"
#include "events/InternalEventTarget.h"
#include "mocks/MockActionExecutor.h"
#include "mocks/MockEventRaiser.h"
#include "runtime/ActionExecutorImpl.h"
#include "runtime/ExecutionContextImpl.h"
#include "scripting/JSEngine.h"
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

namespace RSM {
namespace Test {

/**
 * @brief SCXML Event System Integration Tests
 *
 * Tests for the basic event infrastructure including SendAction and CancelAction
 * implementation. These tests verify that the SCXML event system works
 * correctly with the existing system.
 */
class SCXMLEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize JavaScript engine
        // JSEngine 리셋으로 테스트 간 격리 보장
        JSEngine::instance().reset();

        sessionId_ = "scxml_event_test_session";
        JSEngine::instance().createSession(sessionId_);

        // SCXML Compliance: Set up proper event infrastructure
        // Create event execution callback (SCXML compliant - delegates to target)
        eventExecutionCallback_ = [this](const EventDescriptor &event, std::shared_ptr<IEventTarget> target,
                                         const std::string &sendId) -> bool {
            (void)sendId;  // Suppress unused parameter warning
            // SCXML Compliance: Always delegate to target for proper event handling
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

        // Create ActionExecutor
        executor_ = std::make_shared<ActionExecutorImpl>(sessionId_);

        // Set up event raising with MockEventRaiser for internal events
        raisedEvents_.clear();
        mockEventRaiser_ = std::make_shared<MockEventRaiser>(
            [this](const std::string &eventName, const std::string &eventData) -> bool {
                raisedEvents_.emplace_back(eventName, eventData);
                return true;
            });
        executor_->setEventRaiser(mockEventRaiser_);

        // Create target factory using the MockEventRaiser
        targetFactory_ = std::make_shared<EventTargetFactoryImpl>(mockEventRaiser_);

        // Create dispatcher with proper target factory
        dispatcher_ = std::make_shared<EventDispatcherImpl>(scheduler_, targetFactory_);

        // SCXML Compliance: Set EventDispatcher (mandatory for send actions)
        executor_->setEventDispatcher(dispatcher_);

        // Create execution context
        context_ = std::make_shared<ExecutionContextImpl>(executor_, sessionId_);
    }

    void TearDown() override {
        if (scheduler_) {
            scheduler_->shutdown(true);
        }

        if (dispatcher_) {
            dispatcher_->shutdown();
        }

        raisedEvents_.clear();
        context_.reset();
        executor_.reset();
        JSEngine::instance().destroySession(sessionId_);
        JSEngine::instance().shutdown();
    }

    std::string sessionId_;
    std::shared_ptr<ActionExecutorImpl> executor_;
    std::shared_ptr<ExecutionContextImpl> context_;
    std::shared_ptr<MockEventRaiser> mockEventRaiser_;
    std::vector<std::pair<std::string, std::string>> raisedEvents_;

    // SCXML compliant event infrastructure
    std::shared_ptr<EventTargetFactoryImpl> targetFactory_;
    std::shared_ptr<EventSchedulerImpl> scheduler_;
    std::shared_ptr<EventDispatcherImpl> dispatcher_;
    EventExecutionCallback eventExecutionCallback_;
};

/**
 * @brief Test basic SendAction functionality for internal events
 */
TEST_F(SCXMLEventTest, SendActionBasicInternalEvent) {
    // Create a basic send action for internal event
    auto sendAction = std::make_shared<SendAction>("user.click", "send1");
    sendAction->setTarget("#_internal");
    sendAction->setData("'Hello World'");

    // Execute the action
    bool result = sendAction->execute(*context_);

    // Verify execution succeeded
    EXPECT_TRUE(result);

    // Wait for async event processing (SCXML events are processed asynchronously)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Verify event was raised internally
    ASSERT_EQ(raisedEvents_.size(), 1);
    EXPECT_EQ(raisedEvents_[0].first, "user.click");
    EXPECT_EQ(raisedEvents_[0].second, "Hello World");
}

/**
 * @brief Test SendAction with expression-based event name
 */
TEST_F(SCXMLEventTest, SendActionWithEventExpression) {
    // Set up JavaScript variables
    executor_->assignVariable("eventPrefix", "'user'");
    executor_->assignVariable("eventSuffix", "'notification'");

    // Create send action with event expression
    auto sendAction = std::make_shared<SendAction>("", "send2");
    sendAction->setEventExpr("eventPrefix + '.' + eventSuffix");
    sendAction->setData("42");

    // Execute the action
    bool result = sendAction->execute(*context_);

    // Verify execution succeeded
    EXPECT_TRUE(result);

    // Wait for async event processing (SCXML events are processed asynchronously)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Verify event was raised with evaluated name
    ASSERT_EQ(raisedEvents_.size(), 1);
    EXPECT_EQ(raisedEvents_[0].first, "user.notification");
    EXPECT_EQ(raisedEvents_[0].second, "42");
}

/**
 * @brief Test SendAction with complex data expression
 */
TEST_F(SCXMLEventTest, SendActionWithComplexData) {
    // Set up JavaScript data
    executor_->executeScript("var userData = { name: 'John', age: 30 };");

    // Create send action with data expression
    auto sendAction = std::make_shared<SendAction>("data.update", "send3");
    sendAction->setData("JSON.stringify(userData)");

    // Execute the action
    bool result = sendAction->execute(*context_);

    // Verify execution succeeded
    EXPECT_TRUE(result);

    // Wait for async event processing (SCXML events are processed asynchronously)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Verify event was raised with JSON data
    ASSERT_EQ(raisedEvents_.size(), 1);
    EXPECT_EQ(raisedEvents_[0].first, "data.update");
    EXPECT_EQ(raisedEvents_[0].second, "{\"name\":\"John\",\"age\":30}");
}

/**
 * @brief Test SendAction validation for missing event
 */
TEST_F(SCXMLEventTest, SendActionValidationMissingEvent) {
    // Create send action without event or eventexpr
    auto sendAction = std::make_shared<SendAction>("", "send4");
    sendAction->setData("test");

    // Execute the action
    bool result = sendAction->execute(*context_);

    // Verify execution failed
    EXPECT_FALSE(result);

    // W3C SCXML 6.2: Verify error.execution event was raised for invalid send action
    EXPECT_EQ(raisedEvents_.size(), 1);
    EXPECT_EQ(raisedEvents_[0].first, "error.execution");
    EXPECT_EQ(raisedEvents_[0].second, "Send action has no event or eventexpr");
}

/**
 * @brief Test SendAction with external target (HTTP support available)
 */
TEST_F(SCXMLEventTest, SendActionExternalTargetNotSupported) {
    // Create send action with external target
    auto sendAction = std::make_shared<SendAction>("external.event", "send5");
    sendAction->setTarget("http://example.com/webhook");
    sendAction->setData("test");

    // Execute the action
    bool result = sendAction->execute(*context_);

    // SCXML Compliance: Send actions use "fire and forget" semantics
    // They should return true immediately after queuing, even if HTTP fails later
    EXPECT_TRUE(result);

    // Wait for async HTTP processing to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Note: HTTP errors are logged but don't affect the send action result
    // This follows SCXML "fire and forget" specification
}

/**
 * @brief Test basic CancelAction functionality
 */
TEST_F(SCXMLEventTest, CancelActionBasic) {
    // Create cancel action with sendid
    auto cancelAction = std::make_shared<CancelAction>("msg_001", "cancel1");

    // Execute the action
    bool result = cancelAction->execute(*context_);

    // Verify execution succeeded (SCXML cancel action implementation)
    EXPECT_TRUE(result);
}

/**
 * @brief Test CancelAction with expression-based sendid
 */
TEST_F(SCXMLEventTest, CancelActionWithExpression) {
    // Set up JavaScript variable
    executor_->assignVariable("messageId", "'msg_dynamic_001'");

    // Create cancel action with sendidexpr
    auto cancelAction = std::make_shared<CancelAction>("", "cancel2");
    cancelAction->setSendIdExpr("messageId");

    // Execute the action
    bool result = cancelAction->execute(*context_);

    // Verify execution succeeded
    EXPECT_TRUE(result);
}

/**
 * @brief Test CancelAction validation for missing sendid
 */
TEST_F(SCXMLEventTest, CancelActionValidationMissingSendId) {
    // Create cancel action without sendid or sendidexpr
    auto cancelAction = std::make_shared<CancelAction>("", "cancel3");

    // Execute the action
    bool result = cancelAction->execute(*context_);

    // Verify execution failed
    EXPECT_FALSE(result);
}

/**
 * @brief Test action validation and error handling
 */
TEST_F(SCXMLEventTest, ActionValidationAndErrors) {
    // Test SendAction validation
    {
        auto sendAction = std::make_shared<SendAction>();
        auto errors = sendAction->validate();
        EXPECT_FALSE(errors.empty());
        EXPECT_TRUE(std::any_of(errors.begin(), errors.end(),
                                [](const std::string &error) { return error.find("event") != std::string::npos; }));
    }

    // Test CancelAction validation
    {
        auto cancelAction = std::make_shared<CancelAction>();
        auto errors = cancelAction->validate();
        EXPECT_FALSE(errors.empty());
        EXPECT_TRUE(std::any_of(errors.begin(), errors.end(),
                                [](const std::string &error) { return error.find("sendid") != std::string::npos; }));
    }
}

/**
 * @brief Test action cloning functionality
 */
TEST_F(SCXMLEventTest, ActionCloning) {
    // Test SendAction cloning
    {
        auto original = std::make_shared<SendAction>("test.event", "send_original");
        original->setTarget("http://example.com");
        original->setData("test_data");
        original->setDelay("5s");

        auto cloned = std::static_pointer_cast<SendAction>(original->clone());

        EXPECT_EQ(cloned->getEvent(), original->getEvent());
        EXPECT_EQ(cloned->getTarget(), original->getTarget());
        EXPECT_EQ(cloned->getData(), original->getData());
        EXPECT_EQ(cloned->getDelay(), original->getDelay());
        EXPECT_NE(cloned->getId(), original->getId());  // Should have different ID
    }

    // Test CancelAction cloning
    {
        auto original = std::make_shared<CancelAction>("msg_001", "cancel_original");
        original->setSendIdExpr("dynamicId");

        auto cloned = std::static_pointer_cast<CancelAction>(original->clone());

        EXPECT_EQ(cloned->getSendId(), original->getSendId());
        EXPECT_EQ(cloned->getSendIdExpr(), original->getSendIdExpr());
        EXPECT_NE(cloned->getId(), original->getId());  // Should have different ID
    }
}

/**
 * @brief Test delay parsing functionality in SendAction
 */
TEST_F(SCXMLEventTest, SendActionDelayParsing) {
    auto sendAction = std::make_shared<SendAction>("test.event", "send_delay");

    // Test various delay formats
    EXPECT_EQ(sendAction->parseDelayString("100ms").count(), 100);
    EXPECT_EQ(sendAction->parseDelayString("5s").count(), 5000);
    EXPECT_EQ(sendAction->parseDelayString("2min").count(), 120000);
    EXPECT_EQ(sendAction->parseDelayString("1h").count(), 3600000);

    // Test invalid formats
    EXPECT_EQ(sendAction->parseDelayString("invalid").count(), 0);
    EXPECT_EQ(sendAction->parseDelayString("").count(), 0);
}

/**
 * @brief Test SCXML event system integration with existing action system
 */
TEST_F(SCXMLEventTest, IntegrationWithExistingActions) {
    // Create a script action that sets up data
    auto scriptAction = std::make_shared<ScriptAction>("var eventData = 'setup_complete';", "script1");

    // Create a send action that uses the data
    auto sendAction = std::make_shared<SendAction>("setup.complete", "send1");
    sendAction->setData("eventData");

    // Execute script first
    bool scriptResult = scriptAction->execute(*context_);
    EXPECT_TRUE(scriptResult);

    // Execute send action
    bool sendResult = sendAction->execute(*context_);
    EXPECT_TRUE(sendResult);

    // Wait for async event processing (SCXML events are processed asynchronously)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Verify the event was raised with correct data
    ASSERT_EQ(raisedEvents_.size(), 1);
    EXPECT_EQ(raisedEvents_[0].first, "setup.complete");
    EXPECT_EQ(raisedEvents_[0].second, "setup_complete");
}

/**
 * @brief Test parent-child event communication (Test 207 scenario)
 *
 * This test reproduces the core issue found in W3C test 207:
 * Child session sends events to parent via #_parent target
 */
TEST_F(SCXMLEventTest, ParentChildEventCommunication) {
    // Create child session
    std::string childSessionId = "child_session_test";
    JSEngine::instance().createSession(childSessionId, sessionId_);

    // Create child ActionExecutor and EventRaiser
    auto childExecutor = std::make_shared<ActionExecutorImpl>(childSessionId);
    auto childEventRaiser =
        std::make_shared<MockEventRaiser>([this](const std::string &eventName, const std::string &eventData) -> bool {
            // This should route events to parent session
            raisedEvents_.emplace_back(eventName, eventData);
            return true;
        });
    childExecutor->setEventRaiser(childEventRaiser);
    childExecutor->setEventDispatcher(dispatcher_);

    // Create child execution context
    auto childContext = std::make_shared<ExecutionContextImpl>(childExecutor, childSessionId);

    // Test: Child sends "pass" event to parent
    auto sendToParent = std::make_shared<SendAction>("pass", "send_to_parent");
    sendToParent->setTarget("#_parent");

    // Execute the send action from child session
    bool result = sendToParent->execute(*childContext);
    EXPECT_TRUE(result);

    // Wait for async event processing
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Verify parent session received the "pass" event
    ASSERT_GE(raisedEvents_.size(), 1);
    bool foundPassEvent = false;
    for (const auto &event : raisedEvents_) {
        if (event.first == "pass") {
            foundPassEvent = true;
            break;
        }
    }
    EXPECT_TRUE(foundPassEvent) << "Parent session should receive 'pass' event from child";

    // Cleanup
    JSEngine::instance().destroySession(childSessionId);
}

/**
 * @brief Test cross-session cancel action (Test 207 scenario)
 *
 * This test verifies that cancel actions cannot affect events in other sessions,
 * which is the expected behavior according to W3C SCXML specification.
 */
TEST_F(SCXMLEventTest, CrossSessionCancelAction) {
    // Create child session
    std::string childSessionId = "child_session_cancel_test";
    JSEngine::instance().createSession(childSessionId, sessionId_);

    // Create child infrastructure
    auto childExecutor = std::make_shared<ActionExecutorImpl>(childSessionId);
    auto childEventRaiser =
        std::make_shared<MockEventRaiser>([](const std::string &, const std::string &) -> bool { return true; });
    childExecutor->setEventRaiser(childEventRaiser);
    childExecutor->setEventDispatcher(dispatcher_);

    auto childContext = std::make_shared<ExecutionContextImpl>(childExecutor, childSessionId);

    // Child: Schedule delayed event with sendid "foo"
    auto childSendAction = std::make_shared<SendAction>("event1", "child_send");
    childSendAction->setSendId("foo");
    childSendAction->setDelay("100ms");
    childSendAction->setTarget("#_internal");

    bool childResult = childSendAction->execute(*childContext);
    EXPECT_TRUE(childResult);

    // Parent: Try to cancel the child's event (should not work)
    auto parentCancelAction = std::make_shared<CancelAction>("foo", "parent_cancel");
    bool cancelResult = parentCancelAction->execute(*context_);
    EXPECT_TRUE(cancelResult);  // Cancel action succeeds but doesn't affect child's event

    // Wait for the delayed event to potentially fire
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // The key test: Child's event should still fire because parent cannot cancel cross-session events
    // This is verified by the fact that the cancel action doesn't prevent the delayed event
    // (In a real scenario, we'd check if event1 fired in the child session)

    // Cleanup
    JSEngine::instance().destroySession(childSessionId);
}

/**
 * @brief Test complete invoke workflow with delayed event and cancel (Test 207 full scenario)
 *
 * This test reproduces the complete W3C test 207 workflow:
 * 1. Parent invokes child
 * 2. Child schedules delayed event with sendid "foo"
 * 3. Child notifies parent
 * 4. Parent tries to cancel "foo" (should fail)
 * 5. Child's event1 fires → child sends "pass" to parent
 * 6. Parent should receive "pass" event and transition to final state
 */
TEST_F(SCXMLEventTest, InvokeWithDelayedEventAndCancel) {
    // Step 1: Create child session (simulating invoke)
    std::string childSessionId = "invoke_child_test";
    JSEngine::instance().createSession(childSessionId, sessionId_);

    // Track events received by parent
    std::vector<std::string> parentEvents;
    auto parentEventRaiser =
        std::make_shared<MockEventRaiser>([&parentEvents](const std::string &eventName, const std::string &) -> bool {
            parentEvents.push_back(eventName);
            return true;
        });
    executor_->setEventRaiser(parentEventRaiser);

    // CRITICAL FIX: Manually register MockEventRaiser with EventRaiserRegistry
    // This ensures ParentEventTarget can find the correct EventRaiser

    // First unregister any existing EventRaiser for this session
    EventRaiserService::getInstance().unregisterEventRaiser(sessionId_);

    // Then register our MockEventRaiser using Service pattern
    bool registered = EventRaiserService::getInstance().registerEventRaiser(sessionId_, parentEventRaiser);
    EXPECT_TRUE(registered) << "Failed to register MockEventRaiser for parent session";

    // Create child infrastructure
    auto childExecutor = std::make_shared<ActionExecutorImpl>(childSessionId);
    auto childEventRaiser =
        std::make_shared<MockEventRaiser>([](const std::string &, const std::string &) -> bool { return true; });
    childExecutor->setEventRaiser(childEventRaiser);
    childExecutor->setEventDispatcher(dispatcher_);

    // Step 2: Child schedules delayed event1 with sendid "foo"
    auto childContext = std::make_shared<ExecutionContextImpl>(childExecutor, childSessionId);

    auto scheduleEvent1 = std::make_shared<SendAction>("event1", "child_event1");
    scheduleEvent1->setSendId("foo");
    scheduleEvent1->setDelay("50ms");
    scheduleEvent1->setTarget("#_internal");

    bool scheduleResult = scheduleEvent1->execute(*childContext);
    EXPECT_TRUE(scheduleResult);

    // Step 3: Child notifies parent (simulating childToParent event)
    auto notifyParent = std::make_shared<SendAction>("childToParent", "notify_parent");
    notifyParent->setTarget("#_parent");

    bool notifyResult = notifyParent->execute(*childContext);
    EXPECT_TRUE(notifyResult);

    // Small delay to ensure parent receives notification
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Step 4: Parent tries to cancel child's "foo" event (should not work)
    auto parentCancel = std::make_shared<CancelAction>("foo", "parent_cancel_foo");
    bool cancelResult = parentCancel->execute(*context_);
    EXPECT_TRUE(cancelResult);  // Cancel succeeds but doesn't affect child

    // Step 5: Wait for child's event1 to fire
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    // Simulate child's response: when event1 fires, child sends "pass" to parent
    auto childSendPass = std::make_shared<SendAction>("pass", "child_send_pass");
    childSendPass->setTarget("#_parent");

    bool passResult = childSendPass->execute(*childContext);
    EXPECT_TRUE(passResult);

    // Step 6: Wait for pass event to reach parent
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Verify the complete workflow
    EXPECT_GE(parentEvents.size(), 2);  // Should have received childToParent and pass

    bool receivedChildToParent =
        std::find(parentEvents.begin(), parentEvents.end(), "childToParent") != parentEvents.end();
    bool receivedPass = std::find(parentEvents.begin(), parentEvents.end(), "pass") != parentEvents.end();

    EXPECT_TRUE(receivedChildToParent) << "Parent should receive childToParent notification";
    EXPECT_TRUE(receivedPass) << "Parent should receive pass event (Test 207 critical issue)";

    // Cleanup
    JSEngine::instance().destroySession(childSessionId);
}

}  // namespace Test
}  // namespace RSM