#pragma once

#include "../model/IStateNode.h"
#include "../model/ITransitionNode.h"
#include "HistoryStateManager.h"
#include <memory>
#include <set>
#include <vector>


namespace SCXML {

namespace Model {
class IStateNode;
}
namespace Runtime {

/**
 * @brief Handler for history state transitions and integration
 *
 * Provides integration between history state management and the transition
 * execution system. Handles the logic for entering/exiting states with
 * history considerations.
 */
class HistoryStateTransitionHandler {
public:
    /**
     * @brief Transition context for history operations
     */
    struct TransitionContext {
        std::vector<std::string> currentActiveStates;                    // Currently active states
        std::vector<std::string> exitingStates;                          // States being exited
        std::vector<std::string> enteringStates;                         // States being entered
        std::shared_ptr<const SCXML::Model::ITransitionNode> transition;  // Current transition
        std::string eventName;                                           // Event triggering transition
    };

    /**
     * @brief Result of history state processing
     */
    struct HistoryProcessingResult {
        bool historyInvolved = false;                 // Whether history was involved
        std::vector<std::string> actualTargetStates;  // Actual states to enter
        std::vector<std::string> restoredStates;      // States restored from history
        std::vector<std::string> defaultStates;       // Default states used
        std::string errorMessage;                     // Error if processing failed
    };

public:
    /**
     * @brief Constructor
     * @param historyManager History state manager instance
     */
    explicit HistoryStateTransitionHandler(std::shared_ptr<HistoryStateManager> historyManager);

    /**
     * @brief Destructor
     */
    ~HistoryStateTransitionHandler() = default;

    /**
     * @brief Process state exit with history recording
     * @param context Transition context
     * @return true if processing succeeded
     */
    bool processStateExit(const TransitionContext &context);

    /**
     * @brief Process state entry with history restoration
     * @param context Transition context
     * @return History processing result
     */
    HistoryProcessingResult processStateEntry(const TransitionContext &context);

    /**
     * @brief Check if any of the target states are history states
     * @param targetStates States to check
     * @return true if any target is a history state
     */
    bool hasHistoryStates(const std::vector<std::string> &targetStates) const;

    /**
     * @brief Resolve history state targets to actual states
     * @param targetStates Original target states (may include history states)
     * @param currentActiveStates Currently active states for context
     * @return Processing result with resolved states
     */
    HistoryProcessingResult resolveHistoryTargets(const std::vector<std::string> &targetStates,
                                                  const std::vector<std::string> &currentActiveStates);

    /**
     * @brief Get states that should be exited when leaving a compound state
     * @param compoundStateId Compound state being exited
     * @param allActiveStates All currently active states
     * @return States that should be exited
     */
    std::vector<std::string> getStatesToExitFromCompound(const std::string &compoundStateId,
                                                         const std::vector<std::string> &allActiveStates) const;

    /**
     * @brief Get compound states that should have their history recorded
     * @param exitingStates States being exited
     * @return Compound states that need history recording
     */
    std::vector<std::string> getCompoundStatesForHistoryRecording(const std::vector<std::string> &exitingStates) const;

    /**
     * @brief Check if a state is a compound state (can have history)
     * @param stateId State to check
     * @return true if compound state
     */
    bool isCompoundState(const std::string &stateId) const;

    /**
     * @brief Check if a state is a history state
     * @param stateId State to check
     * @return true if history state
     */
    bool isHistoryState(const std::string &stateId) const;

    /**
     * @brief Get the parent compound state for a given state
     * @param stateId State to find parent for
     * @return Parent compound state ID, empty if none
     */
    std::string getParentCompoundState(const std::string &stateId) const;

    /**
     * @brief Set state model for validation and hierarchy queries
     * @param stateModel State model interface
     */
    void setStateModel(std::shared_ptr<const SCXML::Model::IStateNode> stateModel);

