#pragma once

#include "IDataModelItem.h"
#include "IInvokeNode.h"
#include "IStateNode.h"
#include "ITransitionNode.h"
#include "actions/IActionNode.h"
#include "model/DoneData.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief 상태 노드의 구현 클래스
 *
 * 이 클래스는 상태 차트의 상태 노드를 구현합니다.
 * SCXML 문서의 <state>, <parallel>, <final> 요소에 해당합니다.
 */

namespace RSM {

class StateNode : public IStateNode {
public:
    /**
     * @brief 생성자
     * @param id 상태 식별자
     * @param type 상태 타입
     */
    StateNode(const std::string &id, Type type);

    /**
     * @brief 소멸자
     */
    virtual ~StateNode();

    /**
     * @brief 상태 ID 반환
     * @return 상태 ID
     */
    virtual const std::string &getId() const override;

    /**
     * @brief 상태 타입 반환
     * @return 상태 타입
     */
    virtual Type getType() const override;

    /**
     * @brief 부모 상태 설정
     * @param parent 부모 상태 포인터
     */
    virtual void setParent(IStateNode *parent) override;

    /**
     * @brief 부모 상태 반환
     * @return 부모 상태 포인터
     */
    virtual IStateNode *getParent() const override;

    /**
     * @brief 자식 상태 추가
     * @param child 자식 상태
     */
    virtual void addChild(std::shared_ptr<IStateNode> child) override;

    /**
     * @brief 자식 상태 목록 반환
     * @return 자식 상태 목록
     */
    virtual const std::vector<std::shared_ptr<IStateNode>> &getChildren() const override;

    /**
     * @brief 전환 추가
     * @param transition 전환 노드
     */
    virtual void addTransition(std::shared_ptr<ITransitionNode> transition) override;

    /**
     * @brief 전환 목록 반환
     * @return 전환 목록
     */
    virtual const std::vector<std::shared_ptr<ITransitionNode>> &getTransitions() const override;

    /**
     * @brief 데이터 모델 아이템 추가
     * @param dataItem 데이터 모델 아이템
     */
    virtual void addDataItem(std::shared_ptr<IDataModelItem> dataItem) override;

    /**
     * @brief 데이터 모델 아이템 목록 반환
     * @return 데이터 모델 아이템 목록
     */
    virtual const std::vector<std::shared_ptr<IDataModelItem>> &getDataItems() const override;

    /**
     * @brief 초기 상태 ID 설정
     * @param initialState 초기 상태 ID
     */
    virtual void setInitialState(const std::string &initialState) override;

    /**
     * @brief 초기 상태 ID 반환
     * @return 초기 상태 ID
     */
    virtual const std::string &getInitialState() const override;

    /**
     * @brief 진입 콜백 설정
     * @param callback 진입 콜백 이름
     */
    virtual void setOnEntry(const std::string &callback) override;

    /**
     * @brief 진입 콜백 반환
     * @return 진입 콜백 이름
     */
    virtual const std::string &getOnEntry() const override;

    /**
     * @brief 종료 콜백 설정
     * @param callback 종료 콜백 이름
     */
    virtual void setOnExit(const std::string &callback) override;

    /**
     * @brief 종료 콜백 반환
     * @return 종료 콜백 이름
     */
    virtual const std::string &getOnExit() const override;

    /**
     * @brief 진입 액션 추가
     * @param actionId 액션 ID
     */
    virtual void addEntryAction(const std::string &actionId) override;

    /**
     * @brief 종료 액션 추가
     * @param actionId 액션 ID
     */
    virtual void addExitAction(const std::string &actionId) override;

    /**
     * @brief IActionNode 기반 진입 액션 추가
     * @param action 액션 노드
     */
    void addEntryActionNode(std::shared_ptr<RSM::IActionNode> action);

    /**
     * @brief IActionNode 기반 종료 액션 추가
     * @param action 액션 노드
     */
    void addExitActionNode(std::shared_ptr<RSM::IActionNode> action);

