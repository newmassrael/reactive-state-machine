#include "events/EventQueue.h"
#include "common/Logger.h"
#include "events/EventQueue.h"
#include <functional>
#include <map>

namespace SCXML {
namespace Events {

// ========== EventQueue Implementation ==========

EventQueue::EventQueue() : sequence_(0), shutdown_(false) {}

EventQueue::~EventQueue() {
    shutdown();
}

void EventQueue::enqueue(std::shared_ptr<Event> event) {
    enqueue(event, Priority::NORMAL);
}

void EventQueue::enqueue(std::shared_ptr<Event> event, Priority priority) {
    if (!event || shutdown_.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    PriorityEvent priorityEvent;
    priorityEvent.event = event;
    priorityEvent.priority = priority;
    priorityEvent.sequence = sequence_++;

    SCXML::Common::Logger::debug("EventQueue::enqueue - Event '" + event->getName() + "' assigned sequence " + std::to_string(priorityEvent.sequence));

    queue_.push(priorityEvent);
    condition_.notify_one();
}

std::shared_ptr<Event> EventQueue::dequeue() {
    std::unique_lock<std::mutex> lock(mutex_);

    condition_.wait(lock, [this] { return !queue_.empty() || shutdown_.load(); });

    if (shutdown_.load() && queue_.empty()) {
        return nullptr;
    }

    if (queue_.empty()) {
        return nullptr;
    }

    auto priorityEvent = queue_.top();
    queue_.pop();

    SCXML::Common::Logger::debug("EventQueue::dequeue - Dequeuing event '" + priorityEvent.event->getName() + "' with sequence " + std::to_string(priorityEvent.sequence));

    return priorityEvent.event;
}

bool EventQueue::tryDequeue(std::shared_ptr<Event> &event) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.empty()) {
        return false;
    }

    auto priorityEvent = queue_.top();
    queue_.pop();
    event = priorityEvent.event;

    SCXML::Common::Logger::debug("EventQueue::tryDequeue - Dequeuing event '" + priorityEvent.event->getName() + "' with sequence " + std::to_string(priorityEvent.sequence));

    return true;
}

size_t EventQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool EventQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

void EventQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        queue_.pop();
    }
}

void EventQueue::shutdown() {
    shutdown_.store(true);
    condition_.notify_all();
}

bool EventQueue::isShutdown() const {
    return shutdown_.load();
}

// ========== EventDispatcher Implementation ==========

EventDispatcher::EventDispatcher() {}

EventDispatcher::~EventDispatcher() {}

bool EventDispatcher::dispatch(std::shared_ptr<Event> event, const std::string &target) {
    if (!event || target.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = handlers_.find(target);
    if (it != handlers_.end()) {
        try {
            return it->second(event);
        } catch (const std::exception &e) {
            SCXML::Common::Logger::error("EventDispatcher::dispatch - Exception in event handler for target '" + target +
                          "': " + std::string(e.what()));
            return false;
        } catch (...) {
            SCXML::Common::Logger::error("EventDispatcher::dispatch - Unknown exception in event handler for target '" + target + "'");
            return false;
        }
    }

    return false;
}

void EventDispatcher::registerHandler(const std::string &target, std::function<bool(std::shared_ptr<Event>)> handler) {
    if (target.empty() || !handler) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    handlers_[target] = handler;
}

void EventDispatcher::unregisterHandler(const std::string &target) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_.erase(target);
}

std::vector<std::string> EventDispatcher::getRegisteredTargets() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> targets;
    targets.reserve(handlers_.size());

    for (const auto &pair : handlers_) {
        targets.push_back(pair.first);
    }

    return targets;
}

}  // namespace Events
}  // namespace SCXML