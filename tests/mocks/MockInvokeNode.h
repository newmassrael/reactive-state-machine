#pragma once

#include <gmock/gmock.h>
#include "IInvokeNode.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <iostream>

class MockInvokeNode : public IInvokeNode
{
public:
    MOCK_CONST_METHOD0(getId, const std::string &());
    MOCK_CONST_METHOD0(getType, const std::string &());
    MOCK_CONST_METHOD0(getSrc, const std::string &());
    MOCK_CONST_METHOD0(isAutoForward, bool());
    MOCK_CONST_METHOD0(getIdLocation, const std::string &());
    MOCK_CONST_METHOD0(getNamelist, const std::string &());
    MOCK_CONST_METHOD0(getContent, const std::string &());
    MOCK_CONST_METHOD0(getFinalize, const std::string &());
    MOCK_CONST_METHOD0(getParams, const std::vector<std::tuple<std::string, std::string, std::string>> &());

    MOCK_METHOD1(setType, void(const std::string &));
    MOCK_METHOD1(setSrc, void(const std::string &));
    MOCK_METHOD1(setIdLocation, void(const std::string &));
    MOCK_METHOD1(setNamelist, void(const std::string &));
    MOCK_METHOD1(setAutoForward, void(bool));
    MOCK_METHOD3(addParam, void(const std::string &, const std::string &, const std::string &));
    MOCK_METHOD1(setContent, void(const std::string &));
    MOCK_METHOD1(setFinalize, void(const std::string &));

    // 멤버 변수들
    std::string id_;
    std::string type_;
    std::string src_;
    std::string idLocation_;
    std::string namelist_;
    std::string content_;
    std::string finalize_;
    bool autoForward_ = false;
    std::vector<std::tuple<std::string, std::string, std::string>> params_;

    // 기본 동작 설정 메서드
    void SetupDefaultBehavior()
    {
        std::cout << "Setting up default behavior for MockInvokeNode" << std::endl;

        // 기본 동작 정의
        ON_CALL(*this, getId())
            .WillByDefault([this]() -> const std::string &
                           {
                std::cout << "MockInvokeNode::getId() called, returning: " << id_ << std::endl;
                return id_; });

        ON_CALL(*this, getType())
            .WillByDefault([this]() -> const std::string &
                           {
                std::cout << "MockInvokeNode::getType() called, returning: " << type_ << std::endl;
                return type_; });

        ON_CALL(*this, getSrc())
            .WillByDefault([this]() -> const std::string &
                           {
                std::cout << "MockInvokeNode::getSrc() called, returning: " << src_ << std::endl;
                return src_; });

        ON_CALL(*this, isAutoForward())
            .WillByDefault([this]() -> bool
                           {
                std::cout << "MockInvokeNode::isAutoForward() called, returning: "
                          << (autoForward_ ? "true" : "false") << std::endl;
                return autoForward_; });

        ON_CALL(*this, getIdLocation())
            .WillByDefault([this]() -> const std::string &
                           {
                std::cout << "MockInvokeNode::getIdLocation() called, returning: " << idLocation_ << std::endl;
                return idLocation_; });

        ON_CALL(*this, getNamelist())
            .WillByDefault([this]() -> const std::string &
                           {
                std::cout << "MockInvokeNode::getNamelist() called, returning: " << namelist_ << std::endl;
                return namelist_; });

        ON_CALL(*this, getContent())
            .WillByDefault([this]() -> const std::string &
                           {
                std::cout << "MockInvokeNode::getContent() called, returning: " << content_ << std::endl;
                return content_; });

        ON_CALL(*this, getFinalize())
            .WillByDefault([this]() -> const std::string &
                           {
                std::cout << "MockInvokeNode::getFinalize() called, returning: " << finalize_ << std::endl;
                return finalize_; });

        ON_CALL(*this, getParams())
            .WillByDefault([this]() -> const std::vector<std::tuple<std::string, std::string, std::string>> &
                           {
                std::cout << "MockInvokeNode::getParams() called, returning " << params_.size() << " params" << std::endl;
                return params_; });

        // Default behaviors for setter methods
        ON_CALL(*this, setType(testing::_))
            .WillByDefault([this](const std::string &type)
                           {
                std::cout << "MockInvokeNode::setType() called with: " << type << std::endl;
                this->type_ = type; });

        ON_CALL(*this, setSrc(testing::_))
            .WillByDefault([this](const std::string &src)
                           {
                std::cout << "MockInvokeNode::setSrc() called with: " << src << std::endl;
                this->src_ = src; });

        ON_CALL(*this, setIdLocation(testing::_))
            .WillByDefault([this](const std::string &idLocation)
                           {
                std::cout << "MockInvokeNode::setIdLocation() called with: " << idLocation << std::endl;
                this->idLocation_ = idLocation; });

        ON_CALL(*this, setNamelist(testing::_))
            .WillByDefault([this](const std::string &namelist)
                           {
                std::cout << "MockInvokeNode::setNamelist() called with: " << namelist << std::endl;
                this->namelist_ = namelist; });

        ON_CALL(*this, setAutoForward(testing::_))
            .WillByDefault([this](bool autoForward)
                           {
                std::cout << "MockInvokeNode::setAutoForward() called with: "
                          << (autoForward ? "true" : "false") << std::endl;
                this->autoForward_ = autoForward; });

        ON_CALL(*this, addParam(testing::_, testing::_, testing::_))
            .WillByDefault([this](const std::string &name, const std::string &expr, const std::string &location)
                           {
                std::cout << "MockInvokeNode::addParam() called with name=" << name
                          << ", expr=" << expr << ", location=" << location << std::endl;
                this->params_.push_back(std::make_tuple(name, expr, location)); });

        ON_CALL(*this, setContent(testing::_))
            .WillByDefault([this](const std::string &content)
                           {
                std::cout << "MockInvokeNode::setContent() called with: " << content << std::endl;
                this->content_ = content; });

        ON_CALL(*this, setFinalize(testing::_))
            .WillByDefault([this](const std::string &finalize)
                           {
                std::cout << "MockInvokeNode::setFinalize() called with: " << finalize << std::endl;
                this->finalize_ = finalize; });
    }
};
