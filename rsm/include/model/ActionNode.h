#pragma once

#include "IActionNode.h"
#include <string>
#include <unordered_map>

/**
 * @brief 액션 노드의 구현 클래스
 *
 * 이 클래스는 상태 전환 시 실행되는 액션을 나타냅니다.
 * SCXML 문서의 <code:action> 요소에 해당합니다.
 */

namespace RSM {

class ActionNode : public IActionNode
{
public:
    /**
     * @brief 생성자
     * @param id 액션 식별자
     */
    explicit ActionNode(const std::string &id);

    /**
     * @brief 소멸자
     */
    virtual ~ActionNode();

    /**
     * @brief 액션 ID 반환
     * @return 액션 ID
     */
    virtual const std::string &getId() const override;

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
     * @brief 액션 타입 설정
     * @param type 액션 타입 (예: "normal", "external")
     */
    virtual void setType(const std::string &type) override;

    /**
     * @brief 액션 타입 반환
     * @return 액션 타입
     */
    virtual const std::string &getType() const override;

    /**
     * @brief 추가 속성 설정
     * @param name 속성 이름
     * @param value 속성 값
     */
    virtual void setAttribute(const std::string &name, const std::string &value) override;

    /**
     * @brief 속성 값 반환
     * @param name 속성 이름
     * @return 속성 값, 없으면 빈 문자열
     */
    virtual const std::string &getAttribute(const std::string &name) const override;

    /**
     * @brief 모든 속성 반환
     * @return 모든 속성의 맵
     */
    virtual const std::unordered_map<std::string, std::string> &getAttributes() const override;

    void addChildAction(std::shared_ptr<IActionNode> childAction) override
    {
        childActions_.push_back(childAction);
    }

    void setChildActions(const std::vector<std::shared_ptr<IActionNode>> &childActions) override
    {
        childActions_ = childActions;
    }

    const std::vector<std::shared_ptr<IActionNode>> &getChildActions() const override
    {
        return childActions_;
    }

    bool hasChildActions() const override
    {
        return !childActions_.empty();
    }

private:
    std::string id_;
    std::string externalClass_;
    std::string externalFactory_;
    std::string type_;
    std::unordered_map<std::string, std::string> attributes_;
    std::vector<std::shared_ptr<IActionNode>> childActions_;
    std::string emptyString_;
};


}  // namespace RSM