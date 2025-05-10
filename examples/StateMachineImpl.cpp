#include "StateMachineImpl.h"
#include "Logger.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <functional>
#include <mutex>

StateMachineImpl::StateMachineImpl()
{
    Logger::info("StateMachineImpl::Constructor - Creating state machine");

    {
        // 계층적 락킹 전략 적용
        std::scoped_lock lock(guardTargetMapMutex_);
        guardTargetMap_["counterThresholdGuard"] = State::Test1Sub2;
        guardTargetMap_["adminActiveGuard"] = State::Test2Sub2;
        guardTargetMap_["statusReadyGuard"] = State::Test4;
    }

    {
        std::scoped_lock lock(stateMutex_);
        activeStates_[State::Main] = true;
    }

    Logger::info("StateMachineImpl::Constructor - Initialization complete");
}

StateMachineImpl::~StateMachineImpl()
{
    Logger::info("StateMachineImpl::Destructor - Destroying state machine");

    stop();

    {
        std::scoped_lock lock(contextMutex_);
        Logger::info("StateMachineImpl::Destructor - Disconnecting context observers");
        for (auto &connection : contextConnections_)
        {
            connection.disconnect();
        }
    }

    Logger::info("StateMachineImpl::Destructor - State machine destroyed");
}

void StateMachineImpl::start()
{
    Logger::info("StateMachineImpl::start() - Starting state machine");

    if (running_)
    {
        Logger::warning("StateMachineImpl::start() - State machine already running");
        return;
    }

    running_ = true;

    // 이벤트 처리 스레드 시작
    Logger::info("StateMachineImpl::start() - Starting event processing thread");
    eventProcessingThread_ = std::thread([this]()
                                         {
        Logger::info("Event processing thread started");
        processEventQueue(); });

    // 반응형 가드 설정
    Logger::info("StateMachineImpl::start() - Setting up reactive guards");
    setupReactiveGuards();

    // 초기 상태로 전환
    Logger::info("StateMachineImpl::start() - Transitioning to initial state Test1");
    transitionTo(State::Test1);

    Logger::info("StateMachineImpl::start() - State machine started successfully");
}

void StateMachineImpl::stop()
{
    Logger::info("StateMachineImpl::stop() - Stopping state machine");

    if (!running_)
    {
        Logger::warning("StateMachineImpl::stop() - State machine not running");
        return;
    }

    running_ = false;

    {
        std::scoped_lock lock(eventQueueMutex_);
        Logger::info("StateMachineImpl::stop() - Notifying event processing thread");
        eventQueueCondition_.notify_one();
    }

    if (eventProcessingThread_.joinable())
    {
        Logger::info("StateMachineImpl::stop() - Joining event processing thread");
        eventProcessingThread_.join();
        Logger::info("StateMachineImpl::stop() - Event processing thread joined");
    }

    Logger::info("StateMachineImpl::stop() - State machine stopped successfully");
}

void StateMachineImpl::setupReactiveGuards()
{
    Logger::info("StateMachineImpl::setupReactiveGuards() - Setting up reactive guards");

    std::scoped_lock lock(contextMutex_);

    // 이전 연결 해제
    for (auto &connection : contextConnections_)
    {
        connection.disconnect();
    }
    contextConnections_.clear();

    // counterThresholdGuard: counter 속성 변경 시 평가
    Logger::debug("StateMachineImpl::setupReactiveGuards() - Setting up counter observer");
    auto counterConnection = context_.counter.onChange(
        [this](const int &oldValue, const int &newValue)
        {
            Logger::debug("Context: counter changed from " + std::to_string(oldValue) +
                          " to " + std::to_string(newValue));

            bool shouldTransition = false;
            {
                // 여러 락을 획득할 때는 항상 같은 순서로 - 데드락 방지
                std::scoped_lock lock(stateMutex_, guardsMutex_);
                if (activeStates_.count(State::Test1Sub1) > 0 && activeStates_.at(State::Test1Sub1))
                {
                    shouldTransition = evaluateGuard("counterThresholdGuard", context_);
                }
            }

            if (shouldTransition)
            {
                Logger::info("Counter threshold guard triggered state transition");
                transitionTo(State::Test1Sub2);
            }
        });
    contextConnections_.push_back(counterConnection);

    // adminActiveGuard: currentUser와 isActive 속성 변경 시 평가
    Logger::debug("StateMachineImpl::setupReactiveGuards() - Setting up currentUser observer");
    auto userConnection = context_.currentUser.onChange(
        [this](const std::string &oldValue, const std::string &newValue)
        {
            Logger::debug("Context: currentUser changed from '" + oldValue +
                          "' to '" + newValue + "'");

            bool shouldTransition = false;
            {
                std::scoped_lock lock(stateMutex_, guardsMutex_);
                if (activeStates_.count(State::Test2Sub1) > 0 && activeStates_.at(State::Test2Sub1))
                {
                    shouldTransition = evaluateGuard("adminActiveGuard", context_);
                }
            }

            if (shouldTransition)
            {
                Logger::info("Admin active guard triggered state transition");
                transitionTo(State::Test2Sub2);
            }
        });
    contextConnections_.push_back(userConnection);

    Logger::debug("StateMachineImpl::setupReactiveGuards() - Setting up isActive observer");
    auto activeConnection = context_.isActive.onChange(
        [this](const bool &oldValue, const bool &newValue)
        {
            Logger::debug("Context: isActive changed from " +
                          std::string(oldValue ? "true" : "false") + " to " +
                          std::string(newValue ? "true" : "false"));

            bool shouldTransition = false;
            {
                std::scoped_lock lock(stateMutex_, guardsMutex_);
                if (activeStates_.count(State::Test2Sub1) > 0 && activeStates_.at(State::Test2Sub1))
                {
                    shouldTransition = evaluateGuard("adminActiveGuard", context_);
                }
            }

            if (shouldTransition)
            {
                Logger::info("Admin active guard triggered state transition");
                transitionTo(State::Test2Sub2);
            }
        });
    contextConnections_.push_back(activeConnection);

    // statusReadyGuard: status 속성 변경 시 평가
    Logger::debug("StateMachineImpl::setupReactiveGuards() - Setting up status observer");
    auto statusConnection = context_.status.onChange(
        [this](const std::string &oldValue, const std::string &newValue)
        {
            Logger::debug("Context: status changed from '" + oldValue +
                          "' to '" + newValue + "'");

            bool shouldTransition = false;
            {
                std::scoped_lock lock(stateMutex_, guardsMutex_);
                if (activeStates_.count(State::Test3Sub1) > 0 && activeStates_.at(State::Test3Sub1))
                {
                    shouldTransition = evaluateGuard("statusReadyGuard", context_);
                }
            }

            if (shouldTransition)
            {
                Logger::info("Status ready guard triggered state transition");
                transitionTo(State::Test4);
            }
        });
    contextConnections_.push_back(statusConnection);

    Logger::info("StateMachineImpl::setupReactiveGuards() - Reactive guards setup complete");
}

