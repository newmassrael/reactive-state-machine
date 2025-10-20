#pragma once

#include "core/EventQueueManager.h"
#include "runtime/EventRaiserImpl.h"
#include <memory>

namespace RSM::Core {

/**
 * @brief AOT 엔진용 내부 이벤트 큐 어댑터
 *
 * EventQueueManager<Event>를 EventProcessingAlgorithms에서 사용 가능한
 * 통일된 인터페이스로 래핑.
 *
 * @tparam EventType 이벤트 타입 (보통 enum class Event)
 *
 * @example
 * @code
 * EventQueueManager<Event> eventQueue_;
 * AOTEventQueue<Event> adapter(eventQueue_);
 *
 * EventProcessingAlgorithms::processInternalEventQueue(
 *     adapter,
 *     [this](Event e) { return processInternalEvent(e); }
 * );
 * @endcode
 */
template <typename EventType> class AOTEventQueue {
public:
    /**
     * @brief Constructor
     * @param queue EventQueueManager 참조
     */
    explicit AOTEventQueue(EventQueueManager<EventType> &queue) : queue_(queue) {}

    /**
     * @brief 큐에 이벤트가 있는지 확인
     * @return true if queue has events
     */
    bool hasEvents() const {
        return queue_.hasEvents();
    }

    /**
     * @brief 큐에서 다음 이벤트를 꺼냄 (FIFO)
     * @return 꺼낸 이벤트
     */
    EventType popNext() {
        return queue_.pop();
    }

private:
    EventQueueManager<EventType> &queue_;
};

/**
 * @brief Interpreter 엔진용 내부 이벤트 큐 어댑터
 *
 * EventRaiserImpl을 EventProcessingAlgorithms에서 사용 가능한
 * 통일된 인터페이스로 래핑.
 *
 * @note EventRaiserImpl은 processNextQueuedEvent()가 내부적으로
 *       이벤트를 콜백으로 처리하므로, popNext()는 실제 이벤트 값을
 *       반환하지 않고 처리 성공 여부만 반환.
 *
 * @example
 * @code
 * std::shared_ptr<EventRaiserImpl> eventRaiser_;
 * InterpreterEventQueue adapter(eventRaiser_);
 *
 * EventProcessingAlgorithms::processInternalEventQueue(
 *     adapter,
 *     [](bool) { return true; }  // EventRaiser가 내부적으로 처리
 * );
 * @endcode
 */
class InterpreterEventQueue {
public:
    /**
     * @brief Constructor
     * @param raiser EventRaiserImpl shared_ptr
     */
    explicit InterpreterEventQueue(std::shared_ptr<EventRaiserImpl> raiser) : raiser_(raiser) {}

    /**
     * @brief 큐에 이벤트가 있는지 확인
     * @return true if queue has events
     */
    bool hasEvents() const {
        return raiser_ && raiser_->hasQueuedEvents();
    }

    /**
     * @brief 큐에서 다음 이벤트를 처리
     *
     * EventRaiserImpl::processNextQueuedEvent()를 호출하여
     * 내부적으로 이벤트를 콜백으로 처리.
     *
     * @return 처리 성공 여부 (실제 이벤트 값은 반환하지 않음)
     */
    bool popNext() {
        return raiser_ && raiser_->processNextQueuedEvent();
    }

private:
    std::shared_ptr<EventRaiserImpl> raiser_;
};

}  // namespace RSM::Core