    /**
     * @brief Find parent compound states that have registered history states
     * @param exitingStates States being exited in a transition
     * @return Set of parent state IDs that have history registered
     */
    std::set<std::string> findParentWithHistory(const std::vector<std::string> &exitingStates) const;

private:
    /**
     * @brief Process a single history state target
     * @param historyStateId History state being targeted
     * @param currentActiveStates Current active states for context
     * @return Resolved target states
     */
    std::vector<std::string> processHistoryStateTarget(const std::string &historyStateId,
                                                       const std::vector<std::string> &currentActiveStates);

    /**
     * @brief Record history for compound states being exited
     * @param compoundStates Compound states to record history for
     * @param activeStates Active states when exiting
     * @return true if any history was actually recorded
     */
    bool recordHistoryForCompoundStates(const std::vector<std::string> &compoundStates,
                                        const std::vector<std::string> &activeStates);

    /**
     * @brief Validate that resolved states are valid targets
     * @param resolvedStates States to validate
     * @param originalTargets Original target states for context
     * @return true if all resolved states are valid
     */
    bool validateResolvedStates(const std::vector<std::string> &resolvedStates,
                                const std::vector<std::string> &originalTargets) const;

    /**
     * @brief Get child states of a compound state
     * @param compoundStateId Compound state to get children for
     * @return Child state IDs
     */
    std::vector<std::string> getChildStates(const std::string &compoundStateId) const;

    /**
     * @brief Check if a state has any child states
     * @param stateId State to check
     * @return true if has children
     */
    bool hasChildStates(const std::string &stateId) const;

    /**
     * @brief Check if two states conflict (cannot be active simultaneously)
     * @param state1 First state to check
     * @param state2 Second state to check
     * @return true if states conflict
     */
    bool statesConflict(const std::string &state1, const std::string &state2) const;

    /**
     * @brief Check if a state exists in the state model
     * @param stateId State to check
     * @return true if state exists
     */
    bool stateExistsInModel(const std::string &stateId) const;

    std::shared_ptr<HistoryStateManager> historyManager_;
    std::shared_ptr<const SCXML::Model::IStateNode> stateModel_;

    // Cache for performance
    mutable std::unordered_map<std::string, bool> compoundStateCache_;
    mutable std::unordered_map<std::string, std::vector<std::string>> childStatesCache_;
};

/**
 * @brief Integration helper for existing transition executor
 *
 * Provides static methods for integrating history state functionality
 * into existing transition execution logic.
 */
class HistoryStateIntegration {
public:
    /**
     * @brief Modify target states to account for history states
     * @param originalTargets Original transition targets
     * @param handler History transition handler
     * @param currentActiveStates Current active states
     * @return Modified target states with history resolution
     */
    static std::vector<std::string> resolveTransitionTargets(const std::vector<std::string> &originalTargets,
                                                             HistoryStateTransitionHandler &handler,
                                                             const std::vector<std::string> &currentActiveStates);

    /**
     * @brief Process state exits to record history
     * @param exitingStates States being exited
     * @param handler History transition handler
     * @param currentActiveStates All currently active states
     * @return true if history recording succeeded
     */
    static bool processTransitionExits(const std::vector<std::string> &exitingStates,
                                       HistoryStateTransitionHandler &handler,
                                       const std::vector<std::string> &currentActiveStates);

    /**
     * @brief Check if a transition involves history states
     * @param transition Transition to check
     * @param handler History transition handler
     * @return true if transition involves history
     */
    static bool transitionInvolveHistory(std::shared_ptr<const SCXML::Model::ITransitionNode> transition,
                                         const HistoryStateTransitionHandler &handler);

    /**
     * @brief Create history transition handler with proper setup
     * @param stateModel State model for hierarchy queries
     * @return Configured history transition handler
     */
    static std::unique_ptr<HistoryStateTransitionHandler>
    createHistoryHandler(std::shared_ptr<const SCXML::Model::IStateNode> stateModel);
};

}  // namespace Runtime
}  // namespace SCXML