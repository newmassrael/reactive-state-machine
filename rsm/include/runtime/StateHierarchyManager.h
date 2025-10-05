#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace RSM {

// Forward declarations
class SCXMLModel;
class IStateNode;
class IExecutionContext;
class IInvokeNode;
class ConcurrentStateNode;

/**
 * @brief 계층적 상태 관리 시스템
 *
 * SCXML 복합 상태의 계층적 진입/종료 로직을 담당합니다.
 * 기존 StateMachine과 독립적으로 작동하여 최소 침습적 통합을 지원합니다.
 */
class StateHierarchyManager {
public:
    /**
     * @brief 생성자
     * @param model SCXML 모델 (상태 정보 참조용)
     */
    explicit StateHierarchyManager(std::shared_ptr<SCXMLModel> model);

    /**
     * @brief 소멸자
     */
    ~StateHierarchyManager() = default;

    /**
     * @brief 계층적 상태 진입
     *
     * 대상 상태가 복합 상태인 경우 자동으로 초기 자식 상태로 진입합니다.
     * 모든 활성화된 상태들을 내부적으로 추적합니다.
     *
     * @param stateId 진입할 상태 ID
     * @return 성공 여부
     */
    bool enterState(const std::string &stateId);

    /**
     * @brief 현재 가장 깊은 활성 상태 반환
     *
     * 계층 구조에서 가장 깊이 있는 (자식이 없는) 활성 상태를 반환합니다.
     * StateMachine::getCurrentState() 호환성을 위해 사용됩니다.
     *
     * @return 현재 활성 상태 ID
     */
    std::string getCurrentState() const;

    /**
     * @brief 모든 활성 상태 반환
     *
     * 현재 활성화된 모든 상태의 리스트를 반환합니다.
     * 계층 순서대로 정렬됩니다 (부모 -> 자식 순).
     *
     * @return 활성 상태 ID 리스트
     */
    std::vector<std::string> getActiveStates() const;

    /**
     * @brief 특정 상태의 활성 여부 확인
     *
     * @param stateId 확인할 상태 ID
     * @return 활성 상태 여부
     */
    bool isStateActive(const std::string &stateId) const;

    /**
     * @brief 상태 종료
     *
     * 지정된 상태와 그 하위 상태들을 비활성화합니다.
     *
     * @param stateId 종료할 상태 ID
     * @param executionContext 적절한 종료 액션 실행을 위한 실행 컨텍스트
     */
    void exitState(const std::string &stateId, std::shared_ptr<IExecutionContext> executionContext = nullptr);

    /**
     * @brief 모든 상태 초기화
     *
     * 활성 상태 리스트를 모두 비웁니다.
     */
    void reset();

    /**
     * @brief 계층적 모드 필요 여부 확인
     *
     * 현재 활성 상태들이 계층적 관리를 필요로 하는지 확인합니다.
     *
     * @return 계층적 모드 필요 여부
     */
    bool isHierarchicalModeNeeded() const;

    /**
     * @brief Set callback for onentry action execution
     *
     * This callback is called when states are added to the active configuration
     * to execute their onentry actions per W3C SCXML specification.
     *
     * @param callback Function to call with state ID for onentry execution
     */
    void setOnEntryCallback(std::function<void(const std::string &)> callback);

    /**
     * @brief Set callback for invoke deferring (W3C SCXML 6.4 compliance)
     *
     * This callback is called when a state with invoke elements is entered,
     * allowing the StateMachine to defer invoke execution until after state entry completes.
     * This ensures proper timing with transition actions and W3C SCXML compliance.
     *
     * @param callback Function to call with stateId and invoke nodes for deferring
     */
    void setInvokeDeferCallback(
        std::function<void(const std::string &, const std::vector<std::shared_ptr<IInvokeNode>> &)> callback);

    /**
     * @brief Set condition evaluator callback for transition guard evaluation
     *
     * This callback is used by concurrent regions to evaluate guard conditions
     * on transitions using the StateMachine's JavaScript engine.
     *
     * @param evaluator Function to call with condition string, returns evaluation result
     */
    void setConditionEvaluator(std::function<bool(const std::string &)> evaluator);

    /**
     * @brief Set execution context for concurrent region action execution
     *
     * This context is passed to parallel state regions during state entry
     * to ensure proper action execution in transitions (W3C SCXML 403c compliance).
     *
     * @param context Execution context for JavaScript evaluation and action execution
     */
    void setExecutionContext(std::shared_ptr<IExecutionContext> context);

