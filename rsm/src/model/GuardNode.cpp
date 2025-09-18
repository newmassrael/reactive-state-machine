#include "GuardNode.h"
#include "common/Logger.h"
using namespace RSM;


GuardNode::GuardNode(const std::string &id, const std::string &target)
    : id_(id), target_(target), condition_(""), targetState_(""), reactive_(false)
{
    // target이 조건식인지 상태 ID인지 판별하고 적절한 필드에 저장
    Logger::debug("GuardNode::Constructor - Creating guard node: " + id + " -> " + target);
}


GuardNode::~GuardNode()
{
    Logger::debug("GuardNode::Destructor - Destroying guard node: " + id_);
}


const std::string &GuardNode::getId() const
{
    return id_;
}


void GuardNode::setTargetState(const std::string &targetState)
{
    targetState_ = targetState;
    Logger::debug("GuardNode::setTargetState() - Setting target state for " + id_ + ": " + targetState);
}


const std::string &GuardNode::getTargetState() const
{
    return targetState_;
}


void GuardNode::setCondition(const std::string &condition)
{
    condition_ = condition;
    Logger::debug("GuardNode::setCondition() - Setting condition for " + id_ + ": " + condition);
}


const std::string &GuardNode::getCondition() const
{
    return condition_;
}


void GuardNode::addDependency(const std::string &property)
{
    Logger::debug("GuardNode::addDependency() - Adding dependency for " + id_ + ": " + property);
    dependencies_.push_back(property);
}


const std::vector<std::string> &GuardNode::getDependencies() const
{
    return dependencies_;
}


void GuardNode::setExternalClass(const std::string &className)
{
    Logger::debug("GuardNode::setExternalClass() - Setting external class for " + id_ + ": " + className);
    externalClass_ = className;
}


const std::string &GuardNode::getExternalClass() const
{
    return externalClass_;
}


void GuardNode::setExternalFactory(const std::string &factoryName)
{
    Logger::debug("GuardNode::setExternalFactory() - Setting external factory for " + id_ + ": " + factoryName);
    externalFactory_ = factoryName;
}


const std::string &GuardNode::getExternalFactory() const
{
    return externalFactory_;
}


void GuardNode::setReactive(bool reactive)
{
    Logger::debug("GuardNode::setReactive() - Setting reactive flag for " + id_ + ": " + (reactive ? "true" : "false"));
    reactive_ = reactive;
}


bool GuardNode::isReactive() const
{
    return reactive_;
}


void GuardNode::setAttribute(const std::string &name, const std::string &value)
{
    attributes_[name] = value;
}


const std::string &GuardNode::getAttribute(const std::string &name) const
{
    auto it = attributes_.find(name);
    if (it != attributes_.end())
    {
        return it->second;
    }
    return emptyString_;
}


const std::unordered_map<std::string, std::string> &GuardNode::getAttributes() const
{
    return attributes_;
}
