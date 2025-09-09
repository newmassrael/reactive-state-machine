#pragma once

#include "ITransitionNode.h"
#include <gmock/gmock.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

class MockTransitionNode : public SCXML::Model::ITransitionNode {
public:
    MOCK_CONST_METHOD0(getEvent, const std::string &());
    MOCK_CONST_METHOD0(getTargets, std::vector<std::string>());
    MOCK_METHOD1(addTarget, void(const std::string &));
    MOCK_METHOD0(clearTargets, void());
    MOCK_CONST_METHOD0(hasTargets, bool());
    MOCK_METHOD1(setGuard, void(const std::string &));
    MOCK_CONST_METHOD0(getGuard, const std::string &());
    MOCK_METHOD1(addAction, void(const std::string &));
    MOCK_CONST_METHOD0(getActions, const std::vector<std::string> &());
    MOCK_METHOD1(setReactive, void(bool));
    MOCK_CONST_METHOD0(isReactive, bool());
    MOCK_METHOD1(setInternal, void(bool));
    MOCK_CONST_METHOD0(isInternal, bool());
    MOCK_METHOD2(setAttribute, void(const std::string &, const std::string &));
    MOCK_CONST_METHOD1(getAttribute, std::string(const std::string &));
    MOCK_METHOD1(addEvent, void(const std::string &));
    MOCK_CONST_METHOD0(getEvents, const std::vector<std::string> &());

    // New ActionNode methods
    MOCK_METHOD1(addActionNode, void(std::shared_ptr<SCXML::Model::IActionNode>));
    MOCK_CONST_METHOD0(getActionNodes, const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &());

    // Missing methods that were causing abstract class errors
    MOCK_CONST_METHOD0(getCond, const std::string &());
    MOCK_METHOD1(setCond, void(const std::string &));
    MOCK_CONST_METHOD0(getType, std::string());
    MOCK_METHOD1(setType, void(const std::string &));
    MOCK_CONST_METHOD0(getDocumentOrder, int());
    MOCK_METHOD1(setDocumentOrder, void(int));
    MOCK_CONST_METHOD0(getExecutableContent, std::vector<std::shared_ptr<SCXML::Model::IActionNode>>());
    MOCK_CONST_METHOD0(getSource, std::string());
    MOCK_METHOD1(setSource, void(const std::string &));
    MOCK_CONST_METHOD0(getTarget, std::string());

    // 멤버 변수들
    std::string event_;
    std::vector<std::string> targets_;
    std::string guard_;
    std::vector<std::string> actions_;
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>> actionNodes_;
    std::vector<std::string> events_;
    bool isReactive_ = false;
    bool isInternal_ = false;
    std::unordered_map<std::string, std::string> attributes_;
    std::string cond_;
    std::string type_ = "external";
    int documentOrder_ = 0;
    std::string source_;
    std::string target_;

    // 기본 동작 설정 메서드
    void SetupDefaultBehavior() {
        std::cout << "Setting up default behavior for MockTransitionNode" << std::endl;

        // 기본 동작 정의
        ON_CALL(*this, getEvent()).WillByDefault(testing::ReturnRef(event_));
        ON_CALL(*this, hasTargets()).WillByDefault(testing::Invoke(this, &MockTransitionNode::getHasTargets));
        ON_CALL(*this, getGuard()).WillByDefault(testing::ReturnRef(guard_));
        ON_CALL(*this, getActions()).WillByDefault(testing::ReturnRef(actions_));
        ON_CALL(*this, isReactive()).WillByDefault(testing::Invoke(this, &MockTransitionNode::getIsReactive));
        ON_CALL(*this, isInternal()).WillByDefault(testing::Invoke(this, &MockTransitionNode::getIsInternal));
        ON_CALL(*this, getEvents()).WillByDefault(testing::ReturnRef(events_));
        ON_CALL(*this, getActionNodes()).WillByDefault(testing::ReturnRef(actionNodes_));

        // 메서드 호출 시 멤버 변수 업데이트 및 로깅 추가
        ON_CALL(*this, addTarget(testing::_)).WillByDefault([this](const std::string &target) {
            std::cout << "addTarget called with: " << target << std::endl;
            if (!target.empty()) {
                this->targets_.push_back(target);
            }
        });
        ON_CALL(*this, clearTargets()).WillByDefault([this]() {
            std::cout << "clearTargets called - before clear" << std::endl;
            // 타겟 벡터를 명확하게 비우기
            this->targets_.clear();
            std::cout << "clearTargets called - after clear: targets_.size() = " << this->targets_.size() << std::endl;
        });
        ON_CALL(*this, setGuard(testing::_)).WillByDefault([this](const std::string &guard) {
            std::cout << "setGuard called with: " << guard << std::endl;
            this->guard_ = guard;
        });
        ON_CALL(*this, getTargets()).WillByDefault([this]() {
            std::cout << "getTargets called - targets: [";
            for (const auto &t : this->targets_) {
                std::cout << "'" << t << "', ";
            }
            std::cout << "] size=" << this->targets_.size() << std::endl;
            return this->targets_;
        });
        ON_CALL(*this, addAction(testing::_)).WillByDefault([this](const std::string &action) {
            std::cout << "addAction called with: " << action << std::endl;
            this->actions_.push_back(action);
        });
        ON_CALL(*this, setReactive(testing::_)).WillByDefault([this](bool reactive) {
            std::cout << "setReactive called with: " << (reactive ? "true" : "false") << std::endl;
            this->isReactive_ = reactive;
        });
        ON_CALL(*this, setInternal(testing::_)).WillByDefault([this](bool internal) {
            std::cout << "setInternal called with: " << (internal ? "true" : "false") << std::endl;
            this->isInternal_ = internal;
        });
        ON_CALL(*this, setAttribute(testing::_, testing::_))
            .WillByDefault([this](const std::string &key, const std::string &value) {
                std::cout << "setAttribute called with key: " << key << ", value: " << value << std::endl;
                this->attributes_[key] = value;
            });
        ON_CALL(*this, getAttribute(testing::_)).WillByDefault([this](const std::string &key) {
            std::cout << "getAttribute called with key: " << key << std::endl;
            auto it = attributes_.find(key);
            return (it != attributes_.end()) ? it->second : std::string();
        });
        ON_CALL(*this, addEvent(testing::_)).WillByDefault([this](const std::string &event) {
            std::cout << "addEvent called with: " << event << std::endl;
            this->events_.push_back(event);
        });
        ON_CALL(*this, addActionNode(testing::_))
            .WillByDefault([this](std::shared_ptr<SCXML::Model::IActionNode> actionNode) {
                if (actionNode) {
                    std::cout << "addActionNode called with: " << actionNode->getId() << std::endl;
                    this->actionNodes_.push_back(actionNode);
                    // Maintain backward compatibility like real TransitionNode
                    this->actions_.push_back(actionNode->getId());
                    std::cout << "  -> Also added to actions_: " << actionNode->getId() << std::endl;
                }
            });

        // Default behavior for missing methods
        ON_CALL(*this, getCond()).WillByDefault([this]() -> const std::string & { return cond_; });

        ON_CALL(*this, setCond(testing::_)).WillByDefault([this](const std::string &cond) { cond_ = cond; });

        ON_CALL(*this, getType()).WillByDefault([this]() { return type_; });

        ON_CALL(*this, setType(testing::_)).WillByDefault([this](const std::string &type) { type_ = type; });

        ON_CALL(*this, getDocumentOrder()).WillByDefault([this]() { return documentOrder_; });

        ON_CALL(*this, setDocumentOrder(testing::_)).WillByDefault([this](int order) { documentOrder_ = order; });

        ON_CALL(*this, getExecutableContent()).WillByDefault([this]() { return actionNodes_; });

        ON_CALL(*this, getSource()).WillByDefault([this]() { return source_; });

        ON_CALL(*this, setSource(testing::_)).WillByDefault([this](const std::string &source) { source_ = source; });

        ON_CALL(*this, getTarget()).WillByDefault([this]() { return target_; });
    }

    bool getIsInternal() const {
        return isInternal_;
    }

    bool getIsReactive() const {
        return isReactive_;
    }

    bool getHasTargets() const {
        return !targets_.empty();
    }
};
