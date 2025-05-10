#include <gtest/gtest.h>
#include "MyStateMachine.h"
#include "Logger.h"
#include <thread>
#include <chrono>

// 테스트 헬퍼: 상태 전환 기다리기
void waitForStateTransition(int milliseconds = 100)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// 기본 상태 머신 초기화 테스트
TEST(StateMachineTest, Initialization)
{
    Logger::info("===== Starting Initialization Test =====");

    MyStateMachineFactory factory;
    auto stateMachine = factory.createStateMachine();

    // 시작 전 초기 상태 확인
    EXPECT_FALSE(stateMachine->isRunning());

    // 시작 후 상태 확인
    stateMachine->start();
    EXPECT_TRUE(stateMachine->isRunning());
    EXPECT_TRUE(stateMachine->isInState("Test1"));
    EXPECT_TRUE(stateMachine->isInState("Test1Sub1"));

    // 종료
    stateMachine->stop();
    EXPECT_FALSE(stateMachine->isRunning());

    Logger::info("===== Initialization Test Completed =====");
}

// 이벤트 기반 전환 테스트
TEST(StateMachineTest, EventBasedTransition)
{
    Logger::info("===== Starting EventBasedTransition Test =====");

    MyStateMachineFactory factory;
    auto stateMachine = factory.createStateMachine();
    stateMachine->start();

    // Test1Sub1에서 시작
    EXPECT_TRUE(stateMachine->isInState("Test1Sub1"));
    Logger::info("Initial state: " + stateMachine->getCurrentState());

    // Event1 발생 시 Test1Sub2로 전환
    Logger::info("Firing Event1");
    stateMachine->fireEvent1();
    waitForStateTransition();

    Logger::info("Current state after Event1: " + stateMachine->getCurrentState());

    // Test1Sub2는 final 상태이므로 자동으로 Test2로 전환
    EXPECT_TRUE(stateMachine->isInState("Test2"));
    EXPECT_TRUE(stateMachine->isInState("Test2Sub1"));

    stateMachine->stop();

    Logger::info("===== EventBasedTransition Test Completed =====");
}

// 가드 조건 테스트
TEST(StateMachineTest, GuardConditionTest)
{
    Logger::info("===== Starting GuardConditionTest =====");

    MyStateMachineFactory factory;
    auto stateMachineInterface = factory.createStateMachine();
    auto &stateMachine = static_cast<MyStateMachine &>(*stateMachineInterface);
    stateMachine.start();

    // 현재 상태가 Test1Sub1인지 확인
    EXPECT_TRUE(stateMachine.isInState("Test1Sub1"));
    Logger::info("Initial state: " + stateMachine.getCurrentState());

    // counter가 9이면 전환 안됨
    Logger::info("Setting counter to 9");
    stateMachine.getContext().counter.set(9);
    waitForStateTransition();
    Logger::info("State after counter=9: " + stateMachine.getCurrentState());
    EXPECT_TRUE(stateMachine.isInState("Test1Sub1"));

    // counter가 10이면 전환됨
    Logger::info("Setting counter to 10");
    stateMachine.getContext().counter.set(10);
    waitForStateTransition();

    Logger::info("State after counter=10: " + stateMachine.getCurrentState());

    // 최종적으로 Test2로 전환 (Test1Sub2는 final 상태)
    EXPECT_TRUE(stateMachine.isInState("Test2"));
    EXPECT_TRUE(stateMachine.isInState("Test2Sub1"));

    stateMachine.stop();

    Logger::info("===== GuardConditionTest Completed =====");
}

// 타이머 이벤트 테스트
TEST(StateMachineTest, TimerEventTest)
{
    Logger::info("===== Starting TimerEventTest =====");

    // 별도의 테스트 클래스 정의
    class TestStateMachine : public StateMachineImpl
    {
    public:
        TestStateMachine()
        {
            Logger::info("TestStateMachine constructor");
            // 가드 조건 등록
            registerGuard("counterThresholdGuard", std::make_shared<CounterThresholdGuard>(10));
            registerGuard("adminActiveGuard", std::make_shared<AdminActiveGuard>());
            registerGuard("statusReadyGuard", std::make_shared<StatusReadyGuard>());
        }

        // 타이머 오버라이드: 실제 지연 대신 즉시 이벤트 발생
        void startTimer(int /* delayMs */) override
        {
            Logger::info("Test: Firing timer event immediately");
            // 약간의 지연 추가 (이벤트 처리 시간 확보)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            fireTimerEvent();
            Logger::info("Timer event fired");
        }
    };

    try
    {
        // 테스트 인스턴스 생성 및 시작
        TestStateMachine testMachine;
        Logger::info("Starting test machine");
        testMachine.start();

        // Test1Sub1에서 시작하는지 확인
        EXPECT_TRUE(testMachine.isInState("Test1Sub1"));
        Logger::info("Initial state: " + testMachine.getCurrentState());

        // Event1 발생으로 Test1Sub1 -> Test1Sub2 -> Test2로 전환
        Logger::info("Firing Event1");
        testMachine.fireEvent1();
        waitForStateTransition();
        Logger::info("State after Event1: " + testMachine.getCurrentState());

        // Event2 발생으로 Test2Sub1 -> Test2Sub2 -> Test3로 전환
        Logger::info("Firing Event2");
        testMachine.fireEvent2();
        waitForStateTransition();
        Logger::info("State after Event2: " + testMachine.getCurrentState());

        // Test3Sub1에서 타이머가 시작되고 Test4로 전환
        Logger::info("Waiting for timer event processing");
        waitForStateTransition(200); // 좀 더 긴 대기 시간
        Logger::info("State after timer: " + testMachine.getCurrentState());

        // 최종 상태 확인 (Test4 또는 Test5)
        bool validState = testMachine.isInState("Test4") || testMachine.isInState("Test5");
        EXPECT_TRUE(validState);

        // 테스트 종료 전 상태 머신 정지
        Logger::info("Stopping state machine");
        testMachine.stop();
        Logger::info("State machine stopped");
    }
    catch (const std::exception &e)
    {
        Logger::error("Exception in TimerEventTest: " + std::string(e.what()));
        FAIL() << "Exception: " << e.what();
    }

    Logger::info("===== TimerEventTest Completed =====");
}

int main(int argc, char **argv)
{
    Logger::info("Starting GoogleTest");
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    Logger::info("GoogleTest completed with result: " + std::to_string(result));
    return result;
}