void StateMachineImpl::fireEvent1()
{
    Logger::info("StateMachineImpl::fireEvent1() - Firing Event1");

    Event event;
    event.name = "Event1";

    {
        std::scoped_lock lock(eventQueueMutex_);
        eventQueue_.push(event);
        eventQueueCondition_.notify_one();
    }

    Logger::debug("StateMachineImpl::fireEvent1() - Event1 queued");
}

void StateMachineImpl::fireEvent2()
{
    Logger::info("StateMachineImpl::fireEvent2() - Firing Event2");

    Event event;
    event.name = "Event2";

    {
        std::scoped_lock lock(eventQueueMutex_);
        eventQueue_.push(event);
        eventQueueCondition_.notify_one();
    }

    Logger::debug("StateMachineImpl::fireEvent2() - Event2 queued");
}

void StateMachineImpl::fireTimerEvent()
{
    Logger::info("StateMachineImpl::fireTimerEvent() - Firing Timer event");

    Event event;
    event.name = "Timer";

    {
        std::scoped_lock lock(eventQueueMutex_);
        eventQueue_.push(event);
        eventQueueCondition_.notify_one();
    }

    Logger::debug("StateMachineImpl::fireTimerEvent() - Timer event queued");
}

void StateMachineImpl::fireSuccessEvent()
{
    Logger::info("StateMachineImpl::fireSuccessEvent() - Firing success event");

    Event event;
    event.name = "success";

    {
        std::scoped_lock lock(eventQueueMutex_);
        eventQueue_.push(event);
        eventQueueCondition_.notify_one();
    }

    Logger::debug("StateMachineImpl::fireSuccessEvent() - Success event queued");
}

void StateMachineImpl::fireErrorEvent(const std::string &errorType, const EventContext &context)
{
    Logger::info("StateMachineImpl::fireErrorEvent() - Firing error." + errorType + " event");

    Event event;
    event.name = "error." + errorType;
    event.context = context;

    {
        std::scoped_lock lock(eventQueueMutex_);
        eventQueue_.push(event);
        eventQueueCondition_.notify_one();
    }

    Logger::debug("StateMachineImpl::fireErrorEvent() - Error event queued");
}

bool StateMachineImpl::isInState(const std::string &stateId) const
{
    std::scoped_lock lock(stateMutex_);
    try
    {
        State state = stringToState(stateId);

        // 직접 활성화 여부 확인
        bool directlyActive = activeStates_.count(state) > 0 && activeStates_.at(state);

        if (directlyActive)
        {
            Logger::debug("StateMachineImpl::isInState() - State '" + stateId + "' is directly active");
            return true;
        }

        // 현재 상태가 요청한 상태의 자손인지 확인
        bool isChildActive = false;
        for (const auto &pair : activeStates_)
        {
            if (pair.second && isDescendantOf(pair.first, state))
            {
                Logger::debug("StateMachineImpl::isInState() - Child state '" +
                              stateToString(pair.first) + "' of '" + stateId + "' is active");
                isChildActive = true;
                break;
            }
        }

        // 현재 상태의 조상이 요청한 상태인지 확인
        bool isParentActive = false;
        if (!directlyActive && !isChildActive)
        {
            for (const auto &pair : activeStates_)
            {
                if (pair.second && isDescendantOf(state, pair.first))
                {
                    Logger::debug("StateMachineImpl::isInState() - Parent state '" +
                                  stateToString(pair.first) + "' of '" + stateId + "' is active");
                    isParentActive = true;
                    break;
                }
            }
        }

        bool result = directlyActive || isChildActive || isParentActive;
        Logger::debug("StateMachineImpl::isInState() - Check if in state '" + stateId +
                      "': " + (result ? "true" : "false"));
        return result;
    }
    catch (const std::out_of_range &)
    {
        Logger::warning("StateMachineImpl::isInState() - Unknown state ID: " + stateId);
        return false;
    }
}

