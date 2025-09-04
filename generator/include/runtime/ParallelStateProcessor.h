#pragma once

#include <memory>
#include <string>
#include <vector>

// Forward declarations
namespace SCXML {

namespace Model {
class DocumentModel;
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

class StateHierarchyManager;

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events
}  // namespace SCXML

/**
 * @brief Parallel State Processor for SCXML
 *
 * This class handles parallel states (concurrent regions) according to
 * the SCXML specification. It manages simultaneous execution of multiple
 * state regions within a parallel state.
 */
class ParallelStateProcessor {
public:
    /**
     * @brief Parallel state processing result
     */
    struct ProcessingResult {
        bool success;                            // Whether processing succeeded
        std::vector<std::string> enteredStates;  // States that were entered
        std::vector<std::string> exitedStates;   // States that were exited
        std::string errorMessage;                // Error message if processing failed

        ProcessingResult() : success(false) {}
    };

    /**
     * @brief Region state information
     */
    struct RegionInfo {
        std::string regionId;                   // Region state ID
        std::vector<std::string> activeStates;  // Currently active states in this region
        bool isCompleted;                       // Whether region has reached final state

        RegionInfo() : isCompleted(false) {}
    };

    /**
     * @brief Construct a new Parallel State Processor
     * @param hierarchyManager State hierarchy manager for traversal
     */
    explicit ParallelStateProcessor(std::shared_ptr<StateHierarchyManager> hierarchyManager);

    /**
     * @brief Destructor
     */
    ~ParallelStateProcessor() = default;

    /**
     * @brief Process parallel state entry
     * @param model SCXML model
     * @param parallelStateId Parallel state to enter
     * @param context Runtime context
     * @return Processing result
     */
    ProcessingResult enterParallelState(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                        const std::string &parallelStateId, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Process parallel state exit
     * @param model SCXML model
     * @param parallelStateId Parallel state to exit
     * @param context Runtime context
     * @return Processing result
     */
    ProcessingResult exitParallelState(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                       const std::string &parallelStateId, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check if state is parallel (concurrent regions)
     * @param model SCXML model
     * @param stateId State ID to check
     * @return true if state is parallel
     */
    bool isParallelState(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::string &stateId) const;

    /**
     * @brief Get region information for parallel state
     * @param parallelStateId Parallel state ID
     * @param context Runtime context
     * @return Vector of region information
     */
    std::vector<RegionInfo> getRegionInfo(const std::string &parallelStateId,
                                          SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Check if parallel state is complete (all regions in final states)
     * @param parallelStateId Parallel state ID
     * @param context Runtime context
     * @return true if all regions are completed
     */
    bool isParallelStateComplete(const std::string &parallelStateId, SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Get all region states for parallel state
     * @param model SCXML model
     * @param parallelStateId Parallel state ID
     * @return Vector of region state IDs
     */
    std::vector<std::string> getRegions(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                        const std::string &parallelStateId) const;

    /**
     * @brief Check if all regions are in valid configuration
     * @param parallelStateId Parallel state ID
     * @param context Runtime context
     * @return true if all regions have at least one active state
     */
    bool isValidParallelConfiguration(const std::string &parallelStateId,
                                      SCXML::Runtime::RuntimeContext &context) const;

protected:
    /**
     * @brief Enter parallel state and all its regions
     * @param model SCXML model
     * @param parallelStateId Parallel state ID
     * @param context Runtime context
     * @param result Result to populate
     */
    void enterParallelStateInternal(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                    const std::string &parallelStateId, SCXML::Runtime::RuntimeContext &context,
                                    ProcessingResult &result);

    /**
     * @brief Exit parallel state and all active regions
     * @param model SCXML model
     * @param parallelStateId Parallel state ID
     * @param context Runtime context
     * @param result Result to populate
     */
    void exitParallelStateInternal(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                   const std::string &parallelStateId, SCXML::Runtime::RuntimeContext &context,
                                   ProcessingResult &result);

    /**
     * @brief Enter all regions of parallel state
     * @param model SCXML model
     * @param regions Region state IDs
     * @param context Runtime context
     * @param result Result to populate
     */
    void enterAllRegions(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::vector<std::string> &regions,
                         SCXML::Runtime::RuntimeContext &context, ProcessingResult &result);

    /**
     * @brief Exit all active descendants of parallel state regions
     * @param regions Region state IDs
     * @param context Runtime context
     * @param result Result to populate
     */
    void exitAllRegions(const std::vector<std::string> &regions, SCXML::Runtime::RuntimeContext &context,
                        ProcessingResult &result);

    /**
     * @brief Get initial state for a region
     * @param model SCXML model
     * @param regionId Region state ID
     * @return Initial state ID for the region
     */
    std::string getRegionInitialState(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                      const std::string &regionId) const;

    /**
     * @brief Check if region is in final state
     * @param model SCXML model
     * @param regionId Region state ID
     * @param context Runtime context
     * @return true if region is completed
     */
    bool isRegionComplete(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::string &regionId,
                          SCXML::Runtime::RuntimeContext &context) const;

private:
    // State hierarchy manager for traversal
    std::shared_ptr<StateHierarchyManager> hierarchyManager_;

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

    /**
     * @brief Check if a region is in final state
     * @param regionId Region state ID to check
     * @param context Runtime context
     * @return true if region is in final state
     */
    bool isRegionInFinalState(const std::string &regionId, SCXML::Runtime::RuntimeContext &context) const;
};