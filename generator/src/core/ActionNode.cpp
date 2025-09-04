#include "core/ActionNode.h"
#include "common/Logger.h"
#include "runtime/RuntimeContext.h"

namespace SCXML {
namespace Core {

ActionNode::ActionNode(const std::string &id) : id_(id), type_("normal") {
    SCXML::Common::Logger::debug("ActionNode::Constructor - Creating action node: " + id);
}

ActionNode::~ActionNode() {
    SCXML::Common::Logger::debug("ActionNode::Destructor - Destroying action node: " + id_);
}

const std::string &ActionNode::getId() const {
    return id_;
}

void ActionNode::setExternalClass(const std::string &className) {
    SCXML::Common::Logger::debug("ActionNode::setExternalClass() - Setting external class for " + id_ + ": " + className);
    externalClass_ = className;
}

const std::string &ActionNode::getExternalClass() const {
    return externalClass_;
}

void ActionNode::setExternalFactory(const std::string &factoryName) {
    SCXML::Common::Logger::debug("ActionNode::setExternalFactory() - Setting external factory for " + id_ + ": " + factoryName);
    externalFactory_ = factoryName;
}

const std::string &ActionNode::getExternalFactory() const {
    return externalFactory_;
}

void ActionNode::setType(const std::string &type) {
    SCXML::Common::Logger::debug("ActionNode::setType() - Setting type for " + id_ + ": " + type);
    type_ = type;
}

const std::string &ActionNode::getType() const {
    return type_;
}

void ActionNode::setAttribute(const std::string &name, const std::string &value) {
    SCXML::Common::Logger::debug("ActionNode::setAttribute() - Setting attribute for " + id_ + ": " + name + " = " + value);
    attributes_[name] = value;
}

const std::string &ActionNode::getAttribute(const std::string &name) const {
    auto it = attributes_.find(name);
    if (it != attributes_.end()) {
        return it->second;
    }
    return emptyString_;
}

const std::unordered_map<std::string, std::string> &ActionNode::getAttributes() const {
    return attributes_;
}

bool ActionNode::execute(::SCXML::Runtime::RuntimeContext &context) {
    SCXML::Common::Logger::debug("ActionNode::execute - Base class execute called for: " + id_);

    // Execute all child actions
    bool allSucceeded = true;
    for (const auto &childAction : childActions_) {
        if (childAction) {
            SCXML::Common::Logger::debug("ActionNode::execute - Executing child action: " + childAction->getId());
            bool result = childAction->execute(context);
            if (!result) {
                SCXML::Common::Logger::warning("ActionNode::execute - Child action failed: " + childAction->getId());
                allSucceeded = false;
            }
        }
    }

    return allSucceeded;
}

std::shared_ptr<SCXML::Model::IActionNode> ActionNode::clone() const {
    SCXML::Common::Logger::debug("ActionNode::clone - Base class clone called for: " + id_);

    // Create a basic ActionNode clone
    auto cloned = std::make_shared<ActionNode>(id_);
    cloned->setExternalClass(externalClass_);
    cloned->setExternalFactory(externalFactory_);
    cloned->setType(type_);

    // Copy attributes
    for (const auto &attr : attributes_) {
        cloned->setAttribute(attr.first, attr.second);
    }

    // Clone child actions
    for (const auto &childAction : childActions_) {
        if (childAction) {
            auto clonedChild = childAction->clone();
            cloned->addChildAction(clonedChild);
        }
    }

    return cloned;
}

}  // namespace Core
}  // namespace SCXML
