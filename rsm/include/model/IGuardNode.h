// IGuardNode.h
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief 가드 노드 인터페이스
 *
 * 이 인터페이스는 전환 조건을 나타내는 가드 노드를 정의합니다.
 * SCXML 문서의 <code:guard> 요소에 해당합니다.
 */

namespace RSM {

class IGuardNode {
public:
    /**
     * @brief 가상 소멸자
     */
    virtual ~IGuardNode() {}

    /**
     * @brief 가드 ID 반환
     * @return 가드 ID
     */
    virtual const std::string &getId() const = 0;

    /**
     * @brief 타겟 상태 설정
     * @param targetState 전환 대상 상태 ID
     */
    virtual void setTargetState(const std::string &targetState) = 0;

    /**
     * @brief 타겟 상태 반환
     * @return 전환 대상 상태 ID (있는 경우)
     */
    virtual const std::string &getTargetState() const = 0;

    /**
     * @brief 조건식 설정
     * @param condition 가드 조건식
     */
    virtual void setCondition(const std::string &condition) = 0;

    /**
     * @brief 조건식 반환
     * @return 가드 조건식
     */
    virtual const std::string &getCondition() const = 0;

    /**
     * @brief 의존성 추가
     * @param property 의존하는 속성 이름
     */
    virtual void addDependency(const std::string &property) = 0;

    /**
     * @brief 의존성 목록 반환
     * @return 의존하는 속성 이름 목록
     */
    virtual const std::vector<std::string> &getDependencies() const = 0;

    /**
     * @brief 외부 클래스 설정
     * @param className 외부 구현 클래스 이름
     */
    virtual void setExternalClass(const std::string &className) = 0;

    /**
     * @brief 외부 클래스 이름 반환
     * @return 외부 구현 클래스 이름
     */
    virtual const std::string &getExternalClass() const = 0;

    /**
     * @brief 외부 팩토리 설정
     * @param factoryName 외부 팩토리 이름
     */
    virtual void setExternalFactory(const std::string &factoryName) = 0;

    /**
     * @brief 외부 팩토리 이름 반환
     * @return 외부 팩토리 이름
     */
    virtual const std::string &getExternalFactory() const = 0;

    /**
     * @brief 반응형 여부 설정
     * @param reactive 반응형 여부
     */
    virtual void setReactive(bool reactive) = 0;

    /**
     * @brief 반응형 여부 반환
     * @return 반응형 여부
     */
    virtual bool isReactive() const = 0;

    /**
     * @brief 속성 설정
     * @param name 속성 이름
     * @param value 속성 값
     */
    virtual void setAttribute(const std::string &name, const std::string &value) = 0;

    /**
     * @brief 속성 값 반환
     * @param name 속성 이름
     * @return 속성 값, 없으면 빈 문자열
     */
    virtual const std::string &getAttribute(const std::string &name) const = 0;

    /**
     * @brief 모든 속성 반환
     * @return 모든 속성의 맵
     */
    virtual const std::unordered_map<std::string, std::string> &getAttributes() const = 0;
};

}  // namespace RSM