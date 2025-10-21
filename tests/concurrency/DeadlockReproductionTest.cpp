#include <atomic>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <thread>

#include "actions/RaiseAction.h"
#include "actions/SendAction.h"
#include "common/Logger.h"
#include "events/EventDispatcherImpl.h"
#include "events/EventSchedulerImpl.h"
#include "events/EventTargetFactoryImpl.h"
#include "mocks/MockEventRaiser.h"
#include "runtime/ActionExecutorImpl.h"
#include "runtime/ExecutionContextImpl.h"
#include "scripting/JSEngine.h"

namespace RSM {

/**
 * @brief Test class to reproduce the specific deadlock scenario
 *
 * Deadlock scenario:
 * 1. Main thread: ActionExecutor.executeSendAction() -> JSEngine.evaluateExpression() -> locks queueMutex_
 * 2. Main thread: eventDispatcher.sendEvent() -> EventScheduler.scheduleEvent() -> locks schedulerMutex_
 * 3. Timer thread: EventScheduler.timerThreadMain() -> locks schedulerMutex_ -> executes callback
 * 4. Timer thread: callback -> InternalEventTarget.send() -> ActionExecutor.raiseEvent() -> JSEngine -> tries to lock
 * queueMutex_
 * 5. DEADLOCK: Main thread holds queueMutex_ waiting for schedulerMutex_, Timer thread holds schedulerMutex_ waiting
 * for queueMutex_
 */
class DeadlockReproductionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure test isolation with JSEngine reset
        auto &jsEngine = JSEngine::instance();
        jsEngine.reset();

        jsEngine.createSession("deadlock_test_session");

        // Create deadlock-prone callback that calls back to JSEngine
        deadlockCallback_ = [this](const EventDescriptor &event, std::shared_ptr<IEventTarget> /* target */,
                                   const std::string & /* sendId */) -> bool {
            // This will try to acquire JSEngine mutex from timer thread
            Logger::debug("DeadlockTest: Callback executing on timer thread");

            try {
                // Simulate what InternalEventTarget does - call back to ActionExecutor
                if (actionExecutor_) {
                    // This should cause deadlock if JSEngine mutex is already held by main thread
                    RaiseAction raiseAction(event.eventName);
                    raiseAction.setData(event.data);
                    bool result = actionExecutor_->executeRaiseAction(raiseAction);
                    LOG_DEBUG("DeadlockTest: executeRaiseAction result: {}", result);
                    return result;
                }
                return false;
            } catch (const std::exception &e) {
                LOG_ERROR("DeadlockTest: Exception in callback: {}", e.what());
                return false;
            }
        };

        // Create components that can deadlock
        scheduler_ = std::make_shared<EventSchedulerImpl>(deadlockCallback_);

        // Create MockEventRaiser for target factory
        auto mockEventRaiser =
            std::make_shared<RSM::Test::MockEventRaiser>([](const std::string &, const std::string &) -> bool {
                return true;  // Always succeed for deadlock testing
            });

        // Create ActionExecutor for target factory
        auto tempActionExecutor = std::make_shared<ActionExecutorImpl>("deadlock_test_session");
        tempActionExecutor->setEventRaiser(mockEventRaiser);
        targetFactory_ = std::make_shared<EventTargetFactoryImpl>(mockEventRaiser);

        // Create dispatcher
        dispatcher_ = std::make_shared<EventDispatcherImpl>(scheduler_, targetFactory_);

        // Create ActionExecutor that will be used in main thread (potential deadlock source)
        actionExecutor_ = std::make_shared<ActionExecutorImpl>("deadlock_test_session", dispatcher_);
        actionExecutor_->setEventRaiser(mockEventRaiser);

        deadlockDetected_.store(false);
    }

    void TearDown() override {
        if (scheduler_) {
            scheduler_->shutdown(true);
        }
        if (dispatcher_) {
            dispatcher_->shutdown();
        }

        auto &jsEngine = JSEngine::instance();
        jsEngine.destroySession("deadlock_test_session");
    }

protected:
    std::shared_ptr<ActionExecutorImpl> actionExecutor_;
    std::shared_ptr<EventSchedulerImpl> scheduler_;
    std::shared_ptr<EventDispatcherImpl> dispatcher_;
    std::shared_ptr<EventTargetFactoryImpl> targetFactory_;
    EventExecutionCallback deadlockCallback_;
    std::atomic<bool> deadlockDetected_;
};

