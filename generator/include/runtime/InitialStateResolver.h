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
}
// Forward declarations
namespace Core {
class DocumentModel;
}

namespace Core {
// Forward declaration moved to Model namespace
}

namespace Runtime {
class RuntimeContext;
}

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

namespace Core {

/**
 * @brief Initial State Resolution Engine for SCXML
 *
 * This class implements the SCXML initial state resolution algorithm according to
 * the W3C specification. It determines which states should be active when the
 * state machine starts and handles initial transitions.
 */
class InitialStateResolver {
public:
    /**
     * @brief Resolution result containing initial configuration
     */
    struct InitialConfiguration {
        std::vector<std::string> activeStates;  // States to be activated
        std::vector<std::string> entryOrder;    // Order for state entry actions
        bool success;                           // Whether resolution succeeded
        std::string errorMessage;               // Error message if failed

        InitialConfiguration() : success(false) {}
    };

    /**
     * @brief Construct a new Initial State Resolver
     */
    InitialStateResolver();

    /**
     * @brief Destructor
     */
    ~InitialStateResolver() = default;

    /**
     * @brief Resolve initial state configuration from SCXML model
     * @param model SCXML model to resolve
     * @param context Runtime context for error reporting
     * @return Initial configuration result
     */
    InitialConfiguration resolveInitialStates(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                              ::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Resolve initial states for a specific compound state
     * @param model SCXML model
     * @param stateId Compound state ID to resolve
     * @param context Runtime context
     * @return Initial configuration for the compound state
     */
    InitialConfiguration resolveCompoundState(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                              const std::string &stateId, ::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check if state needs initial resolution (compound/parallel)
     * @param model SCXML model
     * @param stateId State ID to check
     * @return true if state needs resolution
     */
    bool needsInitialResolution(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::string &stateId) const;

    /**
     * @brief Validate initial state configuration
     * @param model SCXML model
     * @param config Initial configuration to validate
     * @return true if configuration is valid
     */
    bool validateConfiguration(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                               const InitialConfiguration &config) const;

protected:
    /**
     * @brief Resolve initial state for atomic state
     * @param model SCXML model
     * @param stateNode State node to resolve
     * @param config Configuration to populate
     */
    void resolveAtomicState(std::shared_ptr<::SCXML::Model::DocumentModel> model, SCXML::Model::IStateNode &stateNode,
                            InitialConfiguration &config);

    /**
     * @brief Resolve initial states for compound state
     * @param model SCXML model
     * @param stateNode Compound state node
     * @param config Configuration to populate
     */
    void resolveCompoundStateInternal(std::shared_ptr<::SCXML::Model::DocumentModel> model, SCXML::Model::IStateNode &stateNode,
                                      InitialConfiguration &config);

    /**
     * @brief Resolve initial states for parallel state
     * @param model SCXML model
     * @param stateNode Parallel state node
     * @param config Configuration to populate
     */
    void resolveParallelState(std::shared_ptr<::SCXML::Model::DocumentModel> model, SCXML::Model::IStateNode &stateNode,
                              InitialConfiguration &config);

    /**
     * @brief Get effective initial state for compound state
     * @param stateNode Compound state node
     * @return Initial state ID, or empty if not specified
     */
    std::string getEffectiveInitial(SCXML::Model::IStateNode &stateNode) const;

    /**
     * @brief Get default initial state (first child) for compound state
     * @param stateNode Compound state node
     * @return Default initial state ID, or empty if no children
     */
    std::string getDefaultInitial(SCXML::Model::IStateNode &stateNode) const;

    /**
     * @brief Add state to configuration with proper ordering
     * @param stateId State ID to add
     * @param config Configuration to update
     * @param visited Set of already visited states (cycle detection)
     */
    void addToConfiguration(const std::string &stateId, InitialConfiguration &config, std::set<std::string> &visited);

    /**
     * @brief Get proper ancestors for state configuration
     * @param model SCXML model
     * @param stateId State ID
     * @return Vector of ancestor state IDs (bottom-up order)
     */
    std::vector<std::string> getProperAncestors(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                const std::string &stateId) const;

    /**
     * @brief Check if state is compound (has child states)
     * @param stateNode State node to check
     * @return true if state is compound
     */
    bool isCompoundState(SCXML::Model::IStateNode &stateNode) const;

    /**
     * @brief Check if state is parallel (concurrent regions)
     * @param stateNode State node to check
     * @return true if state is parallel
     */
    bool isParallelState(const SCXML::Model::IStateNode &stateNode) const;

    /**
     * @brief Check if state is atomic (no child states)
     * @param stateNode State node to check
     * @return true if state is atomic
     */
    bool isAtomicState(const SCXML::Model::IStateNode &stateNode) const;

    /**
     * @brief Check if state is final state
     * @param stateNode State node to check
     * @return true if state is final
     */
    bool isFinalState(const SCXML::Model::IStateNode &stateNode) const;

private:
    // Internal state for resolution process
    std::set<std::string> resolvedStates_;
    std::vector<std::string> errorMessages_;

    /**
     * @brief Clear internal state
     */
    void clearState();

    /**
     * @brief Add error message
     * @param message Error message to add
     */
    void addError(const std::string &message);
};

}  // namespace Core
}  // namespace SCXML