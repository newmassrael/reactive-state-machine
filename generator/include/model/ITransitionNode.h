#pragma once
#include <memory>
#include <string>
#include <vector>

namespace SCXML {
namespace Model {

// Forward declarations

// ITransitionNode.h

// Forward declarations
class IActionNode;

/**
 * @brief 전환 노드 인터페이스
 *
 * 이 인터페이스는 상태 간 전환을 나타냅니다.
 * SCXML 문서의 <transition> 요소에 해당합니다.
 */
class ITransitionNode {
public:
    /**
     * @brief 가상 소멸자
     */
    virtual ~ITransitionNode() {}

    /**
     * @brief 이벤트 반환
     * @return 전환 이벤트
     */
    virtual const std::string &getEvent() const = 0;

    /**
     * @brief 타겟 상태 목록 반환
     * @return 개별 타겟 상태 ID들의 벡터
     */
    virtual std::vector<std::string> getTargets() const = 0;

    /**
     * @brief 타겟 상태 추가
     * @param target 추가할 타겟 상태 ID
     */
    virtual void addTarget(const std::string &target) = 0;

    /**
     * @brief 모든 타겟 상태 제거
     */
    virtual void clearTargets() = 0;

    /**
     * @brief 타겟이 있는지 확인
     * @return 타겟이 하나 이상 있으면 true, 없으면 false
     */
    virtual bool hasTargets() const = 0;

    /**
     * @brief 가드 조건 설정
     * @param guard 가드 조건 ID
     */
    virtual void setGuard(const std::string &guard) = 0;

    /**
     * @brief 가드 조건 반환
     * @return 가드 조건 ID
     */
    virtual const std::string &getGuard() const = 0;

    /**
     * @brief 액션 추가 (문자열 ID)
     * @param action 액션 ID
     */
    virtual void addAction(const std::string &action) = 0;

    /**
     * @brief 액션 목록 반환 (문자열 ID)
     * @return 액션 ID 목록
     */
    virtual const std::vector<std::string> &getActions() const = 0;

    /**
     * @brief 액션 노드 추가 (객체)
     * @param actionNode 액션 노드 객체
     */
    virtual void addActionNode(std::shared_ptr<IActionNode> actionNode) = 0;

    /**
     * @brief 액션 노드 목록 반환 (객체)
     * @return 액션 노드 객체 목록
     */
    virtual const std::vector<std::shared_ptr<IActionNode>> &getActionNodes() const = 0;

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
     * @brief 내부 전환 여부 설정
     * @param internal 내부 전환 여부
     */
    virtual void setInternal(bool internal) = 0;

    /**
     * @brief 내부 전환 여부 반환
     * @return 내부 전환 여부
     */
    virtual bool isInternal() const = 0;

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
    virtual std::string getAttribute(const std::string &name) const = 0;

    /**
     * @brief 이벤트 추가
     * @param event 이벤트 이름
     */
    virtual void addEvent(const std::string &event) = 0;

    /**
     * @brief 이벤트 목록 반환
     * @return 이벤트 이름 목록
     */
    virtual const std::vector<std::string> &getEvents() const = 0;

    // ====== New methods for SCXML Core Engine ======

    /**
     * @brief Get source state ID
     * @return Source state identifier
     */
    virtual std::string getSource() const = 0;

    /**
     * @brief Set source state ID
     * @param source Source state identifier
     */
    virtual void setSource(const std::string &source) = 0;

    /**
     * @brief Get first target (for backward compatibility)
     * @return First target state ID, empty if no targets
     */
    virtual std::string getTarget() const = 0;

    /**
     * @brief Get condition expression
     * @return Condition expression string
     */
    virtual const std::string &getCond() const = 0;

    /**
     * @brief Set condition expression
     * @param cond Condition expression
     */
    virtual void setCond(const std::string &cond) = 0;

    /**
     * @brief Get transition type
     * @return Type string ("internal" or "external")
     */
    virtual std::string getType() const = 0;

    /**
     * @brief Set transition type
     * @param type Type string ("internal" or "external")
     */
    virtual void setType(const std::string &type) = 0;

    /**
     * @brief Get document order for priority sorting
     * @return Document order index
     */
    virtual int getDocumentOrder() const = 0;

    /**
     * @brief Set document order
     * @param order Document order index
     */
    virtual void setDocumentOrder(int order) = 0;

    /**
     * @brief Get executable content (actions) for transition
     * @return Vector of action nodes
     */
    virtual std::vector<std::shared_ptr<IActionNode>> getExecutableContent() const = 0;
};

}  // namespace Model
}  // namespace SCXML
