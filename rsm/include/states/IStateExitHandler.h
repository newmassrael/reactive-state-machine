#pragma once

#include <memory>
#include <string>
#include <vector>

namespace RSM {

class IStateNode;
class IExecutionContext;

/**
 * @brief Interface for handling state exit operations
 *
 * SOLID: Interface Segregation Principle (ISP)
 * - Clients depend only on the exit handling functionality they need
 * - Separates exit concerns from state management concerns
 */
class IStateExitHandler {
public:
    virtual ~IStateExitHandler() = default;

    /**
     * @brief Execute exit actions for a specific state
     * @param state State node to exit
     * @param executionContext Context for action execution
     * @return true if exit actions executed successfully
     */
    virtual bool executeStateExitActions(std::shared_ptr<IStateNode> state,
                                         std::shared_ptr<IExecutionContext> executionContext) = 0;

    /**
     * @brief Execute exit actions for multiple active states in SCXML-compliant manner
     * @param activeStateIds List of active state IDs to exit (must not be empty)
     * @param rootState Root state node for state lookup (must not be null)
     * @param executionContext Context for action execution (must be valid)
     * @return true if all exit actions executed successfully
     */
    virtual bool executeMultipleStateExits(const std::vector<std::string> &activeStateIds,
                                           std::shared_ptr<IStateNode> rootState,
                                           std::shared_ptr<IExecutionContext> executionContext) = 0;
};

}  // namespace RSM