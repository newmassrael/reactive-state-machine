#pragma once
#include "IActionNode.h"
#include "IStateNode.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

class MockStateNode : public SCXML::Model::IStateNode {
public:
    MOCK_CONST_METHOD0(getId, const std::string &());
    MOCK_CONST_METHOD0(getType, SCXML::Type());
    MOCK_METHOD1(setParent, void(SCXML::Model::IStateNode *));
    MOCK_CONST_METHOD0(getParent, SCXML::Model::IStateNode *());
    MOCK_METHOD1(addChild, void(std::shared_ptr<SCXML::Model::IStateNode>));
    MOCK_CONST_METHOD0(getChildren, const std::vector<std::shared_ptr<SCXML::Model::IStateNode>> &());
    MOCK_METHOD1(addTransition, void(std::shared_ptr<SCXML::Model::ITransitionNode>));
    MOCK_CONST_METHOD0(getTransitions, const std::vector<std::shared_ptr<SCXML::Model::ITransitionNode>> &());
    MOCK_METHOD1(addDataItem, void(std::shared_ptr<SCXML::Model::IDataModelItem>));
    MOCK_CONST_METHOD0(getDataItems, const std::vector<std::shared_ptr<SCXML::Model::IDataModelItem>> &());
    MOCK_METHOD1(setOnEntry, void(const std::string &));
    MOCK_CONST_METHOD0(getOnEntry, const std::string &());
    MOCK_METHOD1(setOnExit, void(const std::string &));
    MOCK_CONST_METHOD0(getOnExit, const std::string &());
    MOCK_METHOD1(setInitialState, void(const std::string &));
    MOCK_CONST_METHOD0(getInitialState, const std::string &());
    MOCK_METHOD1(addEntryAction, void(const std::string &));
    MOCK_METHOD1(addExitAction, void(const std::string &));
    MOCK_METHOD1(addInvoke, void(std::shared_ptr<SCXML::Model::IInvokeNode>));
    MOCK_CONST_METHOD0(getInvoke, const std::vector<std::shared_ptr<SCXML::Model::IInvokeNode>> &());
    MOCK_METHOD1(setHistoryType, void(bool));
    MOCK_CONST_METHOD0(getHistoryType, SCXML::HistoryType());
    MOCK_CONST_METHOD0(isShallowHistory, bool());
    MOCK_CONST_METHOD0(isDeepHistory, bool());
    MOCK_METHOD1(addReactiveGuard, void(const std::string &));
    MOCK_CONST_METHOD0(getReactiveGuards, const std::vector<std::string> &());
    MOCK_CONST_METHOD0(getEntryActions, const std::vector<std::string> &());
    MOCK_CONST_METHOD0(getExitActions, const std::vector<std::string> &());
    MOCK_CONST_METHOD0(isFinalState, bool());
    MOCK_CONST_METHOD0(getDoneData, const SCXML::Model::DoneData &());
    MOCK_METHOD0(getDoneData, SCXML::Model::DoneData &());
    MOCK_METHOD1(setDoneDataContent, void(const std::string &));
    MOCK_METHOD2(addDoneDataParam, void(const std::string &, const std::string &));
    MOCK_METHOD0(clearDoneDataParams, void());
    MOCK_METHOD1(setInitialTransition, void(std::shared_ptr<SCXML::Model::ITransitionNode>));
    MOCK_CONST_METHOD0(getInitialTransition, std::shared_ptr<SCXML::Model::ITransitionNode>());

    // New ActionNode methods
    MOCK_CONST_METHOD0(getEntryActionNodes, const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &());
    MOCK_CONST_METHOD0(getExitActionNodes, const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &());
    MOCK_METHOD1(addEntryActionNode, void(std::shared_ptr<SCXML::Model::IActionNode>));
    MOCK_METHOD1(addExitActionNode, void(std::shared_ptr<SCXML::Model::IActionNode>));

