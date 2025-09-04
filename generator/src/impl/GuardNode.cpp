#include "impl/GuardNode.h"
#include "../../generator/include/Logger.h"
#include <stdexcept>

namespace SCXML {
namespace Core {

GuardNode::GuardNode(const std::string &id, const std::string &target)
    : id_(id), target_(target), condition_(""), targetState_(""), reactive_(false) {
    SCXML::Common::Logger::debug("GuardNode::Constructor - Creating guard node: " + id + " -> " + target);
}

GuardNode::~GuardNode() {
    SCXML::Common::Logger::debug("GuardNode::Destructor - Destroying guard node: " + id_);
}

const std::string &GuardNode::getId() const {
    return id_;
}

const std::string &GuardNode::getTarget() const {
    return target_;
}

void GuardNode::setTargetState(const std::string &targetState) {
    targetState_ = targetState;
    SCXML::Common::Logger::debug("GuardNode::setTargetState() - Setting target state for " + id_ + ": " + targetState);
}

const std::string &GuardNode::getTargetState() const {
    return targetState_;
}

void GuardNode::setCondition(const std::string &condition) {
    condition_ = condition;
    SCXML::Common::Logger::debug("GuardNode::setCondition() - Setting condition for " + id_ + ": " + condition);
}

const std::string &GuardNode::getCondition() const {
    return condition_;
}

void GuardNode::addDependency(const std::string &property) {
    dependencies_.push_back(property);
    SCXML::Common::Logger::debug("GuardNode::addDependency() - Adding dependency for " + id_ + ": " + property);
}

const std::vector<std::string> &GuardNode::getDependencies() const {
    return dependencies_;
}

void GuardNode::setExternalClass(const std::string &className) {
    externalClass_ = className;
    SCXML::Common::Logger::debug("GuardNode::setExternalClass() - Setting external class for " + id_ + ": " + className);
}

const std::string &GuardNode::getExternalClass() const {
    return externalClass_;
}

void GuardNode::setExternalFactory(const std::string &factoryName) {
    externalFactory_ = factoryName;
    SCXML::Common::Logger::debug("GuardNode::setExternalFactory() - Setting external factory for " + id_ + ": " + factoryName);
}

const std::string &GuardNode::getExternalFactory() const {
    return externalFactory_;
}

void GuardNode::setReactive(bool reactive) {
    reactive_ = reactive;
    SCXML::Common::Logger::debug("GuardNode::setReactive() - Setting reactive flag for " + id_ + ": " + (reactive ? "true" : "false"));
}

bool GuardNode::isReactive() const {
    return reactive_;
}

bool GuardNode::evaluate(SCXML::Model::IExecutionContext& context) const {
    // Basic implementation - in real implementation, this would evaluate the condition
    return !condition_.empty();
}

std::shared_ptr<SCXML::Model::IGuardNode> GuardNode::clone() const {
    auto cloned = std::make_shared<GuardNode>(id_, target_);
    cloned->targetState_ = targetState_;
    cloned->condition_ = condition_;
    cloned->dependencies_ = dependencies_;
    cloned->externalClass_ = externalClass_;
    cloned->externalFactory_ = externalFactory_;
    cloned->reactive_ = reactive_;
    return cloned;
}

}  // namespace Core
}  // namespace SCXML