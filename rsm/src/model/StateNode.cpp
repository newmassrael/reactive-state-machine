#include "model/StateNode.h"
#include "common/Logger.h"

RSM::StateNode::StateNode(const std::string &id, Type type) : id_(id), type_(type), parent_(nullptr) {
    Logger::debug("RSM::StateNode::Constructor - Creating state node: " + id +
                  ", type: " + std::to_string(static_cast<int>(type)));
}

RSM::StateNode::~StateNode() {
    Logger::debug("RSM::StateNode::Destructor - Destroying state node: " + id_);
}

const std::string &RSM::StateNode::getId() const {
    return id_;
}

RSM::Type RSM::StateNode::getType() const {
    return type_;
}

void RSM::StateNode::setParent(RSM::IStateNode *parent) {
    Logger::debug("RSM::StateNode::setParent() - Setting parent for " + id_ + ": " +
                  (parent ? parent->getId() : "null"));
    parent_ = parent;
}

RSM::IStateNode *RSM::StateNode::getParent() const {
    return parent_;
}

void RSM::StateNode::addChild(std::shared_ptr<RSM::IStateNode> child) {
    if (child) {
        Logger::debug("RSM::StateNode::addChild() - Adding child to " + id_ + ": " + child->getId());
        children_.push_back(child);
    } else {
        Logger::warn("RSM::StateNode::addChild() - Attempt to add null child to " + id_);
    }
}

const std::vector<std::shared_ptr<RSM::IStateNode>> &RSM::StateNode::getChildren() const {
    return children_;
}

void RSM::StateNode::addTransition(std::shared_ptr<RSM::ITransitionNode> transition) {
    if (transition) {
        const auto targets = transition->getTargets();
        std::string targetStr = targets.empty() ? "" : (targets.size() == 1 ? targets[0] : "[multiple targets]");

        Logger::debug("RSM::StateNode::addTransition() - Adding transition to " + id_ +
                      ": event=" + transition->getEvent() + ", target=" + targetStr);
        transitions_.push_back(transition);
    } else {
        Logger::warn("RSM::StateNode::addTransition() - Attempt to add null transition to " + id_);
    }
}

const std::vector<std::shared_ptr<RSM::ITransitionNode>> &RSM::StateNode::getTransitions() const {
    return transitions_;
}

// 새로 추가된 데이터 모델 관련 메서드 구현
void RSM::StateNode::addDataItem(std::shared_ptr<RSM::IDataModelItem> dataItem) {
    if (dataItem) {
        Logger::debug("RSM::StateNode::addDataItem() - Adding data item to " + id_ + ": " + dataItem->getId());
        dataItems_.push_back(dataItem);
    } else {
        Logger::warn("RSM::StateNode::addDataItem() - Attempt to add null data item to " + id_);
    }
}

const std::vector<std::shared_ptr<RSM::IDataModelItem>> &RSM::StateNode::getDataItems() const {
    return dataItems_;
}

void RSM::StateNode::setInitialState(const std::string &initialState) {
    Logger::debug("RSM::StateNode::setInitialState() - Setting initial state for " + id_ + ": " + initialState);
    initialState_ = initialState;
}

const std::string &RSM::StateNode::getInitialState() const {
    return initialState_;
}

void RSM::StateNode::setOnEntry(const std::string &callback) {
    Logger::debug("RSM::StateNode::setOnEntry() - Setting onEntry callback for " + id_ + ": " + callback);
    onEntry_ = callback;
}

const std::string &RSM::StateNode::getOnEntry() const {
    return onEntry_;
}

void RSM::StateNode::setOnExit(const std::string &callback) {
    Logger::debug("RSM::StateNode::setOnExit() - Setting onExit callback for " + id_ + ": " + callback);
    onExit_ = callback;
}

const std::string &RSM::StateNode::getOnExit() const {
    return onExit_;
}

void RSM::StateNode::addEntryAction(const std::string &actionId) {
    Logger::debug("RSM::StateNode::addEntryAction() - Adding entry action to " + id_ + ": " + actionId);
    entryActions_.push_back(actionId);

    // onEntry_ 업데이트
    if (onEntry_.empty()) {
        onEntry_ = actionId;
    } else {
        onEntry_ += ";" + actionId;
    }
}

void RSM::StateNode::addExitAction(const std::string &actionId) {
    Logger::debug("RSM::StateNode::addExitAction() - Adding exit action to " + id_ + ": " + actionId);
    exitActions_.push_back(actionId);

    // onExit_ 업데이트
    if (onExit_.empty()) {
        onExit_ = actionId;
    } else {
        onExit_ += ";" + actionId;
    }
}

void RSM::StateNode::addInvoke(std::shared_ptr<RSM::IInvokeNode> invoke) {
    if (invoke) {
        Logger::debug("RSM::StateNode::addInvoke() - Adding invoke to " + id_ + ": " + invoke->getId());
        invokes_.push_back(invoke);
    } else {
        Logger::warn("RSM::StateNode::addInvoke() - Attempt to add null invoke to " + id_);
    }
}

const std::vector<std::shared_ptr<RSM::IInvokeNode>> &RSM::StateNode::getInvoke() const {
    return invokes_;
}

void RSM::StateNode::addReactiveGuard(const std::string &guardId) {
    reactiveGuards_.push_back(guardId);
}

const std::vector<std::string> &RSM::StateNode::getReactiveGuards() const {
    return reactiveGuards_;
}

bool RSM::StateNode::isFinalState() const {
    return type_ == Type::FINAL;
}

const RSM::DoneData &RSM::StateNode::getDoneData() const {
    return doneData_;
}

RSM::DoneData &RSM::StateNode::getDoneData() {
    return doneData_;
}

void RSM::StateNode::setDoneDataContent(const std::string &content) {
    Logger::debug("RSM::StateNode::setDoneDataContent() - Setting donedata content for " + id_);
    doneData_.setContent(content);
}

void RSM::StateNode::addDoneDataParam(const std::string &name, const std::string &location) {
    Logger::debug("RSM::StateNode::addDoneDataParam() - Adding param to donedata for " + id_ + ": " + name + " -> " +
                  location);
    doneData_.addParam(name, location);
}

void RSM::StateNode::clearDoneDataParams() {
    doneData_.clearParams();
}

std::shared_ptr<RSM::ITransitionNode> RSM::StateNode::getInitialTransition() const {
    return initialTransition_;
}

void RSM::StateNode::setInitialTransition(std::shared_ptr<RSM::ITransitionNode> transition) {
    Logger::debug("RSM::StateNode::setInitialTransition() - Setting initial "
                  "transition for " +
                  id_);
    initialTransition_ = transition;
}
