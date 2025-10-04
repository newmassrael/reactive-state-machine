#include "model/StateNode.h"
#include "common/Logger.h"

RSM::StateNode::StateNode(const std::string &id, Type type) : id_(id), type_(type), parent_(nullptr) {
    LOG_DEBUG("Creating state node: {}, type: {}", id, static_cast<int>(type));
}

RSM::StateNode::~StateNode() {
    LOG_DEBUG("Destroying state node: {}", id_);
}

const std::string &RSM::StateNode::getId() const {
    return id_;
}

RSM::Type RSM::StateNode::getType() const {
    return type_;
}

void RSM::StateNode::setParent(RSM::IStateNode *parent) {
    LOG_DEBUG("Setting parent for {}: {}", id_, (parent ? parent->getId() : "null"));
    parent_ = parent;
}

RSM::IStateNode *RSM::StateNode::getParent() const {
    return parent_;
}

void RSM::StateNode::addChild(std::shared_ptr<RSM::IStateNode> child) {
    if (child) {
        LOG_DEBUG("Adding child to {}: {}", id_, child->getId());
        children_.push_back(child);
    } else {
        LOG_WARN("Attempt to add null child to {}", id_);
    }
}

const std::vector<std::shared_ptr<RSM::IStateNode>> &RSM::StateNode::getChildren() const {
    return children_;
}

void RSM::StateNode::addTransition(std::shared_ptr<RSM::ITransitionNode> transition) {
    if (transition) {
        const auto targets = transition->getTargets();
        std::string targetStr = targets.empty() ? "" : (targets.size() == 1 ? targets[0] : "[multiple targets]");

        LOG_DEBUG("Adding transition to {}: event={}, target={}", id_, transition->getEvent(), targetStr);
        transitions_.push_back(transition);
    } else {
        LOG_WARN("Attempt to add null transition to {}", id_);
    }
}

const std::vector<std::shared_ptr<RSM::ITransitionNode>> &RSM::StateNode::getTransitions() const {
    return transitions_;
}

// Implementation of newly added data model related methods
void RSM::StateNode::addDataItem(std::shared_ptr<RSM::IDataModelItem> dataItem) {
    if (dataItem) {
        LOG_DEBUG("Adding data item to {}: {}", id_, dataItem->getId());
        dataItems_.push_back(dataItem);
    } else {
        LOG_WARN("Attempt to add null data item to {}", id_);
    }
}

const std::vector<std::shared_ptr<RSM::IDataModelItem>> &RSM::StateNode::getDataItems() const {
    return dataItems_;
}

void RSM::StateNode::setInitialState(const std::string &initialState) {
    LOG_DEBUG("Setting initial state for {}: {}", id_, initialState);
    initialState_ = initialState;
}

const std::string &RSM::StateNode::getInitialState() const {
    return initialState_;
}

void RSM::StateNode::setOnEntry(const std::string &callback) {
    LOG_DEBUG("Setting onEntry callback for {}: {}", id_, callback);
    onEntry_ = callback;
}

const std::string &RSM::StateNode::getOnEntry() const {
    return onEntry_;
}

void RSM::StateNode::setOnExit(const std::string &callback) {
    LOG_DEBUG("Setting onExit callback for {}: {}", id_, callback);
    onExit_ = callback;
}

const std::string &RSM::StateNode::getOnExit() const {
    return onExit_;
}

void RSM::StateNode::addInvoke(std::shared_ptr<RSM::IInvokeNode> invoke) {
    if (invoke) {
        LOG_DEBUG("Adding invoke to {}: {}", id_, invoke->getId());
        invokes_.push_back(invoke);
    } else {
        LOG_WARN("Attempt to add null invoke to {}", id_);
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
    LOG_DEBUG("Setting donedata content for {}", id_);
    doneData_.setContent(content);
}

void RSM::StateNode::addDoneDataParam(const std::string &name, const std::string &location) {
    LOG_DEBUG("Adding param to donedata for {}: {} -> {}", id_, name, location);
    doneData_.addParam(name, location);
}

void RSM::StateNode::clearDoneDataParams() {
    doneData_.clearParams();
}

std::shared_ptr<RSM::ITransitionNode> RSM::StateNode::getInitialTransition() const {
    return initialTransition_;
}

void RSM::StateNode::setInitialTransition(std::shared_ptr<RSM::ITransitionNode> transition) {
    LOG_DEBUG("Setting initial transition for {}", id_);
    initialTransition_ = transition;
}

// W3C SCXML 3.8/3.9: Block-based action methods
void RSM::StateNode::addEntryActionBlock(std::vector<std::shared_ptr<RSM::IActionNode>> block) {
    if (!block.empty()) {
        LOG_DEBUG("W3C SCXML 3.8: Adding entry action block to {} with {} actions", id_, block.size());
        entryActionBlocks_.push_back(std::move(block));
    } else {
        LOG_WARN("Attempted to add empty entry action block to {}", id_);
    }
}

const std::vector<std::vector<std::shared_ptr<RSM::IActionNode>>> &RSM::StateNode::getEntryActionBlocks() const {
    return entryActionBlocks_;
}

void RSM::StateNode::addExitActionBlock(std::vector<std::shared_ptr<RSM::IActionNode>> block) {
    if (!block.empty()) {
        LOG_DEBUG("W3C SCXML 3.9: Adding exit action block to {} with {} actions", id_, block.size());
        exitActionBlocks_.push_back(std::move(block));
    } else {
        LOG_WARN("Attempted to add empty exit action block to {}", id_);
    }
}

const std::vector<std::vector<std::shared_ptr<RSM::IActionNode>>> &RSM::StateNode::getExitActionBlocks() const {
    return exitActionBlocks_;
}