    /**
     * @brief Enter a state along with all its ancestors up to a parent
     *
     * W3C SCXML 3.3: When initial attribute specifies deep descendants,
     * all ancestor states must be entered from top to bottom.
     * Properly handles parallel states in the ancestor chain.
     *
     * @param targetStateId Target state to enter
     * @param stopAtParent Stop before entering this parent (exclusive)
     * @return Success status
     */
    bool enterStateWithAncestors(const std::string &targetStateId, IStateNode *stopAtParent,
                                 std::vector<std::string> *deferredOnEntryStates = nullptr);

    /**
     * @brief Remove a state from active configuration
     *
     * @param stateId State ID to remove
     */
    void removeStateFromConfiguration(const std::string &stateId);

    /**
     * @brief 상태를 활성 구성에 추가 (onentry 콜백 없이)
     *
     * W3C SCXML: Deferred onentry execution을 위해 사용
     * 상태를 configuration에만 추가하고 onentry는 호출하지 않음
     *
     * @param stateId 추가할 상태 ID
     */
    void addStateToConfigurationWithoutOnEntry(const std::string &stateId);

private:
    /**
     * @brief SCXML W3C: Specialized cleanup for parallel states
     *
     * Exits a parallel state and all its descendant regions simultaneously
     * @param parallelStateId The parallel state to exit
     */
    void exitParallelStateAndDescendants(const std::string &parallelStateId);

    /**
     * @brief SCXML W3C: Traditional hierarchical state cleanup
     *
     * Removes a state and all its child states from the active configuration
     * @param stateId The hierarchical state to exit
     */
    void exitHierarchicalState(const std::string &stateId);

    /**
     * @brief Recursively collects all descendant states of a given parent state
     *
     * @param parentId Parent state ID
     * @param collector Vector to collect descendant state IDs
     */
    void collectDescendantStates(const std::string &parentId, std::vector<std::string> &collector);

    /**
     * @brief W3C SCXML 3.3: Update parallel region currentState for deep initial targets
     *
     * When deep initial targets bypass default region initialization, we must synchronize
     * each region's currentState with the actual active configuration. This function
     * finds all active parallel states and updates their regions' currentState to match
     * the deepest active descendant within each region.
     */
    void updateParallelRegionCurrentStates();

    std::shared_ptr<SCXMLModel> model_;
    std::vector<std::string> activeStates_;      // 활성 상태 리스트 (계층 순서)
    std::unordered_set<std::string> activeSet_;  // 빠른 검색용 세트

    // W3C SCXML onentry callback
    std::function<void(const std::string &)> onEntryCallback_;

    // W3C SCXML 6.4: Invoke defer callback for proper timing
    std::function<void(const std::string &, const std::vector<std::shared_ptr<IInvokeNode>> &)> invokeDeferCallback_;
    std::function<bool(const std::string &)> conditionEvaluator_;

    // Execution context for concurrent region action execution (403c fix)
    std::shared_ptr<IExecutionContext> executionContext_;

    /**
     * @brief Update execution context for all regions of a parallel state
     *
     * W3C SCXML 403c: DRY principle - centralized region executionContext management
     * This helper eliminates code duplication between enterState() and setExecutionContext()
     *
     * @param parallelState The parallel state whose regions need executionContext update
     */
    void updateRegionExecutionContexts(ConcurrentStateNode *parallelState);

    /**
     * @brief 상태를 활성 구성에 추가
     *
     * @param stateId 추가할 상태 ID
     */
    void addStateToConfiguration(const std::string &stateId);

    /**
     * @brief 복합 상태의 초기 자식 상태 찾기
     *
     * @param stateNode 복합 상태 노드
     * @return 초기 자식 상태 ID (없으면 빈 문자열)
     */
    std::string findInitialChildState(IStateNode *stateNode) const;

    /**
     * @brief 상태 노드가 복합 상태인지 확인
     *
     * @param stateNode 확인할 상태 노드
     * @return 복합 상태 여부
     */
    bool isCompoundState(IStateNode *stateNode) const;

    /**
     * @brief Check if a state is a descendant of a given root state
     *
     * @param rootState Root state to check from
     * @param stateId State ID to find
     * @return true if stateId is rootState or a descendant of rootState
     */
    bool isStateDescendantOf(IStateNode *rootState, const std::string &stateId) const;

    /**
     * @brief Synchronize parallel region currentState when StateMachine modifies states directly
     *
     * W3C SCXML 405: When StateMachine processes eventless transitions within parallel regions,
     * the ConcurrentRegion must be notified to update its internal state tracking.
     *
     * @param stateId The state that was just entered
     */
    void synchronizeParallelRegionState(const std::string &stateId);
};

}  // namespace RSM