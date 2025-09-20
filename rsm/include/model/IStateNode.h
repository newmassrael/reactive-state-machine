#pragma once

#include "DoneData.h"  // 추가된 헤더
#include "actions/IActionNode.h"
#include "types.h"
#include <memory>
#include <string>
#include <vector>

namespace RSM {

class ITransitionNode;
class IInvokeNode;
class IDataModelItem;

class IStateNode {
public:
    virtual ~IStateNode() = default;

    virtual const std::string &getId() const = 0;
    virtual Type getType() const = 0;

    virtual void setParent(IStateNode *parent) = 0;
    virtual IStateNode *getParent() const = 0;

    virtual void addChild(std::shared_ptr<IStateNode> child) = 0;
    virtual const std::vector<std::shared_ptr<IStateNode>> &getChildren() const = 0;

    virtual void addTransition(std::shared_ptr<ITransitionNode> transition) = 0;
    virtual const std::vector<std::shared_ptr<ITransitionNode>> &getTransitions() const = 0;

    virtual void addDataItem(std::shared_ptr<IDataModelItem> dataItem) = 0;
    virtual const std::vector<std::shared_ptr<IDataModelItem>> &getDataItems() const = 0;

    virtual void setOnEntry(const std::string &callback) = 0;
    virtual const std::string &getOnEntry() const = 0;

    virtual void setOnExit(const std::string &callback) = 0;
    virtual const std::string &getOnExit() const = 0;

    virtual void setInitialState(const std::string &state) = 0;
    virtual const std::string &getInitialState() const = 0;

    /**
     * @brief 진입 액션 추가
     * @param actionId 액션 ID
     */
    virtual void addEntryAction(const std::string &actionId) = 0;

    /**
     * @brief 종료 액션 추가
     * @param actionId 액션 ID
     */
    virtual void addExitAction(const std::string &actionId) = 0;

    virtual void addInvoke(std::shared_ptr<IInvokeNode> invoke) = 0;
    virtual const std::vector<std::shared_ptr<IInvokeNode>> &getInvoke() const = 0;

    virtual void setHistoryType(bool isDeep) = 0;

    /**
     * @brief 히스토리 상태 타입 반환
     * @return 히스토리 타입 (NONE, SHALLOW, DEEP)
     */
    virtual HistoryType getHistoryType() const = 0;

    /**
     * @brief Shallow 히스토리 여부 확인
     * @return Shallow 히스토리인지 여부
     */
    virtual bool isShallowHistory() const = 0;

    /**
     * @brief Deep 히스토리 여부 확인
     * @return Deep 히스토리인지 여부
     */
    virtual bool isDeepHistory() const = 0;

    virtual void addReactiveGuard(const std::string &guardId) = 0;
    virtual const std::vector<std::string> &getReactiveGuards() const = 0;

    virtual const std::vector<std::string> &getEntryActions() const = 0;
    virtual const std::vector<std::string> &getExitActions() const = 0;

    // New IActionNode-based action methods
    virtual void addEntryActionNode(std::shared_ptr<RSM::Actions::IActionNode> action) = 0;
    virtual void addExitActionNode(std::shared_ptr<RSM::Actions::IActionNode> action) = 0;
    virtual const std::vector<std::shared_ptr<RSM::Actions::IActionNode>> &getEntryActionNodes() const = 0;
    virtual const std::vector<std::shared_ptr<RSM::Actions::IActionNode>> &getExitActionNodes() const = 0;

    virtual bool isFinalState() const = 0;

    /**
     * @brief DoneData 객체 참조 반환
     * @return DoneData 객체 참조
     */
    virtual const DoneData &getDoneData() const = 0;

    /**
     * @brief DoneData 객체 참조 반환 (수정 가능)
     * @return DoneData 객체 참조
     */
    virtual DoneData &getDoneData() = 0;

    /**
     * @brief <donedata>의 <content> 요소 설정
     * @param content 콘텐츠 문자열
     */
    virtual void setDoneDataContent(const std::string &content) = 0;

    /**
     * @brief <donedata>에 <param> 요소 추가
     * @param name 매개변수 이름
     * @param location 데이터 모델 위치 경로
     */
    virtual void addDoneDataParam(const std::string &name, const std::string &location) = 0;

    /**
     * @brief <donedata>의 모든 <param> 요소 제거
     */
    virtual void clearDoneDataParams() = 0;

    /**
     * @brief initial 요소의 전환 객체 반환
     * @return initial 전환 객체에 대한 포인터, initial 요소가 없는 경우 nullptr
     */
    virtual std::shared_ptr<ITransitionNode> getInitialTransition() const = 0;

    /**
     * @brief initial 요소의 전환 객체 설정
     * @param transition 초기 전환 객체
     */
    virtual void setInitialTransition(std::shared_ptr<ITransitionNode> transition) = 0;
};

}  // namespace RSM
