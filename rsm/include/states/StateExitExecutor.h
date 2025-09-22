#pragma once

#include "common/Logger.h"
#include "states/IStateExitHandler.h"

namespace RSM {

/**
 * @brief Concrete implementation for state exit handling
 *
 * SOLID: Single Responsibility Principle (SRP)
 * - Only responsible for executing state exit logic
 * - Separated from state management and orchestration concerns
 *
 * SOLID: Open/Closed Principle (OCP)
 * - Open for extension through inheritance
 * - Closed for modification of core exit logic
 */
class StateExitExecutor : public IStateExitHandler {
public:
    StateExitExecutor() = default;
    ~StateExitExecutor() override = default;

    /**
     * @brief Execute exit actions for a specific state
     * @param state State node to exit
     * @param executionContext Context for action execution
     * @return true if exit actions executed successfully
     */
    bool executeStateExitActions(std::shared_ptr<IStateNode> state,
                                 std::shared_ptr<IExecutionContext> executionContext) override;

    /**
     * @brief Execute exit actions for multiple active states
     * @param activeStateIds List of active state IDs to exit
     * @param rootState Root state node for state lookup
     * @param executionContext Context for action execution
     * @return true if all exit actions executed successfully
     */
    bool executeMultipleStateExits(const std::vector<std::string> &activeStateIds,
                                   std::shared_ptr<IStateNode> rootState,
                                   std::shared_ptr<IExecutionContext> executionContext) override;

private:
    /**
     * @brief Execute IActionNode-based exit actions
     * @param state State containing the actions
     * @param executionContext Context for execution
     * @return true if all actions executed successfully
     */
    bool executeActionNodes(std::shared_ptr<IStateNode> state, std::shared_ptr<IExecutionContext> executionContext);

    /**
     * @brief Log exit action execution
     * @param stateId State ID being processed
     * @param actionDescription Description of the action
     */
    void logExitAction(const std::string &stateId, const std::string &actionDescription) const;
};

}  // namespace RSM