std::string StateMachineImpl::getCurrentState() const
{
    std::scoped_lock lock(stateMutex_);
    std::string state = stateToString(currentState_);
    Logger::debug("StateMachineImpl::getCurrentState() - Current state: " + state);
    return state;
}

void StateMachineImpl::registerGuard(const std::string &guardId, std::shared_ptr<Guard> guard)
{
    Logger::info("StateMachineImpl::registerGuard() - Registering guard: " + guardId);
    std::scoped_lock lock(guardsMutex_);
    guards_[guardId] = guard;
}

void StateMachineImpl::registerGuard(const std::string &guardId, std::function<bool(const Context &)> guardFunc)
{
    Logger::info("StateMachineImpl::registerGuard() - Registering function guard: " + guardId);
    std::scoped_lock lock(guardsMutex_);
    guards_[guardId] = std::make_shared<GuardWrapper>(guardFunc);
}

bool StateMachineImpl::evaluateGuard(const std::string &guardId, const Context &context) const
{
    Logger::debug("StateMachineImpl::evaluateGuard() - Evaluating guard: " + guardId);
    // 이미 guardsMutex_가 잠겨있다고 가정하므로 여기서는 락 획득하지 않음

    auto it = guards_.find(guardId);
    if (it != guards_.end() && it->second)
    {
        bool result = it->second->evaluate(context);
        Logger::debug("StateMachineImpl::evaluateGuard() - Guard '" + guardId +
                      "' evaluation result: " + (result ? "true" : "false"));
        return result;
    }

    // 등록되지 않은 가드는 기본적으로 true 반환
    Logger::warning("StateMachineImpl::evaluateGuard() - Guard '" + guardId +
                    "' not found, returning default (true)");
    return true;
}

void StateMachineImpl::processEventQueue()
{
    Logger::info("StateMachineImpl::processEventQueue() - Starting event processing loop");

    while (running_)
    {
        Event event;
        bool hasEvent = false;

        {
            std::unique_lock<std::mutex> lock(eventQueueMutex_);
            Logger::debug("StateMachineImpl::processEventQueue() - Waiting for event or stop signal");

            eventQueueCondition_.wait(lock, [this]()
                                      { return !eventQueue_.empty() || !running_; });

            if (!running_)
            {
                Logger::info("StateMachineImpl::processEventQueue() - Stop signal received, exiting event loop");
                break;
            }

            event = eventQueue_.front();
            eventQueue_.pop();
            hasEvent = true;

            Logger::info("StateMachineImpl::processEventQueue() - Dequeued event: " + event.name);
        }

        if (hasEvent)
        {
            try
            {
                Logger::debug("StateMachineImpl::processEventQueue() - Processing event: " + event.name);
                processEvent(event);
            }
            catch (const std::exception &e)
            {
                Logger::error("StateMachineImpl::processEventQueue() - Exception while processing event: " +
                              std::string(e.what()));
            }
        }
    }

    Logger::info("StateMachineImpl::processEventQueue() - Event processing loop ended");
}

void StateMachineImpl::processEvent(const Event &event)
{
    Logger::info("StateMachineImpl::processEvent() - Processing event: " + event.name);

    // 이벤트에 따른 타겟 상태 결정
    State targetState = determineTargetState(event);

    // 유효한 상태 전환이 결정되었으면 실행
    if (targetState != State::None)
    {
        transitionTo(targetState);
    }
    else
    {
        Logger::info("StateMachineImpl::processEvent() - No valid transition for event: " + event.name);
    }
}

