#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <set>
#include "model/IStateNode.h"
#include "model/IGuardNode.h"
#include "model/IDataModelItem.h"

/**
 * @brief SCXML 문서의 객체 모델 표현
 */

namespace RSM {

class SCXMLModel
{
public:
    SCXMLModel();
    ~SCXMLModel();

    /**
     * @brief 루트 상태 노드 설정
     * @param rootState 루트 상태 노드
     */
    void setRootState(std::shared_ptr<IStateNode> rootState);

    /**
     * @brief 루트 상태 노드 반환
     * @return 루트 상태 노드
     */
    IStateNode *getRootState() const;

    /**
     * @brief SCXML 문서 이름 설정
     * @param name 문서 이름
     */
    void setName(const std::string &name);

    /**
     * @brief SCXML 문서 이름 반환
     * @return 문서 이름
     */
    const std::string &getName() const;

    /**
     * @brief 초기 상태 ID 설정
     * @param initialState 초기 상태 ID
     */
    void setInitialState(const std::string &initialState);

    /**
     * @brief 초기 상태 ID 반환
     * @return 초기 상태 ID
     */
    const std::string &getInitialState() const;

    /**
     * @brief 데이터 모델 타입 설정
     * @param datamodel 데이터 모델 타입
     */
    void setDatamodel(const std::string &datamodel);

    /**
     * @brief 데이터 모델 타입 반환
     * @return 데이터 모델 타입
     */
    const std::string &getDatamodel() const;

    /**
     * @brief 컨텍스트 속성 추가
     * @param name 속성 이름
     * @param type 속성 타입
     */
    void addContextProperty(const std::string &name, const std::string &type);

    /**
     * @brief 컨텍스트 속성 목록 반환
     * @return 컨텍스트 속성 맵
     */
    const std::unordered_map<std::string, std::string> &getContextProperties() const;

    /**
     * @brief 의존성 주입 지점 추가
     * @param name 주입 지점 이름
     * @param type 주입 지점 타입
     */
    void addInjectPoint(const std::string &name, const std::string &type);

    /**
     * @brief 의존성 주입 지점 목록 반환
     * @return 의존성 주입 지점 맵
     */
    const std::unordered_map<std::string, std::string> &getInjectPoints() const;

    /**
     * @brief 가드 조건 추가
     * @param guard 가드 조건 노드
     */
    void addGuard(std::shared_ptr<IGuardNode> guard);

    /**
     * @brief 가드 조건 목록 반환
     * @return 가드 조건 벡터
     */
    const std::vector<std::shared_ptr<IGuardNode>> &getGuards() const;

    /**
     * @brief 상태 노드 추가
     * @param state 상태 노드
     */
    void addState(std::shared_ptr<IStateNode> state);

    /**
     * @brief 상태 노드 목록 반환
     * @return 상태 노드 벡터
     */
    const std::vector<std::shared_ptr<IStateNode>> &getAllStates() const;

    /**
     * @brief ID로 상태 노드 찾기
     * @param id 상태 ID
     * @return 상태 노드 포인터, 없으면 nullptr
     */
    IStateNode *findStateById(const std::string &id) const;

    /**
     * @brief 데이터 모델 항목 추가
     * @param dataItem 데이터 모델 항목
     */
    void addDataModelItem(std::shared_ptr<IDataModelItem> dataItem);

    /**
     * @brief 데이터 모델 항목 목록 반환
     * @return 데이터 모델 항목 벡터
     */
    const std::vector<std::shared_ptr<IDataModelItem>> &getDataModelItems() const;

    /**
     * @brief 상태 관계 유효성 검증
     * @return 모든 관계가 유효한지 여부
     */
    bool validateStateRelationships() const;

    /**
     * @brief 누락된 상태 ID 찾기
     * @return 누락된 상태 ID 목록
     */
    std::vector<std::string> findMissingStateIds() const;

    /**
     * @brief 모델 구조 출력 (디버깅용)
     */
    void printModelStructure() const;

    // 클래스 선언 내에 다음 메서드 추가
    /**
     * @brief 바인딩 모드 설정
     * @param binding 바인딩 모드 ("early" 또는 "late")
     */
    void setBinding(const std::string &binding);

    /**
     * @brief 바인딩 모드 반환
     * @return 바인딩 모드
     */
    const std::string &getBinding() const;

    /**
     * @brief 시스템 변수 추가
     * @param systemVar 시스템 변수 데이터 모델 항목
     */
    void addSystemVariable(std::shared_ptr<IDataModelItem> systemVar);

    /**
     * @brief 시스템 변수 목록 반환
     * @return 시스템 변수 벡터
     */
    const std::vector<std::shared_ptr<IDataModelItem>> &getSystemVariables() const;

private:
    /**
     * @brief 상태 ID로 재귀적으로 상태 노드 찾기
     * @param state 검색 시작할 상태 노드
     * @param id 찾을 상태 ID
     * @return 상태 노드 포인터, 없으면 nullptr
     */
    IStateNode *findStateByIdRecursive(IStateNode *state, const std::string &id, std::set<std::string> &visitedStates) const;

    /**
     * @brief 상태 계층 구조 출력 (디버깅용)
     * @param state 상태 노드
     * @param depth 깊이
     */
    void printStateHierarchy(IStateNode *state, int depth) const;

    // 멤버 변수
    std::shared_ptr<IStateNode> rootState_;
    std::string name_;
    std::string initialState_;
    std::string datamodel_;
    std::unordered_map<std::string, std::string> contextProperties_;
    std::unordered_map<std::string, std::string> injectPoints_;
    std::vector<std::shared_ptr<IGuardNode>> guards_;
    std::vector<std::shared_ptr<IStateNode>> allStates_;
    std::unordered_map<std::string, IStateNode *> stateIdMap_;
    std::vector<std::shared_ptr<IDataModelItem>> dataModelItems_;
    std::string binding_;
    std::vector<std::shared_ptr<IDataModelItem>> systemVariables_;
};


}  // namespace RSM