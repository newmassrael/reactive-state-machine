#include "impl/TransitionNode.h"
#include "../../generator/include/Logger.h"
#include <algorithm>

namespace SCXML {
namespace Core {

TransitionNode::TransitionNode(const std::string &event, const std::string &target)
    : event_(event), target_(target), guard_(""), reactive_(false), internal_(false), targetsDirty_(true) {
    if (!target.empty()) {
        targets_.push_back(target);
    }
    SCXML::Common::Logger::debug("TransitionNode::Constructor - Creating transition node: " + (event.empty() ? "<no event>" : event) +
                        " -> " + target);
}

TransitionNode::~TransitionNode() {
    SCXML::Common::Logger::debug("TransitionNode::Destructor - Destroying transition node: " +
                        (event_.empty() ? "<no event>" : event_) + " -> " +
                        (targets_.empty() ? "<no target>" : targets_[0]));
}

const std::string &TransitionNode::getEvent() const {
    return event_;
}

const std::string &TransitionNode::getTarget() const {
    return target_;
}

void TransitionNode::addTarget(const std::string &target) {
    targets_.push_back(target);
    targetsDirty_ = true;
    SCXML::Common::Logger::debug("TransitionNode::addTarget() - Adding target to transition " +
                        (event_.empty() ? "<no event>" : event_) + ": " + target);
}

const std::vector<std::string> &TransitionNode::getTargets() const {
    return targets_;
}

void TransitionNode::setTargets(const std::vector<std::string> &targets) {
    targets_ = targets;
    targetsDirty_ = true;
}

void TransitionNode::clearTargets() {
    targets_.clear();
    targetsDirty_ = true;
    SCXML::Common::Logger::debug("TransitionNode::clearTargets() - Clearing targets for transition " +
                        (event_.empty() ? "<no event>" : event_));
}

const std::string &TransitionNode::getGuard() const {
    return guard_;
}

void TransitionNode::setGuard(const std::string &guard) {
    guard_ = guard;
    SCXML::Common::Logger::debug("TransitionNode::setGuard() - Setting guard for transition " +
                        (event_.empty() ? "<no event>" : event_) + ": " + guard);
}

const std::vector<std::string> &TransitionNode::getActions() const {
    return actions_;
}

void TransitionNode::addAction(const std::string &action) {
    actions_.push_back(action);
    SCXML::Common::Logger::debug("TransitionNode::addAction() - Adding action to transition " +
                        (event_.empty() ? "<no event>" : event_) + ": " + action);
}

bool TransitionNode::isReactive() const {
    return reactive_;
}

void TransitionNode::setReactive(bool reactive) {
    reactive_ = reactive;
    SCXML::Common::Logger::debug("TransitionNode::setReactive() - Setting reactive flag for transition " +
                        (event_.empty() ? "<no event>" : event_) + ": " + (reactive ? "true" : "false"));
}

bool TransitionNode::isInternal() const {
    return internal_;
}

void TransitionNode::setInternal(bool internal) {
    internal_ = internal;
    SCXML::Common::Logger::debug("TransitionNode::setInternal() - Setting internal flag for transition " +
                        (event_.empty() ? "<no event>" : event_) + ": " + (internal ? "true" : "false"));
}

void TransitionNode::setAttribute(const std::string &name, const std::string &value) {
    attributes_[name] = value;
    SCXML::Common::Logger::debug("TransitionNode::setAttribute() - Setting attribute for transition " +
                        (event_.empty() ? "<no event>" : event_) + ": " + name + " = " + value);
}

const std::string &TransitionNode::getAttribute(const std::string &name) const {
    static const std::string empty = "";
    auto it = attributes_.find(name);
    return (it != attributes_.end()) ? it->second : empty;
}

const std::unordered_map<std::string, std::string> &TransitionNode::getAttributes() const {
    return attributes_;
}

void TransitionNode::addEvent(const std::string &event) {
    if (!event.empty()) {
        events_.push_back(event);
        SCXML::Common::Logger::debug("TransitionNode::addEvent() - Adding event to transition: " + event);
    }
}

const std::vector<std::string> &TransitionNode::getEvents() const {
    return events_;
}

void TransitionNode::addActionNode(std::shared_ptr<SCXML::Model::IActionNode> actionNode) {
    if (actionNode) {
        actionNodes_.push_back(actionNode);
        SCXML::Common::Logger::debug("TransitionNode::addActionNode() - Adding action node to transition: " + actionNode->getId());
    }
}

const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &TransitionNode::getActionNodes() const {
    return actionNodes_;
}

std::shared_ptr<SCXML::Model::ITransitionNode> TransitionNode::clone() const {
    auto cloned = std::make_shared<TransitionNode>(event_, target_);
    cloned->targets_ = targets_;
    cloned->guard_ = guard_;
    cloned->actions_ = actions_;
    cloned->reactive_ = reactive_;
    cloned->internal_ = internal_;
    cloned->attributes_ = attributes_;
    cloned->events_ = events_;
    // Note: actionNodes_ is not deep-cloned here
    cloned->targetsDirty_ = targetsDirty_;
    return cloned;
}

}  // namespace Core
}  // namespace SCXML