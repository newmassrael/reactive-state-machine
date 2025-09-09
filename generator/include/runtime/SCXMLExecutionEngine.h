#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

// Forward declarations
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
class TransitionSelector;
class MicrostepProcessor;
}  // namespace Runtime

namespace Runtime {

/**
 * @brief SCXML Execution Engine - W3C SCXML 1.0 compliant execution engine
 *
 * This is the new core execution engine that implements the complete W3C SCXML
 * algorithm including:
 * - State configuration management
 * - Transition selection with proper priority
 * - Microstep/Macrostep processing
 * - Event-driven state machine execution
 */
class SCXMLExecutionEngine {
public:
    /**
     * @brief Execution state of the engine
     */
    enum class ExecutionState {
        IDLE,     // Engine is idle, ready to start
        RUNNING,  // Engine is running
        FINAL,    // Reached final state
        ERROR     // Engine encountered an error
    };

    /**
     * @brief Microstep execution result
     */
    struct MicrostepResult {
        bool eventProcessed = false;             // Whether event was processed
        bool transitionsTaken = false;           // Whether any transitions were taken
        std::vector<std::string> exitedStates;   // States exited during microstep
        std::vector<std::string> enteredStates;  // States entered during microstep
        std::string errorMessage;                // Error message if execution failed
    };

    /**
     * @brief Macrostep execution result
     */
    struct MacrostepResult {
        bool completed = false;                       // Whether macrostep completed
        int microstepsExecuted = 0;                   // Number of microsteps executed
        std::vector<std::string> finalConfiguration;  // Final state configuration
        std::string errorMessage;                     // Error message if execution failed
    };

    /**
     * @brief Constructor
     */
    SCXMLExecutionEngine();

    /**
     * @brief Destructor
     */
    ~SCXMLExecutionEngine();

    // ====== Initialization ======

    /**
     * @brief Initialize the engine with SCXML model
     */
    bool initialize(std::shared_ptr<Model::DocumentModel> model, std::shared_ptr<Runtime::RuntimeContext> context);

    /**
     * @brief Start execution - enter initial states
     */
    bool start();

    /**
     * @brief Reset to initial state
     */
    bool reset();

    // ====== Event Processing (SCXML Algorithm) ======

    /**
     * @brief Process external event (W3C SCXML mainEventLoop)
     * @param event External event to process
     * @return Macrostep execution result
     */
    MacrostepResult processExternalEvent(Events::EventPtr event);

    /**
     * @brief Execute one microstep
     * @param event Event to process (can be null for eventless transitions)
     * @return Microstep execution result
     */
    MicrostepResult executeMicrostep(Events::EventPtr event = nullptr);

    /**
     * @brief Execute macrostep (multiple microsteps until stable)
     * @param initialEvent Initial external event that triggered macrostep
     * @return Macrostep execution result
     */
    MacrostepResult executeMacrostep(Events::EventPtr initialEvent = nullptr);

    // ====== State Queries ======

    /**
     * @brief Get current state configuration
     */
    std::set<std::string> getCurrentConfiguration() const;

    /**
     * @brief Check if state is active
     */
    bool isStateActive(const std::string &stateId) const;

    /**
     * @brief Check if in final state
     */
    bool isInFinalState() const;

    /**
     * @brief Get execution state
     */
    ExecutionState getExecutionState() const;

    // ====== Statistics and Debugging ======

    /**
     * @brief Get execution statistics
     */
    struct ExecutionStats {
        uint64_t microstepsExecuted = 0;
        uint64_t macrostepsExecuted = 0;
        uint64_t transitionsTaken = 0;
        uint64_t eventsProcessed = 0;
        uint64_t stateEntriesExecued = 0;
        uint64_t stateExitsExecuted = 0;
    };

    ExecutionStats getStatistics() const;

    /**
     * @brief Reset statistics
     */
    void resetStatistics();

private:
    // Core components
    std::shared_ptr<Model::DocumentModel> model_;
    std::shared_ptr<Runtime::RuntimeContext> context_;

    // New engine components
    std::unique_ptr<Runtime::StateConfiguration> stateConfig_;
    std::shared_ptr<Runtime::TransitionSelector> transitionSelector_;
    std::unique_ptr<Runtime::MicrostepProcessor> microstepProcessor_;

    // Engine state
    ExecutionState state_;

    // Statistics
    mutable ExecutionStats stats_;

    // Internal methods
    bool initializeComponents();
    bool enterInitialStates();
    void updateStatistics(const MicrostepResult &result);
    void setState(ExecutionState newState);
    std::string stateToString(ExecutionState state) const;
};

}  // namespace Runtime
}  // namespace SCXML