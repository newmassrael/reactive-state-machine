#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "model/DocumentModel.h"
#include "runtime/InitialStateResolver.h"
#include "runtime/Processor.h"
#include "runtime/RuntimeContext.h"

namespace SCXML {

namespace Model {
class DocumentModel;
}
namespace Runtime {

/**
 * @brief State Machine Initializer - Handles state machine startup and configuration
 *
 * This class manages the complex process of initializing an SCXML state machine,
 * including initial state resolution, data model initialization, and runtime setup.
 */
class StateMachineInitializer {
public:
    /**
     * @brief Initialization phases
     */
    enum class InitPhase {
        PREPARATION,     // Preparing for initialization
        DATA_MODEL,      // Initializing data model
        INITIAL_STATES,  // Resolving initial states
        ENTRY_ACTIONS,   // Executing entry actions
        COMPLETION,      // Finalizing initialization
        FAILED           // Initialization failed
    };

    /**
     * @brief Initialization result
     */
    struct InitializationResult {
        bool success = false;
        InitPhase completedPhase = InitPhase::PREPARATION;
        std::vector<std::string> initialStates;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::string errorMessage;

        // Statistics
        size_t dataItemsInitialized = 0;
        size_t entryActionsExecuted = 0;
        size_t statesEntered = 0;
        double initializationTimeMs = 0.0;
    };

    /**
     * @brief Initialization options
     */
    struct InitializationOptions {
        bool validateInitialStates = true;     // Validate initial state configuration
        bool executeEntryActions = true;       // Execute onentry actions during init
        bool enableErrorRecovery = true;       // Try to recover from non-fatal errors
        bool enableProgressReporting = false;  // Report initialization progress
        bool strictMode = false;               // Strict SCXML compliance mode

        // Callback functions
        std::function<void(InitPhase, const std::string &)> progressCallback;
        std::function<void(const std::string &)> errorCallback;
        std::function<void(const std::string &)> warningCallback;
    };

public:
    /**
     * @brief Constructor
     * @param options Initialization options
     */
    explicit StateMachineInitializer(const InitializationOptions &options = InitializationOptions{});

    /**
     * @brief Initialize state machine
     * @param runtime Runtime instance to initialize
     * @param model SCXML model
     * @return Initialization result
     */
    InitializationResult initialize(std::shared_ptr<Processor> runtime,
                                    std::shared_ptr<::SCXML::Model::DocumentModel> model);

    /**
     * @brief Initialize runtime context
     * @param context Runtime context to initialize
     * @param model SCXML model
     * @return true if successful
     */
    bool initializeRuntimeContext(SCXML::Runtime::RuntimeContext &context,
                                  std::shared_ptr<::SCXML::Model::DocumentModel> model);

    /**
     * @brief Resolve initial states
     * @param model SCXML model
     * @param context Runtime context
     * @return Vector of initial state IDs
     */
    std::vector<std::string> resolveInitialStates(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                  SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute initial entry actions
     * @param initialStates List of initial states
     * @param model SCXML model
     * @param context Runtime context
     * @return Number of actions executed successfully
     */
    size_t executeInitialEntryActions(const std::vector<std::string> &initialStates,
                                      std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                      SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Validate initial state configuration
     * @param initialStates List of initial states
     * @param model SCXML model
     * @return Vector of validation errors (empty if valid)
     */
    std::vector<std::string> validateInitialConfiguration(const std::vector<std::string> &initialStates,
                                                          std::shared_ptr<::SCXML::Model::DocumentModel> model);

    /**
     * @brief Get last initialization result
     * @return Last initialization result
     */
    const InitializationResult &getLastResult() const;

    /**
     * @brief Set initialization options
     * @param options New initialization options
     */
    void setOptions(const InitializationOptions &options);

    /**
     * @brief Get current initialization options
     * @return Current initialization options
     */
    const InitializationOptions &getOptions() const;

private:
    /**
     * @brief Initialize data model variables
     * @param model SCXML model
     * @param context Runtime context
     * @return Number of data items initialized
     */
    size_t initializeDataModel(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                               SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Initialize system variables
     * @param model SCXML model
     * @param context Runtime context
     * @return Number of system variables initialized
     */
    size_t initializeSystemVariables(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                     SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Enter initial states
     * @param initialStates List of initial states
     * @param model SCXML model
     * @param context Runtime context
     * @return Number of states entered
     */
    size_t enterInitialStates(const std::vector<std::string> &initialStates,
                              std::shared_ptr<::SCXML::Model::DocumentModel> model,
                              SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Report progress to callback
     * @param phase Current phase
     * @param message Progress message
     */
    void reportProgress(InitPhase phase, const std::string &message);

    /**
     * @brief Add error message
     * @param message Error message
     */
    void addError(const std::string &message);

    /**
     * @brief Add warning message
     * @param message Warning message
     */
    void addWarning(const std::string &message);

    /**
     * @brief Get phase name as string
     * @param phase Initialization phase
     * @return Phase name
     */
    static std::string getPhaseString(InitPhase phase);

private:
    InitializationOptions options_;                                     // Initialization options
    InitializationResult lastResult_;                                   // Last initialization result
    std::shared_ptr<SCXML::Model::InitialStateResolver> stateResolver_;  // Initial state resolver
};

// ========== Utility Functions ==========

/**
 * @brief Quick initialization with default options
 * @param runtime Runtime to initialize
 * @param model SCXML model
 * @return true if successful
 */
bool quickInitialize(std::shared_ptr<Processor> runtime, std::shared_ptr<::SCXML::Model::DocumentModel> model);

/**
 * @brief Initialize with progress reporting
 * @param runtime Runtime to initialize
 * @param model SCXML model
 * @param progressCallback Progress callback function
 * @return Initialization result
 */
StateMachineInitializer::InitializationResult
initializeWithProgress(std::shared_ptr<Processor> runtime, std::shared_ptr<::SCXML::Model::DocumentModel> model,
                       std::function<void(StateMachineInitializer::InitPhase, const std::string &)> progressCallback);

}  // namespace Runtime
}  // namespace SCXML