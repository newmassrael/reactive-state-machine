#pragma once

#include "common/Logger.h"
#include "model/IActionNode.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class MockActionNode : public SCXML::Model::IActionNode {
public:
    MOCK_CONST_METHOD0(getId, const std::string &());
    MOCK_METHOD1(setExternalClass, void(const std::string &));
    MOCK_CONST_METHOD0(getExternalClass, const std::string &());
    MOCK_METHOD1(setExternalFactory, void(const std::string &));
    MOCK_CONST_METHOD0(getExternalFactory, const std::string &());
    MOCK_METHOD1(setType, void(const std::string &));
    MOCK_CONST_METHOD0(getType, const std::string &());
    MOCK_METHOD2(setAttribute, void(const std::string &, const std::string &));
    MOCK_CONST_METHOD1(getAttribute, const std::string &(const std::string &));
    MOCK_CONST_METHOD0(getAttributes, const std::unordered_map<std::string, std::string> &());
    MOCK_METHOD1(addChildAction, void(std::shared_ptr<SCXML::Model::IActionNode>));
    MOCK_METHOD1(setChildActions, void(const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &));
    MOCK_CONST_METHOD0(getChildActions, const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &());
    MOCK_CONST_METHOD0(hasChildActions, bool());
    MOCK_METHOD1(execute, bool(SCXML::Runtime::RuntimeContext &));
    MOCK_CONST_METHOD0(clone, std::shared_ptr<SCXML::Model::IActionNode>());
    MOCK_CONST_METHOD0(getActionType, std::string());

    std::string id_;
    std::string externalClass_;
    std::string externalFactory_;
    std::string type_;
    std::unordered_map<std::string, std::string> attributes_;
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>> childActions_;
    std::string emptyString_;

    // 기본 동작 설정 메서드
    void SetupDefaultBehavior() {
        SCXML::Common::Logger::debug("MockActionNode::SetupDefaultBehavior - Setting up default behavior");

        // 기본 동작 정의
        ON_CALL(*this, getId()).WillByDefault(testing::ReturnRef(id_));
        ON_CALL(*this, getExternalClass()).WillByDefault(testing::ReturnRef(externalClass_));
        ON_CALL(*this, getExternalFactory()).WillByDefault(testing::ReturnRef(externalFactory_));
        ON_CALL(*this, getType()).WillByDefault(testing::ReturnRef(type_));
        ON_CALL(*this, getAttributes()).WillByDefault(testing::ReturnRef(attributes_));
        ON_CALL(*this, getAttribute(testing::_))
            .WillByDefault(testing::Invoke([this](const std::string &key) -> const std::string & {
                SCXML::Common::Logger::debug("MockActionNode::getAttribute - Called with key: " + key);
                auto it = attributes_.find(key);
                return (it != attributes_.end()) ? it->second : emptyString_;
            }));
        ON_CALL(*this, getChildActions()).WillByDefault(testing::ReturnRef(childActions_));
        ON_CALL(*this, hasChildActions()).WillByDefault(testing::Invoke([this]() { return !childActions_.empty(); }));

        // 메서드 호출 시 멤버 변수 업데이트 및 로깅 추가
        ON_CALL(*this, setExternalClass(testing::_))
            .WillByDefault(testing::Invoke([this](const std::string &className) {
                SCXML::Common::Logger::debug("MockActionNode::setExternalClass - Called with: " + className);
                this->externalClass_ = className;
            }));
        ON_CALL(*this, setExternalFactory(testing::_))
            .WillByDefault(testing::Invoke([this](const std::string &factoryName) {
                SCXML::Common::Logger::debug("MockActionNode::setExternalFactory - Called with: " + factoryName);
                this->externalFactory_ = factoryName;
            }));
        ON_CALL(*this, setType(testing::_)).WillByDefault(testing::Invoke([this](const std::string &type) {
            SCXML::Common::Logger::debug("MockActionNode::setType - Called with: " + type);
            this->type_ = type;
        }));
        ON_CALL(*this, setAttribute(testing::_, testing::_))
            .WillByDefault(testing::Invoke([this](const std::string &key, const std::string &value) {
                SCXML::Common::Logger::debug("MockActionNode::setAttribute - Key: " + key + ", Value: " + value);
                this->attributes_[key] = value;
            }));
        ON_CALL(*this, addChildAction(testing::_))
            .WillByDefault(testing::Invoke([this](std::shared_ptr<SCXML::Model::IActionNode> childAction) {
                SCXML::Common::Logger::debug("MockActionNode::addChildAction - Adding child: " + childAction->getId());
                this->childActions_.push_back(childAction);
            }));
        ON_CALL(*this, setChildActions(testing::_))
            .WillByDefault(
                testing::Invoke([this](const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &childActions) {
                    SCXML::Common::Logger::debug("MockActionNode::setChildActions - Setting " +
                                                 std::to_string(childActions.size()) + " child actions");
                    this->childActions_ = childActions;
                }));

        // Default behavior for execute method
        ON_CALL(*this, execute(testing::_)).WillByDefault(testing::Invoke([](SCXML::Runtime::RuntimeContext &) {
            SCXML::Common::Logger::debug("MockActionNode::execute - Called, returning true");
            return true;
        }));

        // Default behavior for clone method
        ON_CALL(*this, clone()).WillByDefault(testing::Invoke([this]() {
            SCXML::Common::Logger::debug("MockActionNode::clone - Creating cloned instance");
            auto cloned = std::make_shared<MockActionNode>();
            cloned->id_ = this->id_;
            cloned->externalClass_ = this->externalClass_;
            cloned->externalFactory_ = this->externalFactory_;
            cloned->type_ = this->type_;
            cloned->attributes_ = this->attributes_;
            cloned->childActions_ = this->childActions_;
            return std::static_pointer_cast<IActionNode>(cloned);
        }));

        // Default behavior for getActionType method
        ON_CALL(*this, getActionType()).WillByDefault(testing::Invoke([]() {
            SCXML::Common::Logger::debug("MockActionNode::getActionType - Returning 'mock'");
            return std::string("mock");
        }));
    }
};
