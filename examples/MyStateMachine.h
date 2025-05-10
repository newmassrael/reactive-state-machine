#pragma once

#include "StateMachineImpl.h"
#include <iostream>

/**
 * @brief 사용자 정의 가드 조건 구현
 * counter 임계값 검사
 */
class CounterThresholdGuard : public Guard
{
private:
    int threshold_;

public:
    explicit CounterThresholdGuard(int threshold = 10) : threshold_(threshold) {}

    bool evaluate(const Context &context) const override
    {
        return context.counter.get() >= threshold_;
    }
};

/**
 * @brief 사용자 정의 가드 조건 구현
 * 관리자 권한 및 활성 상태 검사
 */
class AdminActiveGuard : public Guard
{
public:
    bool evaluate(const Context &context) const override
    {
        return context.currentUser.get() == "admin" && context.isActive.get();
    }
};

/**
 * @brief 사용자 정의 가드 조건 구현
 * 상태가 "ready"인지 검사
 */
class StatusReadyGuard : public Guard
{
public:
    bool evaluate(const Context &context) const override
    {
        return context.status.get() == "ready";
    }
};

/**
 * @brief 상태 머신 구현 클래스
 * 개발자가 구현해야 하는 비즈니스 로직 포함
 */
class MyStateMachine : public StateMachineImpl
{
public:
    MyStateMachine()
    {
        // 가드 조건 등록
        registerGuard("counterThresholdGuard", std::make_shared<CounterThresholdGuard>(10));
        registerGuard("adminActiveGuard", std::make_shared<AdminActiveGuard>());
        registerGuard("statusReadyGuard", std::make_shared<StatusReadyGuard>());
    }

    // 상태 진입/종료 콜백 구현
    void onEnterTest1() override
    {
        std::cout << "Entering Test1 state" << std::endl;
        // 초기화 로직
        getContext().counter.set(0);
    }

    void onExitTest1() override
    {
        std::cout << "Exiting Test1 state" << std::endl;
        // 정리 로직
    }

    void onEnterTest1Sub1() override
    {
        std::cout << "Entering Test1Sub1 state" << std::endl;
    }

    void onExitTest1Sub1() override
    {
        std::cout << "Exiting Test1Sub1 state" << std::endl;
    }

    void onEnterTest1Sub2() override
    {
        std::cout << "Entering Test1Sub2 state (final)" << std::endl;
        // Test1.done 이벤트 자동 발생은 StateMachineImpl에서 처리
    }

    void onEnterTest2() override
    {
        std::cout << "Entering Test2 state" << std::endl;
    }

    void onExitTest2() override
    {
        std::cout << "Exiting Test2 state" << std::endl;
    }

    void onEnterTest2Sub1() override
    {
        std::cout << "Entering Test2Sub1 state" << std::endl;
    }

    void onExitTest2Sub1() override
    {
        std::cout << "Exiting Test2Sub1 state" << std::endl;
    }

    void onEnterTest2Sub2() override
    {
        std::cout << "Entering Test2Sub2 state (final)" << std::endl;
        // done.state.Test2 이벤트 자동 발생은 StateMachineImpl에서 처리
    }

    void onEnterTest3() override
    {
        std::cout << "Entering Test3 state" << std::endl;
    }

    void onExitTest3() override
    {
        std::cout << "Exiting Test3 state" << std::endl;
    }

    void onEnterTest3Sub1() override
    {
        std::cout << "Entering Test3Sub1 state" << std::endl;
    }

    void onExitTest3Sub1() override
    {
        std::cout << "Exiting Test3Sub1 state" << std::endl;
    }

    // 액션 구현
    void initializeTest2Data() override
    {
        std::cout << "Initializing Test2 data" << std::endl;
        // Test2 데이터 초기화
    }

    void cleanupTest2Data() override
    {
        std::cout << "Cleaning up Test2 data" << std::endl;
        // Test2 데이터 정리
    }

    void startTimer(int delayMs) override
    {
        std::cout << "Starting timer for " << delayMs << "ms" << std::endl;

        // 실제 구현에서는 별도 타이머 스레드 또는 라이브러리 사용
        // 여기서는 간단한 예시로 스레드 사용
        std::thread([this, delayMs]()
                    {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            std::cout << "Timer fired after " << delayMs << "ms" << std::endl;
            fireTimerEvent(); })
            .detach();
    }

    void handleError(const EventContext &context) override
    {
        std::cout << "Handling error event" << std::endl;
        // 오류 처리 로직
        if (context.hasValue("errorCode"))
        {
            try
            {
                int errorCode = context.getValue<int>("errorCode");
                std::cout << "Error code: " << errorCode << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cout << "Failed to get error code: " << e.what() << std::endl;
            }
        }
    }
};

/**
 * @brief 상태 머신 팩토리 구현
 * 상태 머신 인스턴스 생성
 */
class MyStateMachineFactory : public StateMachineFactory
{
public:
    std::unique_ptr<StateMachineInterface> createStateMachine() override
    {
        return std::make_unique<MyStateMachine>();
    }
};