/**
 * @brief Test that reproduces the exact deadlock scenario
 */
TEST_F(DeadlockReproductionTest, ReproduceJSEngineEventSchedulerDeadlock) {
    // This test should hang if the deadlock exists, pass if it's fixed

    std::atomic<bool> testCompleted{false};
    std::atomic<bool> sendActionStarted{false};

    // Start a watchdog thread to detect if we're hanging
    auto watchdog = std::async(std::launch::async, [&]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!testCompleted.load()) {
            deadlockDetected_.store(true);
            Logger::error("DeadlockTest: DEADLOCK DETECTED - test hung for 5+ seconds");
        }
    });

    // Main thread: Execute send action that triggers the deadlock scenario
    auto mainTask = std::async(std::launch::async, [&]() {
        try {
            Logger::debug("DeadlockTest: Starting send action execution");
            sendActionStarted.store(true);

            // Create send action with expression that requires JSEngine evaluation
            SendAction sendAction("deadlock.test.event");
            sendAction.setTarget("#_internal");
            sendAction.setData("'test data ' + 'concatenation'");  // This forces JSEngine evaluation
            sendAction.setDelay("0ms");                            // Immediate execution to trigger callback quickly

            // Create execution context
            auto sharedExecutor = std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
            ExecutionContextImpl context(sharedExecutor, "deadlock_test_session");

            // This should cause deadlock:
            // 1. evaluateExpression() locks JSEngine queueMutex_
            // 2. eventDispatcher_->sendEvent() triggers timer thread
            // 3. Timer thread callback tries to lock JSEngine queueMutex_ -> DEADLOCK
            bool success = sendAction.execute(context);

            LOG_DEBUG("DeadlockTest: Send action completed successfully: {}", success);
            testCompleted.store(true);

            return success;

        } catch (const std::exception &e) {
            LOG_ERROR("DeadlockTest: Exception in main task: {}", e.what());
            testCompleted.store(true);
            return false;
        }
    });

    // Wait for main task to complete or timeout
    auto status = mainTask.wait_for(std::chrono::seconds(8));

    if (status == std::future_status::timeout) {
        Logger::error("DeadlockTest: Main task timed out - DEADLOCK CONFIRMED");
        deadlockDetected_.store(true);
    } else {
        try {
            bool result = mainTask.get();
            LOG_DEBUG("DeadlockTest: Main task completed with result: {}", result);
        } catch (...) {
            Logger::error("DeadlockTest: Main task threw exception");
        }
    }

    testCompleted.store(true);

    // Verify results
    if (deadlockDetected_.load()) {
        FAIL() << "DEADLOCK DETECTED: JSEngine mutex vs EventScheduler mutex deadlock reproduced";
    } else {
        SUCCEED() << "No deadlock detected - test completed successfully";
    }
}

/**
 * @brief Test JSEngine mutex behavior in isolation
 */
TEST_F(DeadlockReproductionTest, JSEngineMutexBehavior) {
    auto &jsEngine = JSEngine::instance();

    // Test if JSEngine can handle nested calls from same thread
    std::atomic<bool> nestedCallCompleted{false};

    auto task = std::async(std::launch::async, [&]() {
        try {
            // First JSEngine call
            auto result1Future = jsEngine.evaluateExpression("deadlock_test_session", "1 + 1");
            auto result1 = result1Future.get();
            if (result1.isSuccess()) {
                LOG_DEBUG("DeadlockTest: First evaluation result: {}", result1.getValue<double>());
            }

            // Second JSEngine call from same thread (should work if mutex is recursive)
            auto result2Future = jsEngine.evaluateExpression("deadlock_test_session", "2 + 2");
            auto result2 = result2Future.get();
            if (result2.isSuccess()) {
                LOG_DEBUG("DeadlockTest: Second evaluation result: {}", result2.getValue<double>());
            }

            nestedCallCompleted.store(true);
            return true;

        } catch (const std::exception &e) {
            LOG_ERROR("DeadlockTest: JSEngine nested call failed: {}", e.what());
            return false;
        }
    });

    auto status = task.wait_for(std::chrono::seconds(3));

    if (status == std::future_status::timeout) {
        FAIL() << "JSEngine mutex test timed out - possible mutex issue";
    } else {
        bool result = task.get();
        EXPECT_TRUE(result) << "JSEngine nested calls should work";
        EXPECT_TRUE(nestedCallCompleted.load()) << "Nested call should complete";
    }
}

}  // namespace RSM