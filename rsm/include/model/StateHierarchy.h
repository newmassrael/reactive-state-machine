#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include "model/IStateNode.h"

/**
 * @brief 상태 계층 구조 관리 클래스
 *
 * 이 클래스는 상태 노드 간의 계층 관계를 관리하고
 * 계층 구조 탐색 및 조작 기능을 제공합니다.
 */

namespace RSM {

class StateHierarchy
{
public:
    /**
     * @brief 생성자
     */
    StateHierarchy();

    /**
     * @brief 소멸자
     */
    ~StateHierarchy();

    /**
     * @brief 루트 상태 설정
     * @param rootState 루트 상태 노드
     */
    void setRootState(std::shared_ptr<IStateNode> rootState);

    /**
     * @brief 루트 상태 반환
     * @return 루트 상태 노드
     */
    IStateNode *getRootState() const;

    /**
     * @brief 상태 추가
     * @param state 상태 노드
     * @param parentId 부모 상태 ID (없으면 루트 자식으로 추가)
     * @return 성공 여부
     */
    bool addState(std::shared_ptr<IStateNode> state, const std::string &parentId = "");

    /**
     * @brief ID로 상태 찾기
     * @param id 상태 ID
     * @return 상태 노드 포인터, 없으면 nullptr
     */
    IStateNode *findStateById(const std::string &id) const;

    /**
     * @brief 두 상태 간의 관계 확인
     * @param ancestorId 조상 상태 ID
     * @param descendantId 자손 상태 ID
     * @return descendantId가 ancestorId의 자손인지 여부
     */
    bool isDescendantOf(const std::string &ancestorId, const std::string &descendantId) const;

    /**
     * @brief 모든 상태 목록 반환
     * @return 모든 상태 노드 목록
     */
    const std::vector<std::shared_ptr<IStateNode>> &getAllStates() const;

    /**
     * @brief 상태 관계 유효성 검증
     * @return 모든 관계가 유효한지 여부
     */
    bool validateRelationships() const;

    /**
     * @brief 누락된 상태 ID 찾기
     * @return 누락된 상태 ID 목록
     */
    std::vector<std::string> findMissingStateIds() const;

    /**
     * @brief 상태 계층 구조 출력 (디버깅용)
     */
    void printHierarchy() const;

private:
    /**
     * @brief 상태 계층 구조 출력 (내부용)
     * @param state 상태 노드
     * @param depth 깊이
     */
    void printStateHierarchy(IStateNode *state, int depth) const;

    /**
     * @brief 두 상태 간의 관계 확인 (내부용)
     * @param ancestor 조상 상태
     * @param descendant 자손 상태
     * @return descendant가 ancestor의 자손인지 여부
     */
    bool isDescendantOf(IStateNode *ancestor, IStateNode *descendant) const;

    // 멤버 변수
    std::shared_ptr<IStateNode> rootState_;
    std::vector<std::shared_ptr<IStateNode>> allStates_;
    std::unordered_map<std::string, IStateNode *> stateIdMap_;
};


}  // namespace RSM