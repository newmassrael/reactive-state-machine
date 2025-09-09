#include "core/StateNode.h"
#include "common/Logger.h"
#include "core/StateNode.h"

namespace SCXML {
namespace Core {

StateNode::StateNode(const std::string &id, Type type) : id_(id), type_(type), parent_(nullptr) {
    SCXML::Common::Logger::debug("StateNode::Constructor - Creating state node: " + id +
                                 ", type: " + std::to_string(static_cast<int>(type)));
}

StateNode::~StateNode() {
    SCXML::Common::Logger::debug("StateNode::Destructor - Destroying state node: " + id_);
}

const std::string &StateNode::getId() const {
    return id_;
}

Type StateNode::getType() const {
    return type_;
}

void StateNode::setParent(IStateNode *parent) {
    SCXML::Common::Logger::debug("StateNode::setParent() - Setting parent for " + id_ + ": " +
                                 (parent ? parent->getId() : "null"));
    parent_ = parent;
}

IStateNode *StateNode::getParent() const {
    return parent_;
}

void StateNode::addChild(std::shared_ptr<SCXML::Model::IStateNode> child) {
    if (child) {
        SCXML::Common::Logger::debug("StateNode::addChild() - Adding child to " + id_ + ": " + child->getId());
        children_.push_back(child);
    } else {
        SCXML::Common::Logger::warning("StateNode::addChild() - Attempt to add null child to " + id_);
    }
}

const std::vector<std::shared_ptr<SCXML::Model::IStateNode>> &StateNode::getChildren() const {
    return children_;
}

void StateNode::addTransition(std::shared_ptr<SCXML::Model::ITransitionNode> transition) {
    if (transition) {
        const auto targets = transition->getTargets();
        std::string targetStr = targets.empty() ? "" : (targets.size() == 1 ? targets[0] : "[multiple targets]");

        SCXML::Common::Logger::debug("StateNode::addTransition() - Adding transition to " + id_ +
                                     ": event=" + transition->getEvent() + ", target=" + targetStr);
        transitions_.push_back(transition);
    } else {
        SCXML::Common::Logger::warning("StateNode::addTransition() - Attempt to add null transition to " + id_);
    }
}

const std::vector<std::shared_ptr<SCXML::Model::ITransitionNode>> &StateNode::getTransitions() const {
    return transitions_;
}

// 새로 추가된 데이터 모델 관련 메서드 구현
void StateNode::addDataItem(std::shared_ptr<IDataModelItem> dataItem) {
    if (dataItem) {
        SCXML::Common::Logger::debug("StateNode::addDataItem() - Adding data item to " + id_ + ": " +
                                     dataItem->getId());
        dataItems_.push_back(dataItem);
    } else {
        SCXML::Common::Logger::warning("StateNode::addDataItem() - Attempt to add null data item to " + id_);
    }
}

const std::vector<std::shared_ptr<IDataModelItem>> &StateNode::getDataItems() const {
    return dataItems_;
}

void StateNode::setInitialState(const std::string &initialState) {
    SCXML::Common::Logger::debug("StateNode::setInitialState() - Setting initial state for " + id_ + ": " +
                                 initialState);
    initialState_ = initialState;
}

const std::string &StateNode::getInitialState() const {
    return initialState_;
}

void StateNode::setOnEntry(const std::string &callback) {
    SCXML::Common::Logger::debug("StateNode::setOnEntry() - Setting onEntry callback for " + id_ + ": " + callback);
    onEntry_ = callback;
}

const std::string &StateNode::getOnEntry() const {
    return onEntry_;
}

void StateNode::setOnExit(const std::string &callback) {
    SCXML::Common::Logger::debug("StateNode::setOnExit() - Setting onExit callback for " + id_ + ": " + callback);
    onExit_ = callback;
}

const std::string &StateNode::getOnExit() const {
    return onExit_;
}

void StateNode::addEntryAction(const std::string &actionId) {
    SCXML::Common::Logger::debug("StateNode::addEntryAction() - Adding entry action to " + id_ + ": " + actionId);
    entryActions_.push_back(actionId);

    // onEntry_ 업데이트
    if (onEntry_.empty()) {
        onEntry_ = actionId;
    } else {
        onEntry_ += ";" + actionId;
    }
}

void StateNode::addExitAction(const std::string &actionId) {
    SCXML::Common::Logger::debug("StateNode::addExitAction() - Adding exit action to " + id_ + ": " + actionId);
    exitActions_.push_back(actionId);

    // onExit_ 업데이트
    if (onExit_.empty()) {
        onExit_ = actionId;
    } else {
        onExit_ += ";" + actionId;
    }
}

void StateNode::addInvoke(std::shared_ptr<IInvokeNode> invoke) {
    if (invoke) {
        SCXML::Common::Logger::debug("StateNode::addInvoke() - Adding invoke to " + id_ + ": " + invoke->getId());
        invokes_.push_back(invoke);
    } else {
        SCXML::Common::Logger::warning("StateNode::addInvoke() - Attempt to add null invoke to " + id_);
    }
}

const std::vector<std::shared_ptr<IInvokeNode>> &StateNode::getInvoke() const {
    return invokes_;
}

void StateNode::addReactiveGuard(const std::string &guardId) {
    reactiveGuards_.push_back(guardId);
}

const std::vector<std::string> &StateNode::getReactiveGuards() const {
    return reactiveGuards_;
}

bool StateNode::isFinalState() const {
    return type_ == Type::FINAL;
}

const DoneData &StateNode::getDoneData() const {
    return doneData_;
}

DoneData &StateNode::getDoneData() {
    return doneData_;
}

void StateNode::setDoneDataContent(const std::string &content) {
    SCXML::Common::Logger::debug("StateNode::setDoneDataContent() - Setting donedata content for " + id_);
    doneData_.setContent(content);
}

void StateNode::addDoneDataParam(const std::string &name, const std::string &location) {
    SCXML::Common::Logger::debug("StateNode::addDoneDataParam() - Adding param to donedata for " + id_ + ": " + name +
                                 " -> " + location);
    doneData_.addParam(name, location);
}

void StateNode::clearDoneDataParams() {
    doneData_.clearParams();
}

std::shared_ptr<SCXML::Model::ITransitionNode> StateNode::getInitialTransition() const {
    return initialTransition_;
}

void StateNode::setInitialTransition(std::shared_ptr<SCXML::Model::ITransitionNode> transition) {
    SCXML::Common::Logger::debug("StateNode::setInitialTransition() - Setting initial transition for " + id_);
    initialTransition_ = transition;
}

const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &StateNode::getEntryActionNodes() const {
    return entryActionNodes_;
}

const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &StateNode::getExitActionNodes() const {
    return exitActionNodes_;
}

void StateNode::addEntryActionNode(std::shared_ptr<SCXML::Model::IActionNode> actionNode) {
    if (actionNode) {
        entryActionNodes_.push_back(actionNode);
        SCXML::Common::Logger::debug(
            "StateNode::addEntryActionNode() - Added entry ActionNode: " + actionNode->getId() + " to state: " + id_);
    }
}

void StateNode::addExitActionNode(std::shared_ptr<SCXML::Model::IActionNode> actionNode) {
    if (actionNode) {
        exitActionNodes_.push_back(actionNode);
        SCXML::Common::Logger::debug("StateNode::addExitActionNode() - Added exit ActionNode: " + actionNode->getId() +
                                     " to state: " + id_);
    }
}

int StateNode::getDocumentOrder() const {
    return documentOrder_;
}

void StateNode::setDocumentOrder(int order) {
    documentOrder_ = order;
}

std::vector<std::shared_ptr<SCXML::Model::IActionNode>> StateNode::getOnEntryActions() const {
    return entryActionNodes_;
}

std::vector<std::shared_ptr<SCXML::Model::IActionNode>> StateNode::getOnExitActions() const {
    return exitActionNodes_;
}

}  // namespace Core
}  // namespace SCXML