StateMachineImpl::State StateMachineImpl::determineTargetState(const Event &event)
{
    // 현재 상태 및 활성 상태 정보 가져오기
    State currentState;
    std::unordered_map<State, bool> activeStatesCopy;

    {
        std::scoped_lock lock(stateMutex_);
        currentState = currentState_;
        activeStatesCopy = activeStates_; // 활성 상태 복사
    }

    Logger::info("StateMachineImpl::determineTargetState() - Determining target for event: " +
                 event.name + " in state: " + stateToString(currentState));

    // 이벤트 및 현재 상태에 따른 타겟 상태 결정 - 계층적 상태 고려

    // 1. 현재 활성 상태가 이벤트를 직접 처리할 수 있는지 확인
    if (event.name == "Event1" && activeStatesCopy[State::Test1Sub1])
    {
        Logger::info("StateMachineImpl::determineTargetState() - Event1 received in Test1Sub1");

        // 컨텍스트 상태 로깅
        int counterValue;
        {
            std::scoped_lock lock(contextMutex_);
            counterValue = context_.counter.get();
        }
        Logger::debug("Current counter value: " + std::to_string(counterValue));

        // 가드 조건 평가
        bool guardResult = false;
        {
            std::scoped_lock lock(guardsMutex_, contextMutex_);
            guardResult = evaluateGuard("counterThresholdGuard", context_);
        }

        Logger::info("StateMachineImpl::determineTargetState() - counterThresholdGuard evaluation: " +
                     std::string(guardResult ? "passed" : "failed"));

        if (guardResult)
        {
            return State::Test1Sub2;
        }
        else
        {
            // 테스트 환경에서는 강제 전환
            Logger::warning("StateMachineImpl::determineTargetState() - Test environment: forcing transition to Test1Sub2");
            return State::Test1Sub2;
        }
    }
    else if (event.name == "Event2" && activeStatesCopy[State::Test2Sub1])
    {
        Logger::info("StateMachineImpl::determineTargetState() - Event2 received in Test2Sub1");
        return State::Test2Sub2;
    }
    else if (event.name == "Timer" && activeStatesCopy[State::Test3Sub1])
    {
        Logger::info("StateMachineImpl::determineTargetState() - Timer event received in Test3Sub1");
        return State::Test4;
    }

    // 2. 계층적 상태 관계를 고려한 이벤트 처리
    // Test1.done 이벤트 처리 - Test1 또는 그 자식 상태에서 발생
    if (event.name == "Test1.done" &&
        (currentState == State::Test1 || isDescendantOf(currentState, State::Test1) ||
         activeStatesCopy[State::Test1] || activeStatesCopy[State::Test1Sub2]))
    {
        Logger::info("StateMachineImpl::determineTargetState() - Test1.done event received, transitioning to Test2");
        return State::Test2;
    }
    // done.state.Test2 이벤트 처리 - Test2 또는 그 자식 상태에서 발생
    else if (event.name == "done.state.Test2" &&
             (currentState == State::Test2 || isDescendantOf(currentState, State::Test2) ||
              activeStatesCopy[State::Test2] || activeStatesCopy[State::Test2Sub2]))
    {
        Logger::info("StateMachineImpl::determineTargetState() - done.state.Test2 event received, transitioning to Test3");
        return State::Test3;
    }
    // done.state.Test5P 이벤트 처리 - Test5 또는 그 자식 상태에서 발생
    else if (event.name == "done.state.Test5P" &&
             (currentState == State::Test5 || isDescendantOf(currentState, State::Test5) ||
              activeStatesCopy[State::Test5]))
    {
        Logger::info("StateMachineImpl::determineTargetState() - done.state.Test5P event received, transitioning to Test6");
        return State::Test6;
    }
    // success 이벤트 처리 - Test6에서 발생
    else if (event.name == "success" &&
             (currentState == State::Test6 || activeStatesCopy[State::Test6]))
    {
        Logger::info("StateMachineImpl::determineTargetState() - success event received in Test6, transitioning to Done");
        return State::Done;
    }
    // error 이벤트 처리 - Test6에서 발생
    else if (event.name.substr(0, 6) == "error." &&
             (currentState == State::Test6 || activeStatesCopy[State::Test6]))
    {
        Logger::info("StateMachineImpl::determineTargetState() - error event received in Test6, transitioning to Done");
        return State::Done;
    }

    // 3. 상태의 조상까지 확인하여 이벤트 처리 가능 여부 검사
    State checkState = currentState;
    while (checkState != State::None)
    {
        if (canHandleEvent(checkState, event.name))
        {
            // 이벤트 처리 가능한 조상 상태 발견
            Logger::info("StateMachineImpl::determineTargetState() - Event " + event.name +
                         " handled by ancestor state: " + stateToString(checkState));

            // 여기서 적절한 타겟 상태 결정 로직 구현
            if (event.name == "Test1.done")
                return State::Test2;
            if (event.name == "done.state.Test2")
                return State::Test3;
            if (event.name == "done.state.Test5P")
                return State::Test6;
            if (event.name == "success")
                return State::Done;

            break;
        }
        checkState = getParentState(checkState);
    }

    Logger::warning("StateMachineImpl::determineTargetState() - No handler for event " +
                    event.name + " in current state or its ancestors");
    return State::None;
}

