#include "GuardNode.h"
#include "common/Logger.h"

RSM::GuardNode::GuardNode(const std::string &id, const std::string &target)
    : id_(id), target_(target), condition_(""), targetState_(""), reactive_(false) {
    // Determine if target is a condition expression or state ID and store in appropriate field
    LOG_DEBUG("Creating guard node: {} -> {}", id, target);
}

RSM::GuardNode::~GuardNode() {
    LOG_DEBUG("Destroying guard node: {}", id_);
}

const std::string &RSM::GuardNode::getId() const {
    return id_;
}

void RSM::GuardNode::setTargetState(const std::string &targetState) {
    targetState_ = targetState;
    LOG_DEBUG("Setting target state for {}: {}", id_, targetState);
}

const std::string &RSM::GuardNode::getTargetState() const {
    return targetState_;
}

void RSM::GuardNode::setCondition(const std::string &condition) {
    condition_ = condition;
    LOG_DEBUG("Setting condition for {}: {}", id_, condition);
}

const std::string &RSM::GuardNode::getCondition() const {
    return condition_;
}

void RSM::GuardNode::addDependency(const std::string &property) {
    LOG_DEBUG("Adding dependency for {}: {}", id_, property);
    dependencies_.push_back(property);
}

const std::vector<std::string> &RSM::GuardNode::getDependencies() const {
    return dependencies_;
}

void RSM::GuardNode::setExternalClass(const std::string &className) {
    LOG_DEBUG("Setting external class for {}: {}", id_, className);
    externalClass_ = className;
}

const std::string &RSM::GuardNode::getExternalClass() const {
    return externalClass_;
}

void RSM::GuardNode::setExternalFactory(const std::string &factoryName) {
    LOG_DEBUG("Setting external factory for {}: {}", id_, factoryName);
    externalFactory_ = factoryName;
}

const std::string &RSM::GuardNode::getExternalFactory() const {
    return externalFactory_;
}

void RSM::GuardNode::setReactive(bool reactive) {
    LOG_DEBUG("Setting reactive flag for {}: {}", id_, (reactive ? "true" : "false"));
    reactive_ = reactive;
}

bool RSM::GuardNode::isReactive() const {
    return reactive_;
}

void RSM::GuardNode::setAttribute(const std::string &name, const std::string &value) {
    attributes_[name] = value;
}

const std::string &RSM::GuardNode::getAttribute(const std::string &name) const {
    auto it = attributes_.find(name);
    if (it != attributes_.end()) {
        return it->second;
    }
    return emptyString_;
}

const std::unordered_map<std::string, std::string> &RSM::GuardNode::getAttributes() const {
    return attributes_;
}
