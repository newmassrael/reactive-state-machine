#pragma once
#include "model/ITransitionNode.h"
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace SCXML {
namespace Core {

// TransitionNode.h

// Forward declarations - included headers provide definitions

/**
 * @brief 전환 노드의 구현 클래스
 *
 * 이 클래스는 상태 간 전환을 나타냅니다.
 * SCXML 문서의 <transition> 요소에 해당합니다.
 */
class TransitionNode : public Model::ITransitionNode {
public:
    /**
     * @brief 생성자
     * @param event 전환 이벤트
     * @param target 타겟 상태 ID
     */
    TransitionNode(const std::string &event, const std::string &target);

    /**
     * @brief 소멸자
     */
    virtual ~TransitionNode();

    /**
     * @brief 이벤트 반환
     * @return 전환 이벤트
     */
    virtual const std::string &getEvent() const override;

    /**
     * @brief 타겟 상태 목록 반환
     * @return 개별 타겟 상태 ID들의 벡터
     */
    virtual std::vector<std::string> getTargets() const override;

    /**
     * @brief 타겟 상태 추가
     * @param target 추가할 타겟 상태 ID
     */
    virtual void addTarget(const std::string &target) override;

    /**
     * @brief 모든 타겟 상태 제거
     */
    virtual void clearTargets() override;

    /**
     * @brief 타겟이 있는지 확인
     * @return 타겟이 하나 이상 있으면 true, 없으면 false
     */
    virtual bool hasTargets() const override;

    /**
     * @brief 가드 조건 설정
     * @param guard 가드 조건 ID
     */
    virtual void setGuard(const std::string &guard) override;

    /**
     * @brief 가드 조건 반환
     * @return 가드 조건 ID
     */
    virtual const std::string &getGuard() const override;

    /**
     * @brief 액션 추가 (문자열 ID)
     * @param action 액션 ID
     */
    virtual void addAction(const std::string &action) override;

    /**
     * @brief 액션 목록 반환 (문자열 ID)
     * @return 액션 ID 목록
     */
    virtual const std::vector<std::string> &getActions() const override;

    /**
     * @brief 액션 노드 추가 (객체)
     * @param actionNode 액션 노드 객체
     */
    virtual void addActionNode(std::shared_ptr<SCXML::Model::IActionNode> actionNode) override;

    /**
     * @brief 액션 노드 목록 반환 (객체)
     * @return 액션 노드 객체 목록
     */
    virtual const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &getActionNodes() const override;

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

    virtual void setInternal(bool internal) override;
    virtual bool isInternal() const override;
    virtual void setAttribute(const std::string &name, const std::string &value) override;
    virtual std::string getAttribute(const std::string &name) const override;
    virtual void addEvent(const std::string &event) override;
    virtual const std::vector<std::string> &getEvents() const override;

    // ====== New methods for SCXML Core Engine ======

    /**
     * @brief Get source state ID
     * @return Source state identifier
     */
    virtual std::string getSource() const override;

    /**
     * @brief Set source state ID
     * @param source Source state identifier
     */
    virtual void setSource(const std::string &source) override;

    /**
     * @brief Get first target (for backward compatibility)
     * @return First target state ID, empty if no targets
     */
    virtual std::string getTarget() const override;

    /**
     * @brief Get condition expression
     * @return Condition expression string
     */
    virtual const std::string &getCond() const override;

    /**
     * @brief Set condition expression
     * @param cond Condition expression
     */
    virtual void setCond(const std::string &cond) override;

    /**
     * @brief Get transition type
     * @return Type string ("internal" or "external")
     */
    virtual std::string getType() const override;

    /**
     * @brief Set transition type
     * @param type Type string ("internal" or "external")
     */
    virtual void setType(const std::string &type) override;

    /**
     * @brief Get document order for priority sorting
     * @return Document order index
     */
    virtual int getDocumentOrder() const override;

    /**
     * @brief Set document order
     * @param order Document order index
     */
    virtual void setDocumentOrder(int order) override;

    /**
     * @brief Get executable content (actions) for transition
     * @return Vector of action nodes
     */
    virtual std::vector<std::shared_ptr<SCXML::Model::IActionNode>> getExecutableContent() const override;

private:
    /**
     * @brief 타겟 문자열에서 타겟 ID 목록 파싱
     */
    void parseTargets() const;

    std::string event_;
    std::string target_;
    std::string guard_;
    std::vector<std::string> actions_;
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>> actionNodes_;  // ActionNode 객체들
    bool reactive_;
    bool internal_;
    std::unordered_map<std::string, std::string> attributes_;
    std::vector<std::string> events_;
    mutable std::vector<std::string> cachedTargets_;
    mutable bool targetsDirty_;  // 타겟 캐시가 최신 상태인지 표시

    // New fields for SCXML Core Engine
    std::string source_;     // Source state ID
    std::string condition_;  // Condition expression
    std::string type_;       // Transition type ("internal" or "external")
    int documentOrder_;      // Document order for priority
};

}  // namespace Core
}  // namespace SCXML
