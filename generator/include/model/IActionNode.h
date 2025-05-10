// IActionNode.h
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

/**
 * @brief 액션 노드 인터페이스
 *
 * 이 인터페이스는 상태 전환 시 실행되는 액션을 나타냅니다.
 * SCXML 문서의 <code:action> 요소에 해당합니다.
 */
class IActionNode
{
public:
    /**
     * @brief 가상 소멸자
     */
    virtual ~IActionNode() {}

    /**
     * @brief 액션 ID 반환
     * @return 액션 ID
     */
    virtual const std::string &getId() const = 0;

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
     * @brief 액션 타입 설정
     * @param type 액션 타입 (예: "normal", "external")
     */
    virtual void setType(const std::string &type) = 0;

    /**
     * @brief 액션 타입 반환
     * @return 액션 타입
     */
    virtual const std::string &getType() const = 0;

    /**
     * @brief 추가 속성 설정
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

    /**
     * @brief 자식 액션 노드 추가
     * @param childAction 추가할 자식 액션 노드
     */
    virtual void addChildAction(std::shared_ptr<IActionNode> childAction) = 0;

    /**
     * @brief 자식 액션 노드 목록 설정
     * @param childActions 자식 액션 노드 목록
     */
    virtual void setChildActions(const std::vector<std::shared_ptr<IActionNode>> &childActions) = 0;

    /**
     * @brief 자식 액션 노드 목록 반환
     * @return 자식 액션 노드 목록
     */
    virtual const std::vector<std::shared_ptr<IActionNode>> &getChildActions() const = 0;

    /**
     * @brief 자식 액션 노드가 있는지 확인
     * @return 자식 액션 노드 존재 여부
     */
    virtual bool hasChildActions() const = 0;
};
