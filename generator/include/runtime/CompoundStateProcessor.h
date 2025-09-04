#pragma once

#include "runtime/InitialStateResolver.h"
#include <memory>
#include <string>
#include <vector>

namespace SCXML {

namespace Model {
class DocumentModel;
}
namespace Core {
class DocumentModel;
}

namespace Core {
// Forward declaration moved to Model namespace
}

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

namespace Runtime {
class RuntimeContext;
class StateHierarchyManager;

/**
 * @brief Compound State Processor for SCXML
 *
 * This class handles compound states (states with child states) according to
 * the SCXML specification. It manages hierarchical state entry/exit and
 * initial state resolution for compound states.
 */
class CompoundStateProcessor {
public:
    /**
     * @brief Compound state processing result
     */
    struct ProcessingResult {
        bool success;                            // Whether processing succeeded
        std::vector<std::string> enteredStates;  // States that were entered
        std::vector<std::string> exitedStates;   // States that were exited
        std::string errorMessage;                // Error message if processing failed

        ProcessingResult() : success(false) {}
    };

    /**
     * @brief Construct a new Compound State Processor
     * @param hierarchyManager State hierarchy manager for traversal
     * @param stateResolver Initial state resolver for compound states
     */
    CompoundStateProcessor(std::shared_ptr<SCXML::Runtime::StateHierarchyManager> hierarchyManager,
                           std::shared_ptr<SCXML::Model::InitialStateResolver> stateResolver);

    /**
     * @brief Destructor
     */
    ~CompoundStateProcessor() = default;

    /**
     * @brief Process compound state entry
     * @param model SCXML model
     * @param compoundStateId Compound state to enter
     * @param context Runtime context
     * @return Processing result
     */
    ProcessingResult enterCompoundState(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                        const std::string &compoundStateId, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Process compound state exit
     * @param model SCXML model
     * @param compoundStateId Compound state to exit
     * @param context Runtime context
     * @return Processing result
     */
    ProcessingResult exitCompoundState(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                       const std::string &compoundStateId, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check if state is compound (has child states)
     * @param model SCXML model
     * @param stateId State ID to check
     * @return true if state is compound
     */
    bool isCompoundState(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::string &stateId) const;

    /**
     * @brief Get active descendant states of compound state
     * @param compoundStateId Compound state ID
     * @param context Runtime context
     * @return Vector of active descendant state IDs
     */
    std::vector<std::string> getActiveDescendants(const std::string &compoundStateId,
                                                  SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Resolve initial configuration for compound state
     * @param model SCXML model
     * @param compoundStateId Compound state ID
     * @param context Runtime context
     * @return Initial state configuration for the compound state
     */
    InitialStateResolver::InitialConfiguration
    resolveInitialConfiguration(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                const std::string &compoundStateId, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Get default child state for compound state
     * @param model SCXML model
     * @param compoundStateId Compound state ID
     * @return Default child state ID, or empty if no children
     */
    std::string getDefaultChild(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                const std::string &compoundStateId) const;

    /**
     * @brief Get specified initial state for compound state
     * @param model SCXML model
     * @param compoundStateId Compound state ID
     * @return Initial state ID, or empty if not specified
     */
    std::string getInitialChild(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                const std::string &compoundStateId) const;

protected:
    /**
     * @brief Enter compound state with all necessary children
     * @param model SCXML model
     * @param compoundStateId Compound state ID
     * @param context Runtime context
     * @param result Result to populate
     */
    void enterCompoundStateInternal(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                    const std::string &compoundStateId, SCXML::Runtime::RuntimeContext &context,
                                    ProcessingResult &result);

    /**
     * @brief Exit compound state and all active descendants
     * @param model SCXML model
     * @param compoundStateId Compound state ID
     * @param context Runtime context
     * @param result Result to populate
     */
    void exitCompoundStateInternal(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                   const std::string &compoundStateId, SCXML::Runtime::RuntimeContext &context,
                                   ProcessingResult &result);

    /**
     * @brief Get child states for compound state
     * @param model SCXML model
     * @param compoundStateId Compound state ID
     * @return Vector of child state IDs
     */
    std::vector<std::string> getChildStates(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                            const std::string &compoundStateId) const;

    /**
     * @brief Check if all required child states are active
     * @param compoundStateId Compound state ID
     * @param context Runtime context
     * @return true if compound state configuration is valid
     */
    bool isValidConfiguration(const std::string &compoundStateId, SCXML::Runtime::RuntimeContext &context) const;

private:
    // State hierarchy manager for traversal
    std::shared_ptr<SCXML::Runtime::StateHierarchyManager> hierarchyManager_;

    // Initial state resolver for compound states
    std::shared_ptr<SCXML::Model::InitialStateResolver> stateResolver_;

    // Error handling
    std::vector<std::string> errorMessages_;

    /**
     * @brief Add error message
     * @param message Error message to add
     */
    void addError(const std::string &message);

    /**
     * @brief Clear error state
     */
    void clearErrors();
};

}  // namespace Runtime
}  // namespace SCXML