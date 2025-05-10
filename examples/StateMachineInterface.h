#pragma once

#include <string>
#include <functional>
#include <memory>
#include "Context.h"

/**
 * @brief 상태 머신 인터페이스
 * SCXML에서 생성된 상태 머신 인터페이스
 */
class StateMachineInterface
{
public:
    virtual ~StateMachineInterface() = default;

    // 상태 콜백 메서드 (개발자가 구현해야 함)
    virtual void onEnterTest1() {}
    virtual void onExitTest1() {}
    virtual void onEnterTest1Sub1() {}
    virtual void onExitTest1Sub1() {}
    virtual void onEnterTest1Sub2() {}
    virtual void onEnterTest2() {}
    virtual void onExitTest2() {}
    virtual void onEnterTest2Sub1() {}
    virtual void onExitTest2Sub1() {}
    virtual void onEnterTest2Sub2() {}
    virtual void onEnterTest3() {}
    virtual void onExitTest3() {}
    virtual void onEnterTest3Sub1() {}
    virtual void onExitTest3Sub1() {}
    virtual void onEnterTest4() {}
    virtual void onExitTest4() {}
    virtual void onEnterTest4Sub1() {}
    virtual void onExitTest4Sub1() {}
    virtual void onEnterTest5() {}
    virtual void onExitTest5() {}
    virtual void onEnterTest5P() {}
    virtual void onExitTest5P() {}
    virtual void onEnterTest5PSub1() {}
    virtual void onExitTest5PSub1() {}
    virtual void onEnterTest5PSub2() {}
    virtual void onExitTest5PSub2() {}
    virtual void onEnterTest6() {}
    virtual void onExitTest6() {}
    virtual void onEnterDone() {}

    // 외부 액션 메서드 (개발자가 구현해야 함)
    virtual void initializeTest2Data() {}
    virtual void cleanupTest2Data() {}
    virtual void startTimer(int /* delayMs */) {}
    virtual void handleError(const EventContext & /* context */) {}

    // 이벤트 발생 메서드
    virtual void fireEvent1() = 0;
    virtual void fireEvent2() = 0;
    virtual void fireTimerEvent() = 0;
    virtual void fireSuccessEvent() = 0;
    virtual void fireErrorEvent(const std::string &errorType, const EventContext &context) = 0;

    // 상태 조회 메서드
    virtual bool isInState(const std::string &stateId) const = 0;
    virtual std::string getCurrentState() const = 0;

    // 가드 조건 등록 메서드
    virtual void registerGuard(const std::string &guardId, std::shared_ptr<Guard> guard) = 0;
    virtual void registerGuard(const std::string &guardId, std::function<bool(const Context &)> guardFunc) = 0;

    // 컨텍스트 접근자
    virtual Context &getContext() = 0;
    virtual const Context &getContext() const = 0;

    // 시작/중지 메서드
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

/**
 * @brief 상태 머신 팩토리 인터페이스
 * 상태 머신 구현체를 생성하는 팩토리
 */
class StateMachineFactory
{
public:
    virtual ~StateMachineFactory() = default;

    // 상태 머신 인스턴스 생성
    virtual std::unique_ptr<StateMachineInterface> createStateMachine() = 0;
};
