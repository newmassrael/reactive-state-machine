#include "ActionNode.h"
#include "common/Logger.h"
using namespace RSM;


ActionNode::ActionNode(const std::string &id)
    : id_(id), type_("normal")
{
    Logger::debug("ActionNode::Constructor - Creating action node: " + id);
}


ActionNode::~ActionNode()
{
    Logger::debug("ActionNode::Destructor - Destroying action node: " + id_);
}


const std::string &ActionNode::getId() const
{
    return id_;
}


void ActionNode::setExternalClass(const std::string &className)
{
    Logger::debug("ActionNode::setExternalClass() - Setting external class for " + id_ + ": " + className);
    externalClass_ = className;
}


const std::string &ActionNode::getExternalClass() const
{
    return externalClass_;
}


void ActionNode::setExternalFactory(const std::string &factoryName)
{
    Logger::debug("ActionNode::setExternalFactory() - Setting external factory for " + id_ + ": " + factoryName);
    externalFactory_ = factoryName;
}


const std::string &ActionNode::getExternalFactory() const
{
    return externalFactory_;
}


void ActionNode::setType(const std::string &type)
{
    Logger::debug("ActionNode::setType() - Setting type for " + id_ + ": " + type);
    type_ = type;
}


const std::string &ActionNode::getType() const
{
    return type_;
}


void ActionNode::setAttribute(const std::string &name, const std::string &value)
{
    Logger::debug("ActionNode::setAttribute() - Setting attribute for " + id_ + ": " + name + " = " + value);
    attributes_[name] = value;
}


const std::string &ActionNode::getAttribute(const std::string &name) const
{
    auto it = attributes_.find(name);
    if (it != attributes_.end())
    {
        return it->second;
    }
    return emptyString_;
}


const std::unordered_map<std::string, std::string> &ActionNode::getAttributes() const
{
    return attributes_;
}
