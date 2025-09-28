#pragma once

#include "ConcurrentRegion.h"
#include "ConcurrentStateTypes.h"
#include "IConcurrentRegion.h"
#include "model/IStateNode.h"
#include "runtime/IExecutionContext.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

// Forward declarations
struct EventDescriptor;
class ITransitionNode;
class IDataModelItem;
class IInvokeNode;
class IActionNode;
class DoneData;

/**
 * @brief Callback type for parallel state completion notification
 *
 * Called when all regions in a parallel state reach their final states.
 * This enables the runtime system to generate the required done.state.{id} event
 * according to SCXML W3C specification section 3.4.
 *
 * @param stateId ID of the completed parallel state
 */
using ParallelStateCompletionCallback = std::function<void(const std::string &stateId)>;

/**
 * @brief Implementation of parallel/concurrent state node
 *
 * This class implements the SCXML parallel state semantics where multiple
 * regions (child states) are active simultaneously. Each region operates
 * independently and the parallel state is complete when all regions reach
 * their final states.
 *
 * SCXML Compliance:
 * - Supports multiple concurrent regions
 * - All regions activated simultaneously when state is entered
 * - All regions deactivated when state is exited
 * - Events are broadcast to all active regions
 * - State completes when all regions reach final states
 */
class ConcurrentStateNode : public IStateNode {
public:
    /**
     * @brief Construct a concurrent state node
     * @param id Unique identifier for this state
     * @param config Configuration for concurrent behavior
     */
    explicit ConcurrentStateNode(const std::string &id, const ConcurrentStateConfig &config = {});

    /**
     * @brief Destructor
     */
    virtual ~ConcurrentStateNode();

    // IStateNode interface implementation
    const std::string &getId() const override;
    Type getType() const override;

    void setParent(IStateNode *parent) override;
    IStateNode *getParent() const override;

    void addChild(std::shared_ptr<IStateNode> child) override;
    const std::vector<std::shared_ptr<IStateNode>> &getChildren() const override;

    void addTransition(std::shared_ptr<ITransitionNode> transition) override;
    const std::vector<std::shared_ptr<ITransitionNode>> &getTransitions() const override;

    void addDataItem(std::shared_ptr<IDataModelItem> dataItem) override;
    const std::vector<std::shared_ptr<IDataModelItem>> &getDataItems() const override;

    void setOnEntry(const std::string &callback) override;
    const std::string &getOnEntry() const override;

    void setOnExit(const std::string &callback) override;
    const std::string &getOnExit() const override;

    void setInitialState(const std::string &state) override;
    const std::string &getInitialState() const override;

    void addEntryAction(const std::string &actionId) override;
    void addExitAction(const std::string &actionId) override;

    void addInvoke(std::shared_ptr<IInvokeNode> invoke) override;
    const std::vector<std::shared_ptr<IInvokeNode>> &getInvoke() const override;

    void setHistoryType(bool isDeep) override;
    HistoryType getHistoryType() const override;
    bool isShallowHistory() const override;
    bool isDeepHistory() const override;

    void addReactiveGuard(const std::string &guardId) override;
    const std::vector<std::string> &getReactiveGuards() const override;

    const std::vector<std::string> &getEntryActions() const override;
    const std::vector<std::string> &getExitActions() const override;

    void addEntryActionNode(std::shared_ptr<IActionNode> action) override;
    void addExitActionNode(std::shared_ptr<IActionNode> action) override;
    const std::vector<std::shared_ptr<IActionNode>> &getEntryActionNodes() const override;
    const std::vector<std::shared_ptr<IActionNode>> &getExitActionNodes() const override;

    bool isFinalState() const override;
    const DoneData &getDoneData() const override;
    DoneData &getDoneData() override;

    // Additional IStateNode methods
    void setDoneDataContent(const std::string &content) override;
    void addDoneDataParam(const std::string &name, const std::string &value) override;
    void clearDoneDataParams() override;
    std::shared_ptr<ITransitionNode> getInitialTransition() const override;
    void setInitialTransition(std::shared_ptr<ITransitionNode> transition) override;

    // Concurrent state specific methods

    /**
     * @brief Add a concurrent region to this state
     * @param region Region to add
     * @return Operation result
     */
    ConcurrentOperationResult addRegion(std::shared_ptr<IConcurrentRegion> region);

    /**
     * @brief Remove a region by ID
     * @param regionId ID of region to remove
     * @return Operation result
     */
    ConcurrentOperationResult removeRegion(const std::string &regionId);

    /**
     * @brief Get all concurrent regions
     * @return Vector of concurrent regions
     */
    const std::vector<std::shared_ptr<IConcurrentRegion>> &getRegions() const;

    /**
     * @brief Get a specific region by ID
     * @param regionId ID of region to find
     * @return Shared pointer to region (nullptr if not found)
     */
    std::shared_ptr<IConcurrentRegion> getRegion(const std::string &regionId) const;

    /**
     * @brief Enter this parallel state according to SCXML semantics
     * Automatically activates all child regions as required by SCXML W3C spec
     * @return Operation result indicating success/failure of entry
     */
    ConcurrentOperationResult enterParallelState();

    /**
     * @brief Exit this parallel state according to SCXML semantics
     * Automatically deactivates all child regions
     * @param executionContext Execution context for proper exit action execution
     * @return Operation result indicating success/failure of exit
     */
    ConcurrentOperationResult exitParallelState(std::shared_ptr<IExecutionContext> executionContext);

    /**
     * @brief Activate all regions in this concurrent state
     * @return Vector of operation results for each region
     */
    std::vector<ConcurrentOperationResult> activateAllRegions();

    /**
     * @brief Deactivate all regions in this concurrent state
     * @param executionContext Execution context for proper exit action execution
     * @return Vector of operation results for each region
     */
    std::vector<ConcurrentOperationResult> deactivateAllRegions(std::shared_ptr<IExecutionContext> executionContext);

    /**
     * @brief Check if all regions are in final states
     * @return true if all regions are complete
     */
    bool areAllRegionsComplete() const;

    /**
     * @brief Get the current configuration (active regions and their states)
     * @return Vector of region information
     */
    std::vector<ConcurrentRegionInfo> getConfiguration() const;

    /**
     * @brief Process an event in all active regions
     * @param event Event to process
     * @return Vector of operation results from each region
     */
    std::vector<ConcurrentOperationResult> processEventInAllRegions(const EventDescriptor &event);

    /**
     * @brief Get the concurrent state configuration
     * @return Current configuration
     */
    const ConcurrentStateConfig &getConfig() const;

    /**
     * @brief Update the concurrent state configuration
     * @param config New configuration
     */
    void setConfig(const ConcurrentStateConfig &config);

    /**
     * @brief Validate this concurrent state configuration
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validateConcurrentState() const;

    /**
     * @brief Set callback for parallel state completion notification
     *
     * The callback will be invoked when all regions reach their final states,
     * enabling the runtime system to generate the required done.state.{id} event.
     *
     * @param callback Completion callback function
     */
    void setCompletionCallback(const ParallelStateCompletionCallback &callback);

    /**
     * @brief Set ExecutionContext for all regions in this parallel state
     *
     * Allows StateMachine to provide ExecutionContext
     * for proper action execution in transition processing
     *
     * @param executionContext Shared execution context from StateMachine
     */
    void setExecutionContextForRegions(std::shared_ptr<IExecutionContext> executionContext);

private:
    std::string id_;
    IStateNode *parent_;
    ConcurrentStateConfig config_;

    // Completion callback for done.state event generation
    ParallelStateCompletionCallback completionCallback_;

    // Track completion state to prevent duplicate notifications
    mutable bool hasNotifiedCompletion_;

    // Concurrent regions
    std::vector<std::shared_ptr<IConcurrentRegion>> regions_;

    // Standard state node data (inherited behavior)
    std::vector<std::shared_ptr<IStateNode>> children_;
    std::vector<std::shared_ptr<ITransitionNode>> transitions_;
    std::vector<std::shared_ptr<IDataModelItem>> dataItems_;
    std::vector<std::shared_ptr<IInvokeNode>> invokeNodes_;

    std::string onEntry_;
    std::string onExit_;
    std::string initialState_;

    std::vector<std::string> entryActions_;
    std::vector<std::string> exitActions_;
    std::vector<std::shared_ptr<IActionNode>> entryActionNodes_;
    std::vector<std::shared_ptr<IActionNode>> exitActionNodes_;

    std::vector<std::string> reactiveGuards_;

    HistoryType historyType_;
    std::unique_ptr<DoneData> doneData_;

    // Initial transition for compound states (stored but not typically used for concurrent states)
    std::shared_ptr<ITransitionNode> initialTransition_;

    // SCXML W3C parallel state completion monitoring

    /**
     * @brief Check if all regions are in final state
     * @return true if all regions have reached final states
     */
    bool areAllRegionsInFinalState() const;

    /**
     * @brief Generate done.state event when parallel state completes
     * According to SCXML W3C specification section 3.4
     */
    void generateDoneStateEvent();
};

}  // namespace RSM