    // Missing methods that were causing abstract class errors
    MOCK_CONST_METHOD0(getDocumentOrder, int());
    MOCK_METHOD1(setDocumentOrder, void(int));
    MOCK_CONST_METHOD0(getOnEntryActions, std::vector<std::shared_ptr<SCXML::Model::IActionNode>>());
    MOCK_CONST_METHOD0(getOnExitActions, std::vector<std::shared_ptr<SCXML::Model::IActionNode>>());

    std::string id_;
    SCXML::Type type_;
    std::vector<std::shared_ptr<SCXML::Model::IStateNode>> children_;
    std::vector<std::shared_ptr<SCXML::Model::ITransitionNode>> transitions_;
    std::vector<std::shared_ptr<SCXML::Model::IDataModelItem>> dataItems_;
    std::shared_ptr<SCXML::Model::ITransitionNode> initialTransition_;
    std::string initialState_;
    std::string onEntry_;
    std::string onExit_;
    SCXML::Model::IStateNode *parent_;
    std::vector<std::string> entryActions_;
    std::vector<std::string> exitActions_;
    std::vector<std::shared_ptr<SCXML::Model::IInvokeNode>> invokes_;
    std::vector<std::string> reactiveGuards_;
    SCXML::HistoryType historyType_ = SCXML::HistoryType::NONE;
    bool isDeepHistory_ = false;
    bool isFinalState_ = false;
    SCXML::Model::DoneData doneData_;
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>> entryActionNodes_;
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>> exitActionNodes_;
    int documentOrder_ = 0;

    // 객체 생성 시 기본값 초기화
    MockStateNode() : parent_(nullptr) {}

    // 기본 동작 설정 메서드 추가
    void SetupDefaultBehavior() {
        // 기본 동작 설정
        ON_CALL(*this, getId()).WillByDefault(testing::ReturnRef(id_));

        ON_CALL(*this, getType()).WillByDefault(testing::Return(type_));

        ON_CALL(*this, getParent()).WillByDefault(testing::Return(parent_));

        ON_CALL(*this, getChildren()).WillByDefault(testing::ReturnRef(children_));

        ON_CALL(*this, getTransitions()).WillByDefault(testing::ReturnRef(transitions_));

        ON_CALL(*this, getDataItems()).WillByDefault(testing::ReturnRef(dataItems_));

        ON_CALL(*this, getInitialState()).WillByDefault(testing::ReturnRef(initialState_));

        ON_CALL(*this, getOnEntry()).WillByDefault(testing::ReturnRef(onEntry_));

        ON_CALL(*this, getOnExit()).WillByDefault(testing::ReturnRef(onExit_));

        // 반응형 가드 관련 메서드 설정
        ON_CALL(*this, getReactiveGuards()).WillByDefault(testing::ReturnRef(reactiveGuards_));

        // 메서드 호출 시 내부 상태 변경
        ON_CALL(*this, setParent(testing::_)).WillByDefault([this](IStateNode *parent) { parent_ = parent; });

        ON_CALL(*this, addChild(testing::_)).WillByDefault([this](std::shared_ptr<IStateNode> child) {
            children_.push_back(child);
        });

        ON_CALL(*this, addTransition(testing::_))
            .WillByDefault([this](std::shared_ptr<SCXML::Model::ITransitionNode> transition) {
                transitions_.push_back(transition);
            });

        ON_CALL(*this, addDataItem(testing::_))
            .WillByDefault(
                [this](std::shared_ptr<SCXML::Model::IDataModelItem> dataItem) { dataItems_.push_back(dataItem); });

        ON_CALL(*this, addReactiveGuard(testing::_)).WillByDefault([this](const std::string &guardId) {
            reactiveGuards_.push_back(guardId);
        });

        ON_CALL(*this, setInitialState(testing::_)).WillByDefault([this](const std::string &state) {
            initialState_ = state;
        });

        ON_CALL(*this, setOnEntry(testing::_)).WillByDefault([this](const std::string &entry) { onEntry_ = entry; });

        ON_CALL(*this, setOnExit(testing::_)).WillByDefault([this](const std::string &exit) { onExit_ = exit; });

        ON_CALL(*this, getEntryActions()).WillByDefault(testing::ReturnRef(entryActions_));

        ON_CALL(*this, getExitActions()).WillByDefault(testing::ReturnRef(exitActions_));

        ON_CALL(*this, addEntryAction(testing::_)).WillByDefault([this](const std::string &action) {
            entryActions_.push_back(action);
            if (onEntry_.empty()) {
                onEntry_ = action;
            } else {
                onEntry_ += ";" + action;
            }
        });

        ON_CALL(*this, addExitAction(testing::_)).WillByDefault([this](const std::string &action) {
            exitActions_.push_back(action);
            if (onExit_.empty()) {
                onExit_ = action;
            } else {
                onExit_ += ";" + action;
            }
        });

        ON_CALL(*this, addInvoke(testing::_)).WillByDefault([this](std::shared_ptr<SCXML::Model::IInvokeNode> invoke) {
            invokes_.push_back(invoke);
        });

        ON_CALL(*this, getInvoke()).WillByDefault(testing::ReturnRef(invokes_));

        ON_CALL(*this, setHistoryType(testing::_)).WillByDefault([this](bool isDeep) {
            historyType_ = isDeep ? SCXML::HistoryType::DEEP : SCXML::HistoryType::SHALLOW;
            isDeepHistory_ = isDeep;
        });

        ON_CALL(*this, getHistoryType()).WillByDefault(testing::Return(historyType_));

        ON_CALL(*this, isShallowHistory()).WillByDefault([this]() {
            return historyType_ == SCXML::HistoryType::SHALLOW;
        });

        ON_CALL(*this, isDeepHistory()).WillByDefault([this]() { return historyType_ == SCXML::HistoryType::DEEP; });

        ON_CALL(*this, isFinalState()).WillByDefault([this]() {
            return (type_ == SCXML::Type::FINAL) || isFinalState_;
        });

        ON_CALL(*this, getDoneData()).WillByDefault(testing::ReturnRef(doneData_));

        ON_CALL(*this, setDoneDataContent(testing::_)).WillByDefault([this](const std::string &content) {
            doneData_.setContent(content);
        });

        ON_CALL(*this, addDoneDataParam(testing::_, testing::_))
            .WillByDefault(
                [this](const std::string &name, const std::string &location) { doneData_.addParam(name, location); });

        ON_CALL(*this, clearDoneDataParams()).WillByDefault([this]() { doneData_.clearParams(); });

        ON_CALL(*this, setInitialTransition(testing::_))
            .WillByDefault(
                [this](std::shared_ptr<SCXML::Model::ITransitionNode> transition) { initialTransition_ = transition; });

        ON_CALL(*this, getInitialTransition()).WillByDefault([this]() { return initialTransition_; });

        // New ActionNode methods default behavior
        ON_CALL(*this, getEntryActionNodes()).WillByDefault(testing::ReturnRef(entryActionNodes_));

        ON_CALL(*this, getExitActionNodes()).WillByDefault(testing::ReturnRef(exitActionNodes_));

        ON_CALL(*this, addEntryActionNode(testing::_))
            .WillByDefault([this](std::shared_ptr<SCXML::Model::IActionNode> actionNode) {
                entryActionNodes_.push_back(actionNode);
            });

        ON_CALL(*this, addExitActionNode(testing::_))
            .WillByDefault([this](std::shared_ptr<SCXML::Model::IActionNode> actionNode) {
                exitActionNodes_.push_back(actionNode);
            });

        // Default behavior for missing methods
        ON_CALL(*this, getDocumentOrder()).WillByDefault([this]() { return documentOrder_; });

        ON_CALL(*this, setDocumentOrder(testing::_)).WillByDefault([this](int order) { documentOrder_ = order; });

        ON_CALL(*this, getOnEntryActions()).WillByDefault([this]() { return entryActionNodes_; });

        ON_CALL(*this, getOnExitActions()).WillByDefault([this]() { return exitActionNodes_; });
    }
};
