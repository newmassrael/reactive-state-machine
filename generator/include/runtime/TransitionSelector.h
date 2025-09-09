#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace SCXML {

namespace Model {
class DocumentModel;
class IStateNode;
class ITransitionNode;
}  // namespace Model

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

namespace Runtime {
class RuntimeContext;
class StateConfiguration;
}  // namespace Runtime

namespace Runtime {

/**
 * @brief Transition Selection Engine for SCXML
 *
 * This class implements the W3C SCXML 1.0 transition selection algorithm.
 * It finds enabled transitions, resolves conflicts based on priority rules,
 * and determines the optimal set of transitions to execute for a given event.
 *
 * The selection process follows the SCXML specification:
 * 1. Find all enabled transitions in document order
 * 2. Filter out conflicting transitions using preemption rules
 * 3. Return the maximal consistent set of transitions
 */
class TransitionSelector {
public:
    /**
     * @brief Represents a candidate transition for execution
     */
    struct TransitionCandidate {
        std::shared_ptr<Model::ITransitionNode> transition;  // The transition node
        std::string sourceStateId;                           // Source state ID
        std::vector<std::string> targetStateIds;             // Target state IDs
        Events::EventPtr triggeringEvent;                    // Event that triggered this transition
        int documentOrder;                                   // Document order for priority
        bool isEventless;                                    // Whether this is an eventless transition
        bool isInternal;                                     // Whether this is an internal transition

        TransitionCandidate() : documentOrder(0), isEventless(false), isInternal(false) {}
    };

    /**
     * @brief Result of transition selection process
     */
    struct SelectionResult {
        std::vector<TransitionCandidate> selectedTransitions;     // Transitions to execute
        std::vector<TransitionCandidate> conflictingTransitions;  // Transitions that were filtered out
        bool hasEnabledTransitions;                               // Whether any transitions were found
        std::string errorMessage;                                 // Error message if selection failed

        SelectionResult() : hasEnabledTransitions(false) {}
    };

    /**
     * @brief Constructor
     */
    TransitionSelector();

    /**
     * @brief Destructor
     */
    ~TransitionSelector() = default;

    // ====== Initialization ======

    /**
     * @brief Initialize with SCXML model
     * @param model Document model containing state machine definition
     * @return true if initialization succeeded
     */
    bool initialize(std::shared_ptr<Model::DocumentModel> model);

    // ====== Transition Selection ======

    /**
     * @brief Select transitions for an external event
     * @param event External event to process
     * @param configuration Current state configuration
     * @param context Runtime context for guard evaluation
     * @return Selection result with chosen transitions
     */
    SelectionResult selectTransitionsForEvent(Events::EventPtr event, const StateConfiguration &configuration,
                                              Runtime::RuntimeContext &context);

    /**
     * @brief Select eventless transitions (spontaneous transitions)
     * @param configuration Current state configuration
     * @param context Runtime context for guard evaluation
     * @return Selection result with chosen eventless transitions
     */
    SelectionResult selectEventlessTransitions(const StateConfiguration &configuration,
                                               Runtime::RuntimeContext &context);

    /**
     * @brief Find all enabled transitions for an event
     * @param event Event to match (null for eventless transitions)
     * @param configuration Current state configuration
     * @param context Runtime context for evaluation
     * @return Vector of all enabled transition candidates
     */
    std::vector<TransitionCandidate> findEnabledTransitions(Events::EventPtr event,
                                                            const StateConfiguration &configuration,
                                                            Runtime::RuntimeContext &context);

    // ====== Conflict Resolution ======

    /**
     * @brief Remove conflicting transitions from candidate set
     * @param candidates All candidate transitions
     * @param configuration Current state configuration
     * @return Filtered set of non-conflicting transitions
     */
    std::vector<TransitionCandidate> removeConflictingTransitions(const std::vector<TransitionCandidate> &candidates,
                                                                  const StateConfiguration &configuration);

    /**
     * @brief Check if two transitions conflict (mutually exclusive)
     * @param t1 First transition candidate
     * @param t2 Second transition candidate
     * @param configuration Current state configuration
     * @return true if transitions conflict and cannot both execute
     */
    bool transitionsConflict(const TransitionCandidate &t1, const TransitionCandidate &t2,
                             const StateConfiguration &configuration);