    /**
     * @brief IActionNode 기반 진입 액션들 조회
     * @return 진입 액션 노드들
     */
    const std::vector<std::shared_ptr<RSM::IActionNode>> &getEntryActionNodes() const;

    /**
     * @brief IActionNode 기반 종료 액션들 조회
     * @return 종료 액션 노드들
     */
    const std::vector<std::shared_ptr<RSM::IActionNode>> &getExitActionNodes() const;

    /**
     * @brief invoke 노드 추가
     * @param invoke invoke 노드
     */
    virtual void addInvoke(std::shared_ptr<IInvokeNode> invoke) override;

    /**
     * @brief invoke 노드 목록 반환
     * @return invoke 노드 목록
     */
    virtual const std::vector<std::shared_ptr<IInvokeNode>> &getInvoke() const override;

    // 히스토리 타입 설정
    void setHistoryType(HistoryType type) {
        historyType_ = type;
    }

    // IStateNode 인터페이스 구현
    void setHistoryType(bool isDeep) override {
        historyType_ = isDeep ? HistoryType::DEEP : HistoryType::SHALLOW;
    }

    HistoryType getHistoryType() const override {
        return historyType_;
    }

    bool isShallowHistory() const override {
        return historyType_ == HistoryType::SHALLOW;
    }

    bool isDeepHistory() const override {
        return historyType_ == HistoryType::DEEP;
    }

    // 반응형 가드 ID 추가 메서드
    void addReactiveGuard(const std::string &guardId) override;

    // IStateNode 인터페이스 구현
    const std::vector<std::string> &getReactiveGuards() const override;

    const std::vector<std::string> &getEntryActions() const override {
        return entryActions_;
    }

    const std::vector<std::string> &getExitActions() const override {
        return exitActions_;
    }

    bool isFinalState() const override;

    /**
     * @brief DoneData 객체 참조 반환 (상수)
     * @return DoneData 객체 참조
     */
    const DoneData &getDoneData() const override;

    /**
     * @brief DoneData 객체 참조 반환 (수정 가능)
     * @return DoneData 객체 참조
     */
    DoneData &getDoneData() override;

    /**
     * @brief <donedata>의 <content> 요소 설정
     * @param content 콘텐츠 문자열
     */
    void setDoneDataContent(const std::string &content) override;

    /**
     * @brief <donedata>에 <param> 요소 추가
     * @param name 매개변수 이름
     * @param location 데이터 모델 위치 경로
     */
    void addDoneDataParam(const std::string &name, const std::string &location) override;

    void clearDoneDataParams() override;

    /**
     * @brief initial 요소의 전환 객체 반환
     * @return initial 전환 객체에 대한 포인터, initial 요소가 없는 경우 nullptr
     */
    virtual std::shared_ptr<ITransitionNode> getInitialTransition() const override;

    /**
     * @brief initial 요소의 전환 객체 설정
     * @param transition initial 전환 객체
     */
    virtual void setInitialTransition(std::shared_ptr<ITransitionNode> transition) override;

private:
    std::string id_;
    Type type_;
    IStateNode *parent_;
    HistoryType historyType_ = HistoryType::NONE;
    std::vector<std::shared_ptr<IStateNode>> children_;
    std::vector<std::shared_ptr<ITransitionNode>> transitions_;
    std::vector<std::shared_ptr<IDataModelItem>> dataItems_;
    std::string initialState_;
    std::string onEntry_;
    std::string onExit_;
    std::vector<std::string> entryActions_;
    std::vector<std::string> exitActions_;

    // New action system (IActionNode-based)
    std::vector<std::shared_ptr<RSM::IActionNode>> entryActionNodes_;
    std::vector<std::shared_ptr<RSM::IActionNode>> exitActionNodes_;
    std::vector<std::shared_ptr<IInvokeNode>> invokes_;
    DoneData doneData_;
    std::vector<std::string> reactiveGuards_;
    std::shared_ptr<ITransitionNode> initialTransition_;
};

}  // namespace RSM