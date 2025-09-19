#include "ActionNode.h"
#include "common/Logger.h"

RSM::ActionNode::ActionNode(const std::string &id) : id_(id), type_("normal") {
    Logger::debug("RSM::ActionNode::Constructor - Creating action node: " + id);
}

RSM::ActionNode::~ActionNode() {
    Logger::debug("RSM::ActionNode::Destructor - Destroying action node: " + id_);
}

const std::string &RSM::ActionNode::getId() const {
    return id_;
}

void RSM::ActionNode::setExternalClass(const std::string &className) {
    Logger::debug("RSM::ActionNode::setExternalClass() - Setting external class for " + id_ + ": " + className);
    externalClass_ = className;
}

const std::string &RSM::ActionNode::getExternalClass() const {
    return externalClass_;
}

void RSM::ActionNode::setExternalFactory(const std::string &factoryName) {
    Logger::debug("RSM::ActionNode::setExternalFactory() - Setting external factory for " + id_ + ": " + factoryName);
    externalFactory_ = factoryName;
}

const std::string &RSM::ActionNode::getExternalFactory() const {
    return externalFactory_;
}

void RSM::ActionNode::setType(const std::string &type) {
    Logger::debug("RSM::ActionNode::setType() - Setting type for " + id_ + ": " + type);
    type_ = type;
}

const std::string &RSM::ActionNode::getType() const {
    return type_;
}

void RSM::ActionNode::setAttribute(const std::string &name, const std::string &value) {
    Logger::debug("RSM::ActionNode::setAttribute() - Setting attribute for " + id_ + ": " + name + " = " + value);
    attributes_[name] = value;
}

const std::string &RSM::ActionNode::getAttribute(const std::string &name) const {
    auto it = attributes_.find(name);
    if (it != attributes_.end()) {
        return it->second;
    }
    return emptyString_;
}

const std::unordered_map<std::string, std::string> &RSM::ActionNode::getAttributes() const {
    return attributes_;
}