    // ====== Transition Analysis ======

    /**
     * @brief Check if transition is enabled for given event and context
     * @param transition Transition to evaluate
     * @param event Event to match against transition's event specification
     * @param context Runtime context for guard evaluation
     * @return true if transition should be considered for execution
     */
    bool isTransitionEnabled(std::shared_ptr<Model::ITransitionNode> transition, Events::EventPtr event,
                             Runtime::RuntimeContext &context);

    /**
     * @brief Get all transitions from a specific state
     * @param stateId State ID to get transitions from
     * @return Vector of transitions originating from the state
     */
    std::vector<std::shared_ptr<Model::ITransitionNode>> getTransitionsFromState(const std::string &stateId);

    /**
     * @brief Get all transitions that could be enabled in current configuration
     * @param configuration Current state configuration
     * @return Vector of potentially enabled transitions
     */
    std::vector<std::shared_ptr<Model::ITransitionNode>>
    getPotentialTransitions(const StateConfiguration &configuration);

    // ====== Priority and Ordering ======

    /**
     * @brief Sort transitions by selection priority (document order)
     * @param transitions Transitions to sort
     * @return Sorted vector of transitions
     */
    std::vector<TransitionCandidate> sortByPriority(const std::vector<TransitionCandidate> &transitions);

    /**
     * @brief Get least common ancestor of source and target states
     * @param sourceStateId Source state ID
     * @param targetStateIds Vector of target state IDs
     * @return LCA state ID, or empty string if not found
     */
    std::string getLeastCommonAncestor(const std::string &sourceStateId,
                                       const std::vector<std::string> &targetStateIds);

    // ====== Event Matching ======

    /**
     * @brief Check if event matches transition's event specification
     * @param eventSpec Transition event specification (e.g., "button.click", "*")
     * @param eventName Actual event name
     * @return true if event matches the specification
     */
    bool matchesEventSpec(const std::string &eventSpec, const std::string &eventName);

    /**
     * @brief Check if transition is eventless (no event specification)
     * @param transition Transition to check
     * @return true if transition has no event trigger
     */
    bool isEventlessTransition(std::shared_ptr<Model::ITransitionNode> transition);

    // ====== Validation and Debugging ======

    /**
     * @brief Validate selection result for consistency
     * @param result Selection result to validate
     * @param configuration Current state configuration
     * @return Vector of validation error messages
     */
    std::vector<std::string> validateSelection(const SelectionResult &result, const StateConfiguration &configuration);

    /**
     * @brief Get detailed information about transition selection process
     * @param result Selection result
     * @return Human-readable description of selection process
     */
    std::string getSelectionDetails(const SelectionResult &result);

private:
    // Core data
    std::shared_ptr<Model::DocumentModel> model_;

    // Helper methods
    bool evaluateGuardCondition(std::shared_ptr<Model::ITransitionNode> transition, Runtime::RuntimeContext &context);

    std::vector<std::string> getTargetStates(std::shared_ptr<Model::ITransitionNode> transition);
    bool isInternalTransition(std::shared_ptr<Model::ITransitionNode> transition);

    std::set<std::string> computeExitSet(const std::vector<TransitionCandidate> &transitions,
                                         const StateConfiguration &configuration);
    std::set<std::string> computeEntrySet(const std::vector<TransitionCandidate> &transitions,
                                          const StateConfiguration &configuration);

    bool hasCommonDescendant(const std::string &state1, const std::string &state2);
    std::string findLCA(const std::string &state1, const std::string &state2);
    std::vector<std::string> getProperAncestors(const std::string &stateId);

    std::shared_ptr<Model::IStateNode> getStateNode(const std::string &stateId);
    int getDocumentOrder(std::shared_ptr<Model::ITransitionNode> transition);

    // Error handling
    void logTransitionSelection(const SelectionResult &result);
    std::string candidateToString(const TransitionCandidate &candidate);
};

}  // namespace Runtime
}  // namespace SCXML