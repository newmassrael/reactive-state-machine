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
class IActionNode;
}  // namespace Model

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

namespace Runtime {
class RuntimeContext;
class StateConfiguration;
class TransitionSelector;
}  // namespace Runtime

namespace Runtime {

/**
 * @brief Microstep and Macrostep Processing Engine for SCXML
 *
 * This class implements the W3C SCXML 1.0 microstep and macrostep algorithms.
 * A microstep is the execution of a set of enabled transitions, including:
 * 1. Exiting source states
 * 2. Executing transition actions
 * 3. Entering target states
 *
 * A macrostep is a series of microsteps that continues until no more
 * eventless transitions are enabled, representing one "step" of the
 * state machine in response to an event.
 */
class MicrostepProcessor {
public:
    /**
     * @brief Result of a single microstep execution
     */
    struct MicrostepResult {
        bool success;                              // Whether microstep completed successfully
        bool transitionsTaken;                     // Whether any transitions were executed
        std::vector<std::string> exitedStates;     // States that were exited
        std::vector<std::string> enteredStates;    // States that were entered
        std::vector<std::string> executedActions;  // Actions that were executed
        Events::EventPtr processedEvent;           // Event that was processed
        std::string errorMessage;                  // Error message if execution failed
        uint64_t executionTimeMs;                  // Execution time in milliseconds

        MicrostepResult() : success(false), transitionsTaken(false), executionTimeMs(0) {}
    };

    /**
     * @brief Result of a complete macrostep execution
     */
    struct MacrostepResult {
        bool success;                                 // Whether macrostep completed successfully
        int microstepsExecuted;                       // Number of microsteps executed
        std::vector<MicrostepResult> microsteps;      // Results of individual microsteps
        std::vector<std::string> finalConfiguration;  // Final state configuration after macrostep
        Events::EventPtr triggeringEvent;             // Initial event that triggered macrostep
        std::string errorMessage;                     // Error message if execution failed
        uint64_t totalExecutionTimeMs;                // Total execution time in milliseconds

        MacrostepResult() : success(false), microstepsExecuted(0), totalExecutionTimeMs(0) {}
    };

    /**
     * @brief Constructor
     */
    MicrostepProcessor();

    /**
     * @brief Destructor
     */
    ~MicrostepProcessor() = default;

    // ====== Initialization ======

    /**
     * @brief Initialize with required components
     * @param model Document model containing state machine definition
     * @param transitionSelector Transition selection engine
     * @return true if initialization succeeded
     */
    bool initialize(std::shared_ptr<Model::DocumentModel> model,
                    std::shared_ptr<TransitionSelector> transitionSelector);

    // ====== Microstep Processing ======

    /**
     * @brief Execute a single microstep
     * @param event Event to process (null for eventless microstep)
     * @param configuration Current state configuration (modified during execution)
     * @param context Runtime context for action execution
     * @return Microstep execution result
     */
    MicrostepResult executeMicrostep(Events::EventPtr event, StateConfiguration &configuration,
                                     Runtime::RuntimeContext &context);

    /**
     * @brief Execute eventless microstep (spontaneous transitions only)
     * @param configuration Current state configuration (modified during execution)
     * @param context Runtime context for action execution
     * @return Microstep execution result
     */
    MicrostepResult executeEventlessMicrostep(StateConfiguration &configuration, Runtime::RuntimeContext &context);

    // ====== Macrostep Processing ======

    /**
     * @brief Execute a complete macrostep
     * @param triggeringEvent External event that triggered the macrostep
     * @param configuration Current state configuration (modified during execution)
     * @param context Runtime context for action execution
     * @return Macrostep execution result
     */
    MacrostepResult executeMacrostep(Events::EventPtr triggeringEvent, StateConfiguration &configuration,
                                     Runtime::RuntimeContext &context);

    /**
     * @brief Execute eventless macrostep (process all eventless transitions)
     * @param configuration Current state configuration (modified during execution)
     * @param context Runtime context for action execution
     * @return Macrostep execution result
     */
    MacrostepResult executeEventlessMacrostep(StateConfiguration &configuration, Runtime::RuntimeContext &context);

    // ====== State Transition Operations ======

    /**
     * @brief Execute state exit actions and remove states from configuration
     * @param statesToExit States to exit in proper order
     * @param configuration Current state configuration
     * @param context Runtime context for action execution
     * @return true if all exit actions succeeded
     */
    bool exitStates(const std::vector<std::string> &statesToExit, StateConfiguration &configuration,
                    Runtime::RuntimeContext &context);

    /**
     * @brief Execute transition actions
     * @param transitions Transitions whose actions should be executed
     * @param context Runtime context for action execution
     * @return true if all transition actions succeeded
     */
    bool executeTransitionActions(const std::vector<std::shared_ptr<Model::ITransitionNode>> &transitions,
                                  Runtime::RuntimeContext &context);

    /**
     * @brief Execute state entry actions and add states to configuration
     * @param statesToEnter States to enter in proper order
     * @param configuration Current state configuration
     * @param context Runtime context for action execution
     * @return true if all entry actions succeeded
     */
    bool enterStates(const std::vector<std::string> &statesToEnter, StateConfiguration &configuration,
                     Runtime::RuntimeContext &context);