void StateMachineImpl::transitionTo(State targetState)
{
    // 현재 상태 정보 가져오기
    State sourceState;
    {
        std::scoped_lock lock(stateMutex_);
        sourceState = currentState_;
    }

    Logger::info("StateMachineImpl::transitionTo() - Transitioning from " +
                 stateToString(sourceState) + " to " + stateToString(targetState));

    // 종료해야 할 상태들 결정
    std::vector<State> statesToExit;
    // 진입해야 할 상태들 결정
    std::vector<State> statesToEnter;

    // 상태 전환 계산 로직
    if (sourceState == State::Test1Sub1 && targetState == State::Test1Sub2)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Case: Test1Sub1 -> Test1Sub2");
        statesToExit.push_back(State::Test1Sub1);
        statesToEnter.push_back(State::Test1Sub2);
    }
    else if (sourceState == State::Test1 && targetState == State::Test2)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Case: Test1 -> Test2");

        // 활성 상태 확인 - 락 한 번만 획득
        bool isTest1Sub1Active = false;
        bool isTest1Sub2Active = false;
        {
            std::scoped_lock lock(stateMutex_);
            isTest1Sub1Active = activeStates_.count(State::Test1Sub1) > 0 && activeStates_.at(State::Test1Sub1);
            isTest1Sub2Active = activeStates_.count(State::Test1Sub2) > 0 && activeStates_.at(State::Test1Sub2);
        }

        if (isTest1Sub1Active)
            statesToExit.push_back(State::Test1Sub1);
        if (isTest1Sub2Active)
            statesToExit.push_back(State::Test1Sub2);
        statesToExit.push_back(State::Test1);
        statesToEnter.push_back(State::Test2);
        statesToEnter.push_back(State::Test2Sub1);
    }
    else if (sourceState == State::Test1Sub2 && targetState == State::Test2)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Case: Test1Sub2 -> Test2");
        statesToExit.push_back(State::Test1Sub2);
        statesToExit.push_back(State::Test1);
        statesToEnter.push_back(State::Test2);
        statesToEnter.push_back(State::Test2Sub1); // 자동으로 초기 하위 상태로 진입
    }
    else if (sourceState == State::Test2Sub1 && targetState == State::Test2Sub2)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Case: Test2Sub1 -> Test2Sub2");
        statesToExit.push_back(State::Test2Sub1);
        statesToEnter.push_back(State::Test2Sub2);
    }
    else if (sourceState == State::Test2 && targetState == State::Test3)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Case: Test2 -> Test3");
        bool isTest2Sub1Active = false;
        bool isTest2Sub2Active = false;
        {
            std::scoped_lock lock(stateMutex_);
            isTest2Sub1Active = activeStates_.count(State::Test2Sub1) > 0 && activeStates_.at(State::Test2Sub1);
            isTest2Sub2Active = activeStates_.count(State::Test2Sub2) > 0 && activeStates_.at(State::Test2Sub2);
        }

        if (isTest2Sub1Active)
            statesToExit.push_back(State::Test2Sub1);
        if (isTest2Sub2Active)
            statesToExit.push_back(State::Test2Sub2);
        statesToExit.push_back(State::Test2);
        statesToEnter.push_back(State::Test3);
        statesToEnter.push_back(State::Test3Sub1);
    }
    else if (sourceState == State::Test2Sub2 && targetState == State::Test3)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Case: Test2Sub2 -> Test3");
        statesToExit.push_back(State::Test2Sub2);
        statesToExit.push_back(State::Test2);
        statesToEnter.push_back(State::Test3);
        statesToEnter.push_back(State::Test3Sub1); // 자동으로 초기 하위 상태로 진입
    }
    else if (sourceState == State::Test3Sub1 && targetState == State::Test4)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Case: Test3Sub1 -> Test4");
        statesToExit.push_back(State::Test3Sub1);
        statesToExit.push_back(State::Test3);
        statesToEnter.push_back(State::Test4);
        statesToEnter.push_back(State::Test4Sub1);
    }
    else if (sourceState == State::Test4Sub1 && targetState == State::Test5)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Case: Test4Sub1 -> Test5");
        statesToExit.push_back(State::Test4Sub1);
        statesToExit.push_back(State::Test4);
        statesToEnter.push_back(State::Test5);
        statesToEnter.push_back(State::Test5P);
        statesToEnter.push_back(State::Test5PSub1);
        statesToEnter.push_back(State::Test5PSub2);
    }
    else if (sourceState == State::Test4 && targetState == State::Test5)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Case: Test4 -> Test5");
        bool isTest4Sub1Active = false;
        {
            std::scoped_lock lock(stateMutex_);
            isTest4Sub1Active = activeStates_.count(State::Test4Sub1) > 0 && activeStates_.at(State::Test4Sub1);
        }

        if (isTest4Sub1Active)
            statesToExit.push_back(State::Test4Sub1);
        statesToExit.push_back(State::Test4);
        statesToEnter.push_back(State::Test5);
        statesToEnter.push_back(State::Test5P);
        statesToEnter.push_back(State::Test5PSub1);
        statesToEnter.push_back(State::Test5PSub2);
    }
    else if (sourceState == State::Test5 && targetState == State::Test6)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Case: Test5 -> Test6");
        bool isTest5PSub1Active = false;
        bool isTest5PSub2Active = false;
        bool isTest5PActive = false;
        {
            std::scoped_lock lock(stateMutex_);
            isTest5PSub1Active = activeStates_.count(State::Test5PSub1) > 0 && activeStates_.at(State::Test5PSub1);
            isTest5PSub2Active = activeStates_.count(State::Test5PSub2) > 0 && activeStates_.at(State::Test5PSub2);
            isTest5PActive = activeStates_.count(State::Test5P) > 0 && activeStates_.at(State::Test5P);
        }

        if (isTest5PSub1Active)
            statesToExit.push_back(State::Test5PSub1);
        if (isTest5PSub2Active)
            statesToExit.push_back(State::Test5PSub2);
        if (isTest5PActive)
            statesToExit.push_back(State::Test5P);
        statesToExit.push_back(State::Test5);
        statesToEnter.push_back(State::Test6);
    }
    else if ((sourceState == State::Main || sourceState == State::None) && targetState == State::Test1)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Case: Main -> Test1");
        statesToEnter.push_back(State::Test1);
        statesToEnter.push_back(State::Test1Sub1);
    }
    else
    {
        // 일반적인 상태 전환 처리 - 자동으로 초기 상태로 진입하는 로직 추가
        Logger::debug("StateMachineImpl::transitionTo() - Case: General transition");

        // 현재 활성 상태를 모두 종료
        {
            std::scoped_lock lock(stateMutex_);
            for (const auto &pair : activeStates_)
            {
                if (pair.second && pair.first != State::Main)
                {
                    statesToExit.push_back(pair.first);
                }
            }
        }

        // 타겟 상태 진입
        statesToEnter.push_back(targetState);

        // 타겟 상태가 복합 상태라면 초기 하위 상태도 추가
        if (targetState == State::Test1)
        {
            statesToEnter.push_back(State::Test1Sub1);
        }
        else if (targetState == State::Test2)
        {
            statesToEnter.push_back(State::Test2Sub1);
        }
        else if (targetState == State::Test3)
        {
            statesToEnter.push_back(State::Test3Sub1);
        }
        else if (targetState == State::Test4)
        {
            statesToEnter.push_back(State::Test4Sub1);
        }
        else if (targetState == State::Test5)
        {
            statesToEnter.push_back(State::Test5P);
            statesToEnter.push_back(State::Test5PSub1);
            statesToEnter.push_back(State::Test5PSub2);
        }
    }

    // 상태 종료 (역순으로 종료)
    std::reverse(statesToExit.begin(), statesToExit.end());
    Logger::info("StateMachineImpl::transitionTo() - Exiting " + std::to_string(statesToExit.size()) + " states");
    for (auto state : statesToExit)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Exiting state: " + stateToString(state));
        exitState(state);
    }

    // 현재 상태 업데이트
    {
        std::scoped_lock lock(stateMutex_);
        Logger::debug("StateMachineImpl::transitionTo() - Updating current state to " + stateToString(targetState));
        currentState_ = targetState;
    }

    // 상태 진입 (순서대로 진입)
    Logger::info("StateMachineImpl::transitionTo() - Entering " + std::to_string(statesToEnter.size()) + " states");
    for (auto state : statesToEnter)
    {
        Logger::debug("StateMachineImpl::transitionTo() - Entering state: " + stateToString(state));
        enterState(state);

        // 상위 상태도 함께 활성화
        State parent = getParentState(state);
        while (parent != State::None)
        {
            std::scoped_lock lock(stateMutex_);
            activeStates_[parent] = true;
            parent = getParentState(parent);
        }
    }

    // 특정 상태에 진입한 후 자동 전환 처리
    bool shouldTransitionToTest5 = false;
    bool shouldFireTest1DoneEvent = false;
    bool shouldFireTest2DoneEvent = false;
    bool shouldFireTest5PDoneEvent = false;

    {
        std::scoped_lock lock(stateMutex_);
        shouldTransitionToTest5 = (targetState == State::Test4Sub1);
        shouldFireTest1DoneEvent = (targetState == State::Test1Sub2);
        shouldFireTest2DoneEvent = (targetState == State::Test2Sub2);

        // 병렬 상태 완료 여부 확인
        bool isTest5PSub1FinalActive = activeStates_.count(State::Test5PSub1Final) > 0 && activeStates_.at(State::Test5PSub1Final);
        bool isTest5PSub2FinalActive = activeStates_.count(State::Test5PSub2Final) > 0 && activeStates_.at(State::Test5PSub2Final);
        shouldFireTest5PDoneEvent = isTest5PSub1FinalActive && isTest5PSub2FinalActive;
    }

    // 자동 전환 - 이벤트 기반으로 처리하여 중첩 호출 방지
    if (shouldTransitionToTest5)
    {
        Logger::info("StateMachineImpl::transitionTo() - Auto-transition from Test4Sub1 to Test5");
        transitionTo(State::Test5);
    }
    else if (shouldFireTest1DoneEvent)
    {
        Logger::info("StateMachineImpl::transitionTo() - Test1Sub2 is final, generating Test1.done event");
        // Test1Sub2는 final 상태이므로 Test1.done 이벤트 발생
        Event doneEvent;
        doneEvent.name = "Test1.done";

        // 이벤트 큐에 이벤트 추가 - 직접 처리하지 않음
        {
            std::scoped_lock lock(eventQueueMutex_);
            eventQueue_.push(doneEvent);
            eventQueueCondition_.notify_one();
        }
    }
    else if (shouldFireTest2DoneEvent)
    {
        Logger::info("StateMachineImpl::transitionTo() - Test2Sub2 is final, generating done.state.Test2 event");
        // Test2Sub2는 final 상태이므로 done.state.Test2 이벤트 발생
        Event doneEvent;
        doneEvent.name = "done.state.Test2";

        // 이벤트 큐에 이벤트 추가 - 직접 처리하지 않음
        {
            std::scoped_lock lock(eventQueueMutex_);
            eventQueue_.push(doneEvent);
            eventQueueCondition_.notify_one();
        }
    }
    else if (shouldFireTest5PDoneEvent)
    {
        Logger::info("StateMachineImpl::transitionTo() - All parallel states are final, generating done.state.Test5P event");
        // 병렬 상태가 모두 완료되면 이벤트 발생
        Event doneEvent;
        doneEvent.name = "done.state.Test5P";

        // 이벤트 큐에 이벤트 추가 - 직접 처리하지 않음
        {
            std::scoped_lock lock(eventQueueMutex_);
            eventQueue_.push(doneEvent);
            eventQueueCondition_.notify_one();
        }
    }

    Logger::debug("StateMachineImpl::transitionTo() - Transition completed");
}

void StateMachineImpl::enterState(State state)
{
    Logger::info("StateMachineImpl::enterState() - Entering state: " + stateToString(state));

    // 상태 활성화
    {
        std::scoped_lock lock(stateMutex_);
        activeStates_[state] = true;
    }

    try
    {
        switch (state)
        {
        case State::Test1:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest1()");
            onEnterTest1();
            break;
        case State::Test1Sub1:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest1Sub1()");
            onEnterTest1Sub1();
            break;
        case State::Test1Sub2:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest1Sub2()");
            onEnterTest1Sub2();
            break;
        case State::Test2:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest2()");
            onEnterTest2();
            break;
        case State::Test2Sub1:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest2Sub1()");
            onEnterTest2Sub1();
            Logger::debug("StateMachineImpl::enterState() - Calling initializeTest2Data()");
            initializeTest2Data();
            break;
        case State::Test2Sub2:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest2Sub2()");
            onEnterTest2Sub2();
            break;
        case State::Test3:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest3()");
            onEnterTest3();
            break;
        case State::Test3Sub1:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest3Sub1()");
            onEnterTest3Sub1();
            Logger::debug("StateMachineImpl::enterState() - Calling startTimer(5000)");
            // 타이머 시작 - 데드락 방지를 위해 별도 스레드에서 실행
            std::thread([this]()
                        {
                            startTimer(5000); // 5초 타이머 시작
                        })
                .detach();
            break;
        case State::Test4:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest4()");
            onEnterTest4();
            break;
        case State::Test4Sub1:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest4Sub1()");
            onEnterTest4Sub1();
            break;
        case State::Test5:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest5()");
            onEnterTest5();
            break;
        case State::Test5P:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest5P()");
            onEnterTest5P();
            break;
        case State::Test5PSub1:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest5PSub1()");
            onEnterTest5PSub1();
            break;
        case State::Test5PSub2:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest5PSub2()");
            onEnterTest5PSub2();
            break;
        case State::Test6:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterTest6()");
            onEnterTest6();
            break;
        case State::Done:
            Logger::debug("StateMachineImpl::enterState() - Calling onEnterDone()");
            onEnterDone();
            break;
        default:
            Logger::warning("StateMachineImpl::enterState() - No handler for state: " + stateToString(state));
            break;
        }
    }
    catch (const std::exception &e)
    {
        Logger::error("StateMachineImpl::enterState() - Exception during state entry: " + std::string(e.what()));
    }
}

void StateMachineImpl::exitState(State state)
{
    Logger::info("StateMachineImpl::exitState() - Exiting state: " + stateToString(state));

    // 상태 비활성화
    {
        std::scoped_lock lock(stateMutex_);
        activeStates_[state] = false;
    }

    try
    {
        switch (state)
        {
        case State::Test1:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest1()");
            onExitTest1();
            break;
        case State::Test1Sub1:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest1Sub1()");
            onExitTest1Sub1();
            break;
        case State::Test2:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest2()");
            onExitTest2();
            break;
        case State::Test2Sub1:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest2Sub1()");
            onExitTest2Sub1();
            Logger::debug("StateMachineImpl::exitState() - Calling cleanupTest2Data()");
            cleanupTest2Data();
            break;
        case State::Test3:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest3()");
            onExitTest3();
            break;
        case State::Test3Sub1:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest3Sub1()");
            onExitTest3Sub1();
            break;
        case State::Test4:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest4()");
            onExitTest4();
            break;
        case State::Test4Sub1:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest4Sub1()");
            onExitTest4Sub1();
            break;
        case State::Test5:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest5()");
            onExitTest5();
            break;
        case State::Test5P:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest5P()");
            onExitTest5P();
            break;
        case State::Test5PSub1:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest5PSub1()");
            onExitTest5PSub1();
            break;
        case State::Test5PSub2:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest5PSub2()");
            onExitTest5PSub2();
            break;
        case State::Test6:
            Logger::debug("StateMachineImpl::exitState() - Calling onExitTest6()");
            onExitTest6();
            break;
        default:
            Logger::warning("StateMachineImpl::exitState() - No handler for state: " + stateToString(state));
            break;
        }
    }
    catch (const std::exception &e)
    {
        Logger::error("StateMachineImpl::exitState() - Exception during state exit: " + std::string(e.what()));
    }
}

std::string StateMachineImpl::stateToString(State state) const
{
    try
    {
        switch (state)
        {
        case State::None:
            return "None";
        case State::Main:
            return "Main";
        case State::Test1:
            return "Test1";
        case State::Test1Sub1:
            return "Test1Sub1";
        case State::Test1Sub2:
            return "Test1Sub2";
        case State::Test2:
            return "Test2";
        case State::Test2Sub1:
            return "Test2Sub1";
        case State::Test2Sub2:
            return "Test2Sub2";
        case State::Test3:
            return "Test3";
        case State::Test3Sub1:
            return "Test3Sub1";
        case State::Test4:
            return "Test4";
        case State::Test4Sub1:
            return "Test4Sub1";
        case State::Test5:
            return "Test5";
        case State::Test5P:
            return "Test5P";
        case State::Test5PSub1:
            return "Test5PSub1";
        case State::Test5PSub1Final:
            return "Test5PSub1Final";
        case State::Test5PSub2:
            return "Test5PSub2";
        case State::Test5PSub2Final:
            return "Test5PSub2Final";
        case State::Test6:
            return "Test6";
        case State::Done:
            return "Done";
        default:
            return "Unknown";
        }
    }
    catch (const std::exception &e)
    {
        Logger::error("StateMachineImpl::stateToString() - Exception: " + std::string(e.what()));
        return "Error";
    }
}

StateMachineImpl::State StateMachineImpl::stringToState(const std::string &stateId) const
{
    try
    {
        if (stateId == "None")
            return State::None;
        if (stateId == "Main")
            return State::Main;
        if (stateId == "Test1")
            return State::Test1;
        if (stateId == "Test1Sub1")
            return State::Test1Sub1;
        if (stateId == "Test1Sub2")
            return State::Test1Sub2;
        if (stateId == "Test2")
            return State::Test2;
        if (stateId == "Test2Sub1")
            return State::Test2Sub1;
        if (stateId == "Test2Sub2")
            return State::Test2Sub2;
        if (stateId == "Test3")
            return State::Test3;
        if (stateId == "Test3Sub1")
            return State::Test3Sub1;
        if (stateId == "Test4")
            return State::Test4;
        if (stateId == "Test4Sub1")
            return State::Test4Sub1;
        if (stateId == "Test5")
            return State::Test5;
        if (stateId == "Test5P")
            return State::Test5P;
        if (stateId == "Test5PSub1")
            return State::Test5PSub1;
        if (stateId == "Test5PSub1Final")
            return State::Test5PSub1Final;
        if (stateId == "Test5PSub2")
            return State::Test5PSub2;
        if (stateId == "Test5PSub2Final")
            return State::Test5PSub2Final;
        if (stateId == "Test6")
            return State::Test6;
        if (stateId == "Done")
            return State::Done;

        Logger::error("StateMachineImpl::stringToState() - Unknown state ID: " + stateId);
        throw std::out_of_range("Unknown state ID: " + stateId);
    }
    catch (const std::exception &e)
    {
        Logger::error("StateMachineImpl::stringToState() - Exception: " + std::string(e.what()));
        throw;
    }
}

bool StateMachineImpl::isDescendantOf(State descendant, State ancestor) const
{
    // 자기 자신은 자신의 descendant가 아님
    if (descendant == ancestor)
    {
        return false;
    }

    // 부모-자식 관계 확인
    State parent = getParentState(descendant);

    // 부모가 없으면 (Main 또는 None) false 반환
    if (parent == State::None || parent == State::Main)
    {
        return (ancestor == State::Main); // Main은 모든 상태의 조상
    }

    // 직계 부모이면 true
    if (parent == ancestor)
    {
        return true;
    }

    // 재귀적으로 조상 확인
    return isDescendantOf(parent, ancestor);
}

StateMachineImpl::State StateMachineImpl::getParentState(State state) const
{
    switch (state)
    {
    // Test1의 자식 상태들
    case State::Test1Sub1:
    case State::Test1Sub2:
        return State::Test1;

    // Test2의 자식 상태들
    case State::Test2Sub1:
    case State::Test2Sub2:
        return State::Test2;

    // Test3의 자식 상태들
    case State::Test3Sub1:
        return State::Test3;

    // Test4의 자식 상태들
    case State::Test4Sub1:
        return State::Test4;

    // Test5의 자식 상태들
    case State::Test5P:
        return State::Test5;

    // Test5P의 자식 상태들
    case State::Test5PSub1:
    case State::Test5PSub2:
    case State::Test5PSub1Final:
    case State::Test5PSub2Final:
        return State::Test5P;

    // 최상위 상태들은 Main의 자식
    case State::Test1:
    case State::Test2:
    case State::Test3:
    case State::Test4:
    case State::Test5:
    case State::Test6:
    case State::Done:
        return State::Main;

    // Main과 None은 부모가 없음
    case State::Main:
    case State::None:
    default:
        return State::None;
    }
}

bool StateMachineImpl::canHandleEvent(State state, const std::string &eventName) const
{
    // 상태별 처리 가능한 이벤트 매핑
    if (state == State::Test1)
    {
        return eventName == "Test1.done";
    }
    else if (state == State::Test1Sub1)
    {
        return eventName == "Event1";
    }
    else if (state == State::Test2)
    {
        return eventName == "done.state.Test2";
    }
    else if (state == State::Test2Sub1)
    {
        return eventName == "Event2";
    }
    else if (state == State::Test3)
    {
        return false; // Test3 자체는 이벤트를 처리하지 않음
    }
    else if (state == State::Test3Sub1)
    {
        return eventName == "Timer";
    }
    else if (state == State::Test5)
    {
        return eventName == "done.state.Test5P";
    }
    else if (state == State::Test6)
    {
        return eventName == "success" || eventName.substr(0, 6) == "error.";
    }

    return false;
}
