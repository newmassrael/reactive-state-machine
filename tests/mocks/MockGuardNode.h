#pragma once

#include <gmock/gmock.h>
#include "IGuardNode.h"
#include <string>
#include <vector>
#include <unordered_map>

class MockGuardNode : public IGuardNode
{
public:
    MOCK_CONST_METHOD0(getId, const std::string &());
    MOCK_CONST_METHOD0(getTarget, const std::string &());
    MOCK_METHOD1(setTargetState, void(const std::string &));
    MOCK_CONST_METHOD0(getTargetState, const std::string &());
    MOCK_METHOD1(addDependency, void(const std::string &));
    MOCK_CONST_METHOD0(getDependencies, const std::vector<std::string> &());
    MOCK_METHOD1(setExternalClass, void(const std::string &));
    MOCK_CONST_METHOD0(getExternalClass, const std::string &());
    MOCK_METHOD1(setExternalFactory, void(const std::string &));
    MOCK_CONST_METHOD0(getExternalFactory, const std::string &());
    MOCK_METHOD1(setReactive, void(bool));
    MOCK_CONST_METHOD0(isReactive, bool());
    MOCK_METHOD2(setAttribute, void(const std::string &, const std::string &));
    MOCK_CONST_METHOD1(getAttribute, const std::string &(const std::string &));
    MOCK_CONST_METHOD0(getAttributes, const std::unordered_map<std::string, std::string> &());
    MOCK_METHOD1(setCondition, void(const std::string &));
    MOCK_CONST_METHOD0(getCondition, const std::string &());

    // 실제 데이터 저장을 위한 멤버 변수들
    std::string id_;
    std::string target_;
    std::string targetState_; // 추가된 멤버 변수
    std::vector<std::string> dependencies_;
    std::string externalClass_;
    std::string externalFactory_;
    bool reactive_ = false;
    std::unordered_map<std::string, std::string> attributes_;
    std::string emptyString_; // getAttribute()가 반환할 빈 문자열
    std::string condition_;

    // 기본 생성자
    MockGuardNode() : reactive_(false) {}

    // 기본 동작 설정 메서드
    void SetupDefaultBehavior()
    {
        // 함수 시작 로그
        std::cout << "MockGuardNode::SetupDefaultBehavior() - Setting up default behavior for guard: " << id_ << std::endl;

        ON_CALL(*this, getId()).WillByDefault(testing::ReturnRef(id_));
        std::cout << "  - Setup getId() to return: " << id_ << std::endl;

        ON_CALL(*this, getTarget()).WillByDefault(testing::ReturnRef(target_));
        std::cout << "  - Setup getTarget() to return: " << target_ << std::endl;

        // 타겟 상태 관련 메서드 설정
        ON_CALL(*this, getTargetState()).WillByDefault(testing::ReturnRef(targetState_));
        std::cout << "  - Setup getTargetState() to return: " << targetState_ << std::endl;

        ON_CALL(*this, getDependencies()).WillByDefault(testing::ReturnRef(dependencies_));
        std::cout << "  - Setup getDependencies()" << std::endl;

        ON_CALL(*this, getExternalClass()).WillByDefault(testing::ReturnRef(externalClass_));
        ON_CALL(*this, getExternalFactory()).WillByDefault(testing::ReturnRef(externalFactory_));

        // 조건식 관련 메서드 설정
        ON_CALL(*this, getCondition()).WillByDefault(testing::ReturnRef(condition_));
        std::cout << "  - Setup getCondition() to return: " << condition_ << std::endl;

        // isReactive에 대한 콜백 설정
        ON_CALL(*this, isReactive()).WillByDefault([this]()
                                                   {
        std::cout << "  - isReactive() called, returning: " << (reactive_ ? "true" : "false") << std::endl;
        return reactive_; });
        std::cout << "  - Setup isReactive() with current value: " << (reactive_ ? "true" : "false") << std::endl;

        ON_CALL(*this, getAttributes()).WillByDefault(testing::ReturnRef(attributes_));

        // getAttribute() 메서드가 키가 없을 때 빈 문자열을 반환하도록 설정
        ON_CALL(*this, getAttribute(testing::_))
            .WillByDefault([this](const std::string &name) -> const std::string &
                           {
            std::cout << "  - getAttribute() called for: " << name << std::endl;
            auto it = attributes_.find(name);
            if (it != attributes_.end()) {
                std::cout << "    - Found value: " << it->second << std::endl;
                return it->second;
            }
            std::cout << "    - Not found, returning empty string" << std::endl;
            return emptyString_; });

        // 데이터 수정 메서드
        ON_CALL(*this, addDependency(testing::_))
            .WillByDefault([this](const std::string &dependency)
                           {
            std::cout << "  - addDependency() called with: " << dependency << std::endl;
            dependencies_.push_back(dependency); });

        ON_CALL(*this, setReactive(testing::_))
            .WillByDefault([this](bool reactive)
                           {
            std::cout << "  - setReactive() called with: " << (reactive ? "true" : "false") << std::endl;
            reactive_ = reactive; });

        ON_CALL(*this, setAttribute(testing::_, testing::_))
            .WillByDefault([this](const std::string &name, const std::string &value)
                           {
            std::cout << "  - setAttribute() called with name: " << name << ", value: " << value << std::endl;
            attributes_[name] = value;
            if (name == "reactive" && (value == "true" || value == "1")) {
                std::cout << "    - Setting reactive_ = true" << std::endl;
                reactive_ = true;
            } });

        ON_CALL(*this, setExternalClass(testing::_))
            .WillByDefault([this](const std::string &className)
                           {
            std::cout << "  - setExternalClass() called with: " << className << std::endl;
            externalClass_ = className; });

        ON_CALL(*this, setExternalFactory(testing::_))
            .WillByDefault([this](const std::string &factoryName)
                           {
            std::cout << "  - setExternalFactory() called with: " << factoryName << std::endl;
            externalFactory_ = factoryName; });

        // 조건식 설정 메서드
        ON_CALL(*this, setCondition(testing::_))
            .WillByDefault([this](const std::string &condition)
                           {
            std::cout << "  - setCondition() called with: " << condition << std::endl;
            condition_ = condition; });

        // 타겟 상태 설정 메서드
        ON_CALL(*this, setTargetState(testing::_))
            .WillByDefault([this](const std::string &targetState)
                           {
            std::cout << "  - setTargetState() called with: " << targetState << std::endl;
            targetState_ = targetState; });

        std::cout << "MockGuardNode::SetupDefaultBehavior() - Setup completed for guard: " << id_ << std::endl;
    }
};