    // ====== Execution Order Computation ======

    /**
     * @brief Compute states to exit for given transitions
     * @param transitions Selected transitions
     * @param configuration Current state configuration
     * @return States to exit in reverse document order
     */
    std::vector<std::string> computeExitSet(const std::vector<std::shared_ptr<Model::ITransitionNode>> &transitions,
                                            const StateConfiguration &configuration);

    /**
     * @brief Compute states to enter for given transitions
     * @param transitions Selected transitions
     * @param configuration Current state configuration
     * @return States to enter in document order
     */
    std::vector<std::string> computeEntrySet(const std::vector<std::shared_ptr<Model::ITransitionNode>> &transitions,
                                             const StateConfiguration &configuration);

    /**
     * @brief Get least common ancestor of transition source and targets
     * @param transition Transition to analyze
     * @return LCA state ID
     */
    std::string getLeastCommonAncestor(std::shared_ptr<Model::ITransitionNode> transition);

    // ====== Action Execution ======

    /**
     * @brief Execute a list of actions
     * @param actions Actions to execute
     * @param context Runtime context
     * @return true if all actions succeeded
     */
    bool executeActions(const std::vector<std::shared_ptr<Model::IActionNode>> &actions,
                        Runtime::RuntimeContext &context);

    /**
     * @brief Execute onentry actions for a state
     * @param stateId State whose onentry actions to execute
     * @param context Runtime context
     * @return true if all actions succeeded
     */
    bool executeOnEntryActions(const std::string &stateId, Runtime::RuntimeContext &context);

    /**
     * @brief Execute onexit actions for a state
     * @param stateId State whose onexit actions to execute
     * @param context Runtime context
     * @return true if all actions succeeded
     */
    bool executeOnExitActions(const std::string &stateId, Runtime::RuntimeContext &context);

    // ====== Validation and Configuration ======

    /**
     * @brief Set maximum number of microsteps per macrostep (prevents infinite loops)
     * @param maxMicrosteps Maximum microsteps allowed (0 = no limit)
     */
    void setMaxMicrostepsPerMacrostep(int maxMicrosteps);

    /**
     * @brief Set maximum execution time per macrostep (prevents infinite loops)
     * @param maxTimeMs Maximum execution time in milliseconds (0 = no limit)
     */
    void setMaxExecutionTimeMs(uint64_t maxTimeMs);

    /**
     * @brief Validate microstep result for consistency
     * @param result Microstep result to validate
     * @return Vector of validation error messages
     */
    std::vector<std::string> validateMicrostepResult(const MicrostepResult &result);

    /**
     * @brief Validate macrostep result for consistency
     * @param result Macrostep result to validate
     * @return Vector of validation error messages
     */
    std::vector<std::string> validateMacrostepResult(const MacrostepResult &result);

    // ====== Debugging and Statistics ======

    /**
     * @brief Get detailed execution information
     * @param result Macrostep result to analyze
     * @return Human-readable execution details
     */
    std::string getExecutionDetails(const MacrostepResult &result);

    /**
     * @brief Get execution statistics
     */
    struct ExecutionStats {
        uint64_t totalMicrosteps;
        uint64_t totalMacrosteps;
        uint64_t totalStatesEntered;
        uint64_t totalStatesExited;
        uint64_t totalActionsExecuted;
        uint64_t totalExecutionTimeMs;

        ExecutionStats()
            : totalMicrosteps(0), totalMacrosteps(0), totalStatesEntered(0), totalStatesExited(0),
              totalActionsExecuted(0), totalExecutionTimeMs(0) {}
    };

    /**
     * @brief Get cumulative execution statistics
     * @return Statistics since processor creation or last reset
     */
    ExecutionStats getStatistics() const;

    /**
     * @brief Reset execution statistics
     */
    void resetStatistics();

private:
    // Core components
    std::shared_ptr<Model::DocumentModel> model_;
    std::shared_ptr<TransitionSelector> transitionSelector_;

    // Configuration
    int maxMicrostepsPerMacrostep_;
    uint64_t maxExecutionTimeMs_;

    // Statistics
    mutable ExecutionStats stats_;

    // Helper methods
    std::shared_ptr<Model::IStateNode> getStateNode(const std::string &stateId);
    std::vector<std::string> getProperAncestors(const std::string &stateId);
    std::vector<std::string> getDescendants(const std::string &stateId);
    bool isAncestor(const std::string &ancestor, const std::string &descendant);

    std::vector<std::string> sortInDocumentOrder(const std::vector<std::string> &stateIds);
    std::vector<std::string> sortInReverseDocumentOrder(const std::vector<std::string> &stateIds);

    uint64_t getCurrentTimeMs() const;
    void updateStatistics(const MicrostepResult &result);
    void logMicrostepExecution(const MicrostepResult &result);
    void logMacrostepExecution(const MacrostepResult &result);

    std::string microstepResultToString(const MicrostepResult &result);
    std::string macrostepResultToString(const MacrostepResult &result);
};

}  // namespace Runtime
}  // namespace SCXML