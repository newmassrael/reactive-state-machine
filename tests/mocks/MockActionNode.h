#pragma once

#include <gmock/gmock.h>
#include "IActionNode.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <iostream>

class MockActionNode : public IActionNode
{
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
    MOCK_METHOD1(addChildAction, void(std::shared_ptr<IActionNode>));
    MOCK_METHOD1(setChildActions, void(const std::vector<std::shared_ptr<IActionNode>> &));
    MOCK_CONST_METHOD0(getChildActions, const std::vector<std::shared_ptr<IActionNode>> &());
    MOCK_CONST_METHOD0(hasChildActions, bool());

    std::string id_;
    std::string externalClass_;
    std::string externalFactory_;
    std::string type_;
    std::unordered_map<std::string, std::string> attributes_;
    std::vector<std::shared_ptr<IActionNode>> childActions_;
    std::string emptyString_;

    // 기본 동작 설정 메서드
    void SetupDefaultBehavior()
    {
        std::cout << "Setting up default behavior for MockActionNode" << std::endl;

        // 기본 동작 정의
        ON_CALL(*this, getId())
            .WillByDefault(testing::ReturnRef(id_));
        ON_CALL(*this, getExternalClass())
            .WillByDefault(testing::ReturnRef(externalClass_));
        ON_CALL(*this, getExternalFactory())
            .WillByDefault(testing::ReturnRef(externalFactory_));
        ON_CALL(*this, getType())
            .WillByDefault(testing::ReturnRef(type_));
        ON_CALL(*this, getAttributes())
            .WillByDefault(testing::ReturnRef(attributes_));
        ON_CALL(*this, getAttribute(testing::_))
            .WillByDefault([this](const std::string &key)
                           {
                std::cout << "getAttribute called with key: " << key << std::endl;
                auto it = attributes_.find(key);
                return (it != attributes_.end()) ? it->second : emptyString_; });
        ON_CALL(*this, getChildActions())
            .WillByDefault(testing::ReturnRef(childActions_));
        ON_CALL(*this, hasChildActions())
            .WillByDefault([this]()
                           { return !childActions_.empty(); });

        // 메서드 호출 시 멤버 변수 업데이트 및 로깅 추가
        ON_CALL(*this, setExternalClass(testing::_))
            .WillByDefault([this](const std::string &className)
                           {
                std::cout << "setExternalClass called with: " << className << std::endl;
                this->externalClass_ = className; });
        ON_CALL(*this, setExternalFactory(testing::_))
            .WillByDefault([this](const std::string &factoryName)
                           {
                std::cout << "setExternalFactory called with: " << factoryName << std::endl;
                this->externalFactory_ = factoryName; });
        ON_CALL(*this, setType(testing::_))
            .WillByDefault([this](const std::string &type)
                           {
                std::cout << "setType called with: " << type << std::endl;
                this->type_ = type; });
        ON_CALL(*this, setAttribute(testing::_, testing::_))
            .WillByDefault([this](const std::string &key, const std::string &value)
                           {
                std::cout << "setAttribute called with key: " << key << ", value: " << value << std::endl;
                this->attributes_[key] = value; });
        ON_CALL(*this, addChildAction(testing::_))
            .WillByDefault([this](std::shared_ptr<IActionNode> childAction)
                           {
                std::cout << "addChildAction called with: " << childAction->getId() << std::endl;
                this->childActions_.push_back(childAction); });
        ON_CALL(*this, setChildActions(testing::_))
            .WillByDefault([this](const std::vector<std::shared_ptr<IActionNode>> &childActions)
                           {
                std::cout << "setChildActions called with " << childActions.size() << " actions" << std::endl;
                this->childActions_ = childActions; });
    }
};
