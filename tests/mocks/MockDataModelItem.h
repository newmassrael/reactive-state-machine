#pragma once

#include <gmock/gmock.h>
#include "IDataModelItem.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <iostream>

class MockDataModelItem : public IDataModelItem
{
public:
    MOCK_CONST_METHOD0(getId, const std::string &());
    MOCK_CONST_METHOD0(getExpr, const std::string &());
    MOCK_METHOD1(setExpr, void(const std::string &));
    MOCK_CONST_METHOD0(getType, const std::string &());
    MOCK_METHOD1(setType, void(const std::string &));
    MOCK_CONST_METHOD0(getScope, const std::string &());
    MOCK_METHOD1(setScope, void(const std::string &));
    MOCK_METHOD1(setContent, void(const std::string &));
    MOCK_METHOD1(addContent, void(const std::string &));
    MOCK_CONST_METHOD0(getContent, const std::string &());
    MOCK_CONST_METHOD0(getContentItems, const std::vector<std::string> &());
    MOCK_CONST_METHOD0(isXmlContent, bool());
    MOCK_CONST_METHOD1(queryXPath, std::optional<std::string>(const std::string &));
    MOCK_CONST_METHOD1(supportsDataModel, bool(const std::string &));
    MOCK_METHOD1(setSrc, void(const std::string &));
    MOCK_CONST_METHOD0(getSrc, const std::string &());
    MOCK_METHOD2(setAttribute, void(const std::string &, const std::string &));
    MOCK_CONST_METHOD1(getAttribute, const std::string &(const std::string &));
    MOCK_CONST_METHOD0(getAttributes, const std::unordered_map<std::string, std::string> &());

    // 멤버 변수 추가
    std::string id_;
    std::string expr_;
    std::string type_;
    std::string scope_;
    std::string content_;
    std::string src_;
    std::unordered_map<std::string, std::string> attributes_;
    std::vector<std::string> contentItems_;
    bool isXml_ = false;

    // 기본 동작 설정 메서드
    void SetupDefaultBehavior()
    {
        std::cout << "Setting up default behavior for MockDataModelItem" << std::endl;

        // 기본 동작 정의
        ON_CALL(*this, getId())
            .WillByDefault(testing::ReturnRef(id_));
        ON_CALL(*this, getExpr())
            .WillByDefault(testing::ReturnRef(expr_));
        ON_CALL(*this, getType())
            .WillByDefault(testing::ReturnRef(type_));
        ON_CALL(*this, getScope())
            .WillByDefault(testing::ReturnRef(scope_));
        ON_CALL(*this, getContent())
            .WillByDefault(testing::ReturnRef(content_));
        ON_CALL(*this, getSrc())
            .WillByDefault(testing::ReturnRef(src_));
        ON_CALL(*this, getAttributes())
            .WillByDefault(testing::ReturnRef(attributes_));
        ON_CALL(*this, getContentItems())
            .WillByDefault(testing::ReturnRef(contentItems_));
        ON_CALL(*this, isXmlContent())
            .WillByDefault(testing::Return(isXml_));

        // 메서드 호출 시 멤버 변수 업데이트 및 로깅 추가
        ON_CALL(*this, setExpr(testing::_))
            .WillByDefault([this](const std::string &expr)
                           {
                std::cout << "setExpr called with: " << expr << std::endl;
                this->expr_ = expr; });
        ON_CALL(*this, setType(testing::_))
            .WillByDefault([this](const std::string &type)
                           {
                std::cout << "setType called with: " << type << std::endl;
                this->type_ = type;
                // XML 타입이면 isXml_ 설정
                this->isXml_ = (type == "xpath" || type == "xml"); });
        ON_CALL(*this, setScope(testing::_))
            .WillByDefault([this](const std::string &scope)
                           {
                std::cout << "setScope called with: " << scope << std::endl;
                this->scope_ = scope; });
        ON_CALL(*this, setContent(testing::_))
            .WillByDefault([this](const std::string &content)
                           {
                    std::cout << "setContent called with: " << content << std::endl;
                    if (this->type_ == "xpath" || this->type_ == "xml") {
                        // XML 타입의 경우 내용 누적
                        if (!this->content_.empty()) {
                            this->content_ += content;
                        } else {
                            this->content_ = content;
                        }
                    } else {
                        // 다른 타입은 덮어쓰기
                        this->content_ = content;
                    }
                    this->contentItems_.push_back(content); });
        ON_CALL(*this, addContent(testing::_))
            .WillByDefault([this](const std::string &content)
                           {
                std::cout << "addContent called with: " << content << std::endl;
                if (!this->content_.empty()) {
                    this->content_ += content;
                } else {
                    this->content_ = content;
                }
                this->contentItems_.push_back(content); });
        ON_CALL(*this, setSrc(testing::_))
            .WillByDefault([this](const std::string &src)
                           {
                std::cout << "setSrc called with: " << src << std::endl;
                this->src_ = src; });
        ON_CALL(*this, setAttribute(testing::_, testing::_))
            .WillByDefault([this](const std::string &key, const std::string &value)
                           {
                std::cout << "setAttribute called with key: " << key << ", value: " << value << std::endl;
                this->attributes_[key] = value; });
        ON_CALL(*this, getAttribute(testing::_))
            .WillByDefault([this](const std::string &key) -> const std::string &
                           {
                    std::cout << "getAttribute called with key: " << key << std::endl;
                    auto it = attributes_.find(key);
                    static std::string empty;
                    return (it != attributes_.end()) ? it->second : empty; });
        ON_CALL(*this, queryXPath(testing::_))
            .WillByDefault([this](const std::string &xpath)
                           {
                std::cout << "queryXPath called with: " << xpath << std::endl;
                // XPath 쿼리 가상 구현, 실제로는 XML 파싱 및 쿼리 로직이 필요
                if (this->isXml_) {
                    return std::optional<std::string>("Mock XPath result for query: " + xpath);
                }
                return std::optional<std::string>(); });
        ON_CALL(*this, supportsDataModel(testing::_))
            .WillByDefault([](const std::string &dataModelType)
                           {
                std::cout << "supportsDataModel called with: " << dataModelType << std::endl;
                return (dataModelType == "xpath" || dataModelType == "xml" ||
                        dataModelType == "ecmascript" || dataModelType == "null"); });
    }
};
