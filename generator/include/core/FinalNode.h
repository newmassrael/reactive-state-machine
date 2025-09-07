#pragma once

#include "StateNode.h"
#include "core/DoneDataNode.h"
#include "model/IExecutionContext.h"
#include "runtime/RuntimeContext.h"
#include "common/Result.h"
#include <memory>
#include <mutex>

namespace SCXML {
namespace Core {

/**
 * @brief Final State Node for SCXML
 * 
 * SCXML W3C 1.0 사양에 따른 Final State (<final>) 구현:
 * - Final 상태에 진입하면 done.state.{id} 이벤트 자동 생성
 * - <donedata> 요소 지원으로 완료 데이터 포함 가능
 * - 최상위 final 상태 시 state machine 완료 신호
 * - 부모 상태의 완료 조건 평가에 기여
 * 
 * Thread Safety: This class is thread-safe
 */
class FinalNode : public StateNode {
public:
    /**
     * @brief 생성자
     * @param id Final 상태 식별자
     */
    explicit FinalNode(const std::string &id);

    /**
     * @brief 소멸자
     */
    virtual ~FinalNode() = default;

    /**
     * @brief 상태 타입 반환 (항상 FINAL)
     * @return Type::FINAL
     */
    Type getType() const override {
        return Type::FINAL;
    }

    /**
     * @brief Final 상태인지 확인 (항상 true)
     * @return true
     */
    bool isFinalState() const override {
        return true;
    }

    /**
     * @brief Done data 설정
     * @param doneData Done data 노드
     */
    void setDoneData(std::shared_ptr<DoneDataNode> doneData);

    /**
     * @brief Done data 반환
     * @return Done data 노드 (없으면 nullptr)
     */
    std::shared_ptr<DoneDataNode> getDoneDataNode() const;

    /**
     * @brief Final 상태 진입 처리
     * @param context 실행 컨텍스트
     * @return 진입 결과
     */
    SCXML::Common::Result<void> enter(SCXML::Model::IExecutionContext &context);

    /**
     * @brief Final 상태 종료 처리  
     * @param context 실행 컨텍스트
     * @return 종료 결과
     */
    SCXML::Common::Result<void> exit(SCXML::Model::IExecutionContext &context);

    /**
     * @brief SCXML 사양 준수 검증
     * @return 검증 오류 목록 (빈 벡터면 유효)
     */
    std::vector<std::string> validate() const;

    /**
     * @brief Final node 복제
     * @return 복제된 final node
     */
    std::shared_ptr<IStateNode> clone() const;

protected:
    /**
     * @brief done.state.{id} 이벤트 생성
     * @param context 실행 컨텍스트
     * @return 이벤트 생성 결과
     */
    SCXML::Common::Result<void> generateDoneEvent(SCXML::Model::IExecutionContext &context);

    /**
     * @brief 부모 상태의 완료 상태 확인 및 처리
     * @param context 실행 컨텍스트
     */
    void checkParentCompletion(SCXML::Model::IExecutionContext &context);

private:
    std::shared_ptr<DoneDataNode> doneData_;  ///< Done data for completion
    mutable std::mutex finalMutex_;           ///< Thread safety
};

}  // namespace Core
}  // namespace SCXML