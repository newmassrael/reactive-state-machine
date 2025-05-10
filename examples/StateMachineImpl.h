#pragma once

#include "StateMachineInterface.h"
#include "Context.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <thread>

/**
 * @brief 상태 머신 기본 구현 클래스
 * SCXML에서 생성된 상태 전환 로직 구현 - 데드락 문제 해결 (scoped_lock 방식)
 */
class StateMachineImpl : public StateMachineInterface
{
public:
    StateMachineImpl();
    ~StateMachineImpl() override;

    // StateMachineInterface 구현
    void fireEvent1() override;
    void fireEvent2() override;
    void fireTimerEvent() override;
    void fireSuccessEvent() override;
    void fireErrorEvent(const std::string &errorType, const EventContext &context) override;

    bool isInState(const std::string &stateId) const override;
    std::string getCurrentState() const override;

    void registerGuard(const std::string &guardId, std::shared_ptr<Guard> guard) override;
    void registerGuard(const std::string &guardId, std::function<bool(const Context &)> guardFunc) override;

    Context &getContext() override { return context_; }
    const Context &getContext() const override { return context_; }

    void start() override;
    void stop() override;
    bool isRunning() const override { return running_; }

private:
    // 상태 식별자 열거형
    enum class State
    {
        None, // 전환이 필요 없음을 표시하는 새 상태 추가
        Main,
        Test1,
        Test1Sub1,
        Test1Sub2,
        Test2,
        Test2Sub1,
        Test2Sub2,
        Test3,
        Test3Sub1,
        Test4,
        Test4Sub1,
        Test5,
        Test5P,
        Test5PSub1,
        Test5PSub1Final,
        Test5PSub2,
        Test5PSub2Final,
        Test6,
        Done
    };

    // 이벤트 구조체
    struct Event
    {
        std::string name;
        EventContext context;
    };

    // 가드 조건 래퍼 클래스
    class GuardWrapper : public Guard
    {
    public:
        GuardWrapper(std::function<bool(const Context &)> func) : func_(std::move(func)) {}
        bool evaluate(const Context &context) const override { return func_(context); }

    private:
        std::function<bool(const Context &)> func_;
    };

    // 상태 전환 처리 - 데드락 방지 버전
    void transitionTo(State targetState);

    // 이벤트 기반 타겟 상태 결정 함수 - 실제 전환과 분리
    State determineTargetState(const Event &event);

    // 상태 진입/종료 처리
    void enterState(State state);
    void exitState(State state);

    // 이벤트 처리
    void processEvent(const Event &event);
    void processEventQueue();

    // 가드 조건 평가
    bool evaluateGuard(const std::string &guardId, const Context &context) const;

    // 반응형 가드 설정
    void setupReactiveGuards();

    // 상태 간의 계층 관계를 확인하는 메서드
    bool isDescendantOf(State descendant, State ancestor) const;

    // 상태의 부모 상태를 반환하는 메서드
    State getParentState(State state) const;

    // 상태가 특정 이벤트를 처리할 수 있는지 확인하는 메서드
    bool canHandleEvent(State state, const std::string &eventName) const;

    // 현재 상태 및 활성 상태들
    State currentState_{State::Main};
    std::unordered_map<State, bool> activeStates_;
    mutable std::mutex stateMutex_;

    // 컨텍스트
    Context context_;
    std::vector<boost::signals2::connection> contextConnections_;
    mutable std::mutex contextMutex_;

    // 가드 조건
    std::unordered_map<std::string, std::shared_ptr<Guard>> guards_;
    mutable std::mutex guardsMutex_;

    // 이벤트 큐 처리
    std::queue<Event> eventQueue_;
    std::mutex eventQueueMutex_;
    std::condition_variable eventQueueCondition_;
    std::atomic<bool> running_{false};
    std::thread eventProcessingThread_;

    // 가드 조건과 대상 상태 매핑
    std::unordered_map<std::string, State> guardTargetMap_;
    mutable std::mutex guardTargetMapMutex_;

    // 가드 ID를 상태 문자열로 변환
    std::string stateToString(State state) const;

    // 상태 문자열을 State 열거형으로 변환
    State stringToState(const std::string &stateId) const;
};
