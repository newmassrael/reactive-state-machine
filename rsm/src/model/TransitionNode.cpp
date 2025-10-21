// TransitionNode.cpp
#include "TransitionNode.h"
#include "common/Logger.h"
#include <algorithm>
#include <sstream>

RSM::TransitionNode::TransitionNode(const std::string &event, const std::string &target)
    : event_(event), target_(target), guard_(""), reactive_(false), internal_(false), targetsDirty_(true) {
    LOG_DEBUG("Creating transition node: {} -> {}", (event.empty() ? "<no event>" : event), target);

    if (!event.empty()) {
        events_.push_back(event);
    }
}

RSM::TransitionNode::~TransitionNode() {
    LOG_DEBUG("Destroying transition node: {} -> {}", (event_.empty() ? "<no event>" : event_), target_);
}

const std::string &RSM::TransitionNode::getEvent() const {
    return event_;
}

std::vector<std::string> RSM::TransitionNode::getTargets() const {
    // Caching mechanism - parse if targets changed or not yet parsed
    if (targetsDirty_) {
        parseTargets();
        targetsDirty_ = false;
    }
    return cachedTargets_;
}

void RSM::TransitionNode::addTarget(const std::string &target) {
    LOG_DEBUG("Adding target to transition {}: {}", (event_.empty() ? "<no event>" : event_), target);

    if (target.empty()) {
        return;  // Do not add empty target
    }

    if (target_.empty()) {
        target_ = target;
    } else {
        target_ += " " + target;
    }
    targetsDirty_ = true;  // Mark cache needs update
}

void RSM::TransitionNode::clearTargets() {
    LOG_DEBUG("Clearing targets for transition {}", (event_.empty() ? "<no event>" : event_));

    target_.clear();
    cachedTargets_.clear();
    targetsDirty_ = false;  // Cache already empty, no update needed
}

bool RSM::TransitionNode::hasTargets() const {
    if (!targetsDirty_ && !cachedTargets_.empty()) {
        return true;
    }
    return !target_.empty();
}

void RSM::TransitionNode::parseTargets() const {
    cachedTargets_.clear();

    if (target_.empty()) {
        return;
    }

    std::istringstream ss(target_);
    std::string target;
    while (ss >> target) {
        cachedTargets_.push_back(target);
    }
}

void RSM::TransitionNode::setGuard(const std::string &guard) {
    LOG_DEBUG("Setting guard for transition {} -> {}: {}", (event_.empty() ? "<no event>" : event_), target_, guard);
    guard_ = guard;
}

const std::string &RSM::TransitionNode::getGuard() const {
    return guard_;
}

void RSM::TransitionNode::addActionNode(std::shared_ptr<RSM::IActionNode> actionNode) {
    LOG_DEBUG("Adding ActionNode to transition {} -> {}: {}", (event_.empty() ? "<no event>" : event_), target_,
              (actionNode ? actionNode->getActionType() : "null"));
    if (actionNode) {
        actionNodes_.push_back(actionNode);
    }
}

const std::vector<std::shared_ptr<RSM::IActionNode>> &RSM::TransitionNode::getActionNodes() const {
    return actionNodes_;
}

void RSM::TransitionNode::setReactive(bool reactive) {
    LOG_DEBUG("Setting reactive flag for transition {} -> {}: {}", (event_.empty() ? "<no event>" : event_), target_,
              (reactive ? "true" : "false"));
    reactive_ = reactive;
}

bool RSM::TransitionNode::isReactive() const {
    return reactive_;
}

void RSM::TransitionNode::setInternal(bool internal) {
    LOG_DEBUG("Setting internal flag for transition {} -> {}: {}", (event_.empty() ? "<no event>" : event_), target_,
              (internal ? "true" : "false"));
    internal_ = internal;
}

bool RSM::TransitionNode::isInternal() const {
    return internal_;
}

void RSM::TransitionNode::setAttribute(const std::string &name, const std::string &value) {
    LOG_DEBUG("Setting attribute for transition {} -> {}: {}={}", (event_.empty() ? "<no event>" : event_), target_,
              name, value);
    attributes_[name] = value;
}

std::string RSM::TransitionNode::getAttribute(const std::string &name) const {
    auto it = attributes_.find(name);
    if (it != attributes_.end()) {
        return it->second;
    }
    return "";
}

void RSM::TransitionNode::addEvent(const std::string &event) {
    if (std::find(events_.begin(), events_.end(), event) == events_.end()) {
        LOG_DEBUG("Adding event to transition: {}", event);
        events_.push_back(event);
    }
}

const std::vector<std::string> &RSM::TransitionNode::getEvents() const {
    return events_;
}
