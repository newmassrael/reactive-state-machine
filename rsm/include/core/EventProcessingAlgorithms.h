#pragma once

#include "common/Logger.h"
#include <functional>

namespace RSM::Core {

/**
 * @brief W3C SCXML 이벤트 처리 알고리즘 (Single Source of Truth)
 *
 * Interpreter와 AOT 엔진의 모든 이벤트 처리 로직을 템플릿 기반으로 공유.
 *
 * 설계 원칙:
 * 1. 알고리즘만 공유, 자료구조는 각 엔진 최적화 유지
 * 2. 템플릿 기반 제로 오버헤드 (인라인 확장)
 * 3. 명확한 인터페이스로 타입 안전성 보장
 *
 * @note 이 클래스의 모든 메서드는 static template 함수로,
 *       컴파일 타임에 인라인 확장되어 런타임 오버헤드가 없음.
 */
class EventProcessingAlgorithms {
public:
    /**
     * @brief W3C SCXML 3.12.1: 내부 이벤트 큐 처리 (FIFO)
     *
     * Macrostep 완료 시 모든 내부 이벤트를 FIFO 순서로 소진.
     * Interpreter와 AOT 엔진 모두 동일한 알고리즘 사용.
     *
     * @tparam EventQueue 이벤트 큐 타입
     *   요구 메서드: bool hasEvents() const, EventType popNext()
     * @tparam EventHandler 이벤트 처리 콜백 타입
     *   시그니처: bool handler(EventType event)
     *
     * @param queue 내부 이벤트 큐 (AOTEventQueue 또는 InterpreterEventQueue)
     * @param handler 이벤트 처리 함수 (false 반환 시 처리 중단)
     *
     * @example AOT 엔진:
     * @code
     * AOTEventQueue aotQueue(eventQueue_);
     * processInternalEventQueue(aotQueue, [this](Event e) {
     *     return processInternalEvent(e);
     * });
     * @endcode
     *
     * @example Interpreter 엔진:
     * @code
     * InterpreterEventQueue interpQueue(eventRaiser_);
     * processInternalEventQueue(interpQueue, [this](auto) {
     *     return true;  // EventRaiser가 내부적으로 처리
     * });
     * @endcode
     */
    template <typename EventQueue, typename EventHandler>
    static void processInternalEventQueue(EventQueue &queue, EventHandler &&handler) {
        // W3C SCXML 3.12.1: Process all internal events in FIFO order
        while (queue.hasEvents()) {
            auto event = queue.popNext();

            // 이벤트 처리 실패 시 중단
            if (!handler(event)) {
                LOG_DEBUG("EventProcessingAlgorithms: Event handler returned false, stopping queue processing");
                break;
            }
        }
    }

    /**
     * @brief W3C SCXML 3.13: Eventless transitions 체크
     *
     * 상태 진입 후 이벤트 없이 자동 실행되는 전환을 체크.
     * 무한 루프 방지를 위한 최대 반복 횟수 제한 포함.
     *
     * @tparam StateMachine 상태 머신 타입
     *   요구 메서드:
     *   - StateType getCurrentState() const
     *   - bool processEventlessTransition()
     *   - void executeOnExit(StateType)
     *   - void executeOnEntry(StateType)
     * @tparam EventQueue 내부 이벤트 큐 타입
     * @tparam InternalEventProcessor 내부 이벤트 처리 함수 타입
     *
     * @param sm 상태 머신 인스턴스
     * @param queue 내부 이벤트 큐
     * @param processInternalEvent 내부 이벤트 처리 함수
     * @param maxIterations 최대 반복 횟수 (기본 100)
     * @return true if any eventless transition occurred, false otherwise
     */
    template <typename StateMachine, typename EventQueue, typename InternalEventProcessor>
    static bool checkEventlessTransitions(StateMachine &sm, EventQueue &queue,
                                          InternalEventProcessor &&processInternalEvent, int maxIterations = 100) {
        bool anyTransition = false;
        int iterations = 0;

        while (iterations++ < maxIterations) {
            auto oldState = sm.getCurrentState();

            // W3C SCXML 3.13: Eventless transition 시도
            if (sm.processEventlessTransition()) {
                auto newState = sm.getCurrentState();

                if (oldState != newState) {
                    anyTransition = true;
                    sm.executeOnExit(oldState);
                    sm.executeOnEntry(newState);

                    // 새 상태 진입 후 내부 이벤트 처리
                    processInternalEventQueue(queue, processInternalEvent);

                    // 계속해서 eventless transition 체크
                } else {
                    // 상태 변경 없음 - 중단
                    break;
                }
            } else {
                // Eventless transition 없음 - 중단
                break;
            }
        }

        if (iterations >= maxIterations) {
            LOG_ERROR("EventProcessingAlgorithms: Eventless transition loop detected after {} iterations",
                      maxIterations);
            return false;
        }

        return anyTransition;
    }

    /**
     * @brief W3C SCXML 3.3 / D.1: Complete Macrostep 처리
     *
     * 외부 이벤트 처리 → 내부 이벤트 소진 → Eventless transitions.
     * Interpreter와 AOT 엔진의 핵심 이벤트 처리 패턴.
     *
     * @tparam StateMachine 상태 머신 타입
     * @tparam Event 이벤트 타입
     * @tparam EventQueue 내부 이벤트 큐 타입
     * @tparam InternalEventProcessor 내부 이벤트 처리 함수 타입
     *
     * @param sm 상태 머신 인스턴스
     * @param event 외부 이벤트
     * @param queue 내부 이벤트 큐
     * @param processInternalEvent 내부 이벤트 처리 함수
     * @param checkEventless Eventless transitions 체크 여부 (기본 true)
     */
    template <typename StateMachine, typename Event, typename EventQueue, typename InternalEventProcessor>
    static void processMacrostep(StateMachine &sm, const Event &event, EventQueue &queue,
                                 InternalEventProcessor &&processInternalEvent, bool checkEventless = true) {
        auto oldState = sm.getCurrentState();

        // 1. W3C SCXML 3.12: 외부 이벤트로 전환 시도
        if (sm.processTransition(event)) {
            auto newState = sm.getCurrentState();

            // 2. 상태 변경 시: exit/entry 실행
            if (oldState != newState) {
                sm.executeOnExit(oldState);
                sm.executeOnEntry(newState);

                // 3. W3C SCXML 3.12.1: 내부 이벤트 모두 처리
                processInternalEventQueue(queue, processInternalEvent);

                // 4. W3C SCXML 3.13: Eventless transitions
                if (checkEventless) {
                    checkEventlessTransitions(sm, queue, processInternalEvent);
                }
            }
        }
    }
};

}  // namespace RSM::Core
