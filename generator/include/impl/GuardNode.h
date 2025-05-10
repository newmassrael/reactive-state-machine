#pragma once

#include "IGuardNode.h"
#include <string>
#include <vector>
#include <unordered_map>

/**
 * @brief 가드 노드의 구현 클래스
 *
 * 이 클래스는 전환 조건을 나타내는 가드 노드를 구현합니다.
 * SCXML 문서의 <code:guard> 요소에 해당합니다.
 */
class GuardNode : public IGuardNode
{
public:
    /**
     * @brief 생성자
     * @param id 가드 식별자
     * @param target 타겟 상태 ID
     */
    GuardNode(const std::string &id, const std::string &target);

    /**
     * @brief 소멸자
     */
    virtual ~GuardNode();

    /**
     * @brief 가드 ID 반환
     * @return 가드 ID
     */
    virtual const std::string &getId() const override;

    /**
     * @brief 의존성 추가
     * @param property 의존하는 속성 이름
     */
    virtual void addDependency(const std::string &property) override;

    /**
     * @brief 의존성 목록 반환
     * @return 의존하는 속성 이름 목록
     */
    virtual const std::vector<std::string> &getDependencies() const override;

    /**
     * @brief 외부 클래스 설정
     * @param className 외부 구현 클래스 이름
     */
    virtual void setExternalClass(const std::string &className) override;

    /**
     * @brief 외부 클래스 이름 반환
     * @return 외부 구현 클래스 이름
     */
    virtual const std::string &getExternalClass() const override;

    /**
     * @brief 외부 팩토리 설정
     * @param factoryName 외부 팩토리 이름
     */
    virtual void setExternalFactory(const std::string &factoryName) override;

    /**
     * @brief 외부 팩토리 이름 반환
     * @return 외부 팩토리 이름
     */
    virtual const std::string &getExternalFactory() const override;

    /**
     * @brief 반응형 여부 설정
     * @param reactive 반응형 여부
     */
    virtual void setReactive(bool reactive) override;

    /**
     * @brief 반응형 여부 반환
     * @return 반응형 여부
     */
    virtual bool isReactive() const override;

    virtual void setAttribute(const std::string &name, const std::string &value) override;
    virtual const std::string &getAttribute(const std::string &name) const override;
    virtual const std::unordered_map<std::string, std::string> &getAttributes() const override;

    void setTargetState(const std::string &targetState) override;
    const std::string &getTargetState() const override;
    void setCondition(const std::string &condition) override;
    const std::string &getCondition() const override;

private:
    std::string id_;                                          // 가드 ID
    std::string target_;                                      // 호환성을 위한 기존 필드
    std::string condition_;                                   // 조건식
    std::string targetState_;                                 // 타겟 상태 ID
    std::vector<std::string> dependencies_;                   // 의존성 목록
    std::string externalClass_;                               // 외부 클래스
    std::string externalFactory_;                             // 외부 팩토리
    bool reactive_;                                           // 반응형 여부
    std::unordered_map<std::string, std::string> attributes_; // 기타 속성
    const std::string emptyString_;                           // 빈 문자열 반환용
};
