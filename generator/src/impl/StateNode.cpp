#include "impl/StateNode.h"
#include "common/Logger.h"
#include <algorithm>

namespace SCXML {
namespace Core {

StateNode::StateNode(const std::string &id, SCXML::Type type)
    : id_(id), type_(type), parent_(nullptr), initialState_(""), onEntry_(""), onExit_(""),
      initialTransition_(nullptr) {
    SCXML::Common::Logger::debug("StateNode::Constructor - Creating state node: " + id +
                                 ", type: " + std::to_string(static_cast<int>(type)));
}

StateNode::~StateNode() {
    SCXML::Common::Logger::debug("StateNode::Destructor - Destroying state node: " + id_);
}

const std::string &StateNode::getId() const {
    return id_;
}

SCXML::Type StateNode::getType() const {
    return type_;
}

void StateNode::setParent(SCXML::Model::IStateNode *parent) {
    parent_ = parent;
    SCXML::Common::Logger::debug("StateNode::setParent() - Setting parent for " + id_ + ": " +
                                 (parent ? parent->getId() : "null"));
}

SCXML::Model::IStateNode *StateNode::getParent() const {
    return parent_;
}

void StateNode::addChild(std::shared_ptr<SCXML::Model::IStateNode> child) {
    if (child) {
        children_.push_back(child);
        child->setParent(this);
        SCXML::Common::Logger::debug("StateNode::addChild() - Adding child to " + id_ + ": " + child->getId());
    } else {
        SCXML::Common::Logger::warning("StateNode::addChild() - Attempt to add null child to " + id_);
    }
}

const std::vector<std::shared_ptr<SCXML::Model::IStateNode>> &StateNode::getChildren() const {
    return children_;
}

void StateNode::addTransition(std::shared_ptr<SCXML::Model::ITransitionNode> transition) {
    if (transition) {
        transitions_.push_back(transition);
        SCXML::Common::Logger::debug("StateNode::addTransition() - Adding transition to " + id_ +
                                     ": event=" + transition->getEvent() + ", target=" + transition->getTarget());
    } else {
        SCXML::Common::Logger::warning("StateNode::addTransition() - Attempt to add null transition to " + id_);
    }
}

const std::vector<std::shared_ptr<SCXML::Model::ITransitionNode>> &StateNode::getTransitions() const {
    return transitions_;
}

void StateNode::addDataItem(std::shared_ptr<SCXML::Model::IDataModelItem> dataItem) {
    if (dataItem) {
        dataItems_.push_back(dataItem);
        SCXML::Common::Logger::debug("StateNode::addDataItem() - Adding data item to " + id_ + ": " +
                                     dataItem->getId());
    } else {
        SCXML::Common::Logger::warning("StateNode::addDataItem() - Attempt to add null data item to " + id_);
    }
}

const std::vector<std::shared_ptr<SCXML::Model::IDataModelItem>> &StateNode::getDataItems() const {
    return dataItems_;
}

void StateNode::setInitialState(const std::string &initialState) {
    initialState_ = initialState;
    SCXML::Common::Logger::debug("StateNode::setInitialState() - Setting initial state for " + id_ + ": " +
                                 initialState);
}

const std::string &StateNode::getInitialState() const {
    return initialState_;
}

void StateNode::setOnEntry(const std::string &callback) {
    onEntry_ = callback;
    SCXML::Common::Logger::debug("StateNode::setOnEntry() - Setting onEntry callback for " + id_ + ": " + callback);
}

const std::string &StateNode::getOnEntry() const {
    return onEntry_;
}

void StateNode::setOnExit(const std::string &callback) {
    onExit_ = callback;
    SCXML::Common::Logger::debug("StateNode::setOnExit() - Setting onExit callback for " + id_ + ": " + callback);
}

const std::string &StateNode::getOnExit() const {
    return onExit_;
}

void StateNode::addEntryAction(const std::string &actionId) {
    entryActions_.push_back(actionId);
    SCXML::Common::Logger::debug("StateNode::addEntryAction() - Adding entry action to " + id_ + ": " + actionId);
}

const std::vector<std::string> &StateNode::getEntryActions() const {
    return entryActions_;
}

void StateNode::clearEntryActions() {
    entryActions_.clear();
}

void StateNode::addExitAction(const std::string &actionId) {
    exitActions_.push_back(actionId);
    SCXML::Common::Logger::debug("StateNode::addExitAction() - Adding exit action to " + id_ + ": " + actionId);
}

const std::vector<std::string> &StateNode::getExitActions() const {
    return exitActions_;
}

void StateNode::clearExitActions() {
    exitActions_.clear();
}

void StateNode::addInvoke(std::shared_ptr<SCXML::Model::IInvokeNode> invoke) {
    if (invoke) {
        invokes_.push_back(invoke);
        SCXML::Common::Logger::debug("StateNode::addInvoke() - Adding invoke to " + id_ + ": " + invoke->getId());
    } else {
        SCXML::Common::Logger::warning("StateNode::addInvoke() - Attempt to add null invoke to " + id_);
    }
}

const std::vector<std::shared_ptr<SCXML::Model::IInvokeNode>> &StateNode::getInvokes() const {
    return invokes_;
}

bool StateNode::isCompound() const {
    return !children_.empty();
}

bool StateNode::isAtomic() const {
    return children_.empty();
}

bool StateNode::isFinal() const {
    return type_ == SCXML::Type::Final;
}

bool StateNode::isParallel() const {
    return type_ == SCXML::Type::Parallel;
}

bool StateNode::isHistory() const {
    return type_ == SCXML::Type::History || type_ == SCXML::Type::DeepHistory;
}

std::shared_ptr<SCXML::Model::IStateNode> StateNode::findChild(const std::string &id) const {
    auto it =
        std::find_if(children_.begin(), children_.end(), [&id](const std::shared_ptr<SCXML::Model::IStateNode> &child) {
            return child && child->getId() == id;
        });
    return (it != children_.end()) ? *it : nullptr;
}

void StateNode::setDoneDataContent(const std::string &content) {
    doneDataContent_ = content;
    SCXML::Common::Logger::debug("StateNode::setDoneDataContent() - Setting donedata content for " + id_);
}

void StateNode::addDoneDataParam(const std::string &name, const std::string &expr) {
    doneDataParams_[name] = expr;
    SCXML::Common::Logger::debug("StateNode::addDoneDataParam() - Adding param to donedata for " + id_ + ": " + name +
                                 " -> " + expr);
}

const std::string &StateNode::getDoneDataContent() const {
    return doneDataContent_;
}

const std::map<std::string, std::string> &StateNode::getDoneDataParams() const {
    return doneDataParams_;
}

void StateNode::setInitialTransition(std::shared_ptr<SCXML::Model::ITransitionNode> transition) {
    initialTransition_ = transition;
    SCXML::Common::Logger::debug("StateNode::setInitialTransition() - Setting initial transition for " + id_);
}

std::shared_ptr<SCXML::Model::ITransitionNode> StateNode::getInitialTransition() const {
    return initialTransition_;
}

void StateNode::addEntryActionNode(std::shared_ptr<SCXML::Model::IActionNode> actionNode) {
    if (actionNode) {
        entryActionNodes_.push_back(actionNode);
        SCXML::Common::Logger::debug("StateNode::addEntryActionNode() - Added entry ActionNode: " +
                                     actionNode->getId() + " to StateNode: " + id_);
    }
}

void StateNode::addExitActionNode(std::shared_ptr<SCXML::Model::IActionNode> actionNode) {
    if (actionNode) {
        exitActionNodes_.push_back(actionNode);
        SCXML::Common::Logger::debug("StateNode::addExitActionNode() - Added exit ActionNode: " + actionNode->getId() +
                                     " to StateNode: " + id_);
    }
}

const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &StateNode::getEntryActionNodes() const {
    return entryActionNodes_;
}

const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &StateNode::getExitActionNodes() const {
    return exitActionNodes_;
}

std::shared_ptr<SCXML::Model::IStateNode> StateNode::clone() const {
    auto cloned = std::make_shared<StateNode>(id_, type_);
    cloned->initialState_ = initialState_;
    cloned->onEntry_ = onEntry_;
    cloned->onExit_ = onExit_;
    cloned->entryActions_ = entryActions_;
    cloned->exitActions_ = exitActions_;
    cloned->doneDataContent_ = doneDataContent_;
    cloned->doneDataParams_ = doneDataParams_;
    // Note: children_, transitions_, dataItems_, invokes_, initialTransition_ are not deep-cloned
    // This would require a more complex implementation
    return cloned;
}

}  // namespace Core
}  // namespace SCXML