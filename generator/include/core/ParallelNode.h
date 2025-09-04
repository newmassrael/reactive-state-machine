#pragma once
#include "common/SCXMLCommon.h"
#include "model/IParallelNode.h"
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

using SCXML::Model::IParallelNode;
using SCXML::Model::IParallelStateManager;


namespace SCXML {

// Forward declarations
namespace Runtime {
class RuntimeContext;
}

namespace Core {

/**
 * @brief Concrete implementation of SCXML <parallel> element
 *
 * This class implements parallel state execution with full support
 * for concurrent child state management and event processing.
 */
class ParallelNode : public IParallelNode {
public:
    /**
     * @brief Constructor
     * @param id Parallel state ID
     */
    explicit ParallelNode(const std::string &id);

    /**
     * @brief Destructor
     */
    virtual ~ParallelNode() = default;

    // IParallelNode interface implementation
    const std::string &getId() const override;
    void setId(const std::string &id) override;

    void addChildState(const std::string &childStateId) override;
    bool removeChildState(const std::string &childStateId) override;
    std::vector<std::string> getChildStates() const override;
    bool isChildState(const std::string &stateId) const override;

    SCXML::Common::Result<void> enter(SCXML::Model::IExecutionContext &context) override;
    SCXML::Common::Result<void> exit(SCXML::Model::IExecutionContext &context) override;

    bool isComplete(SCXML::Model::IExecutionContext &context) const override;
    std::set<std::string> getActiveChildStates(SCXML::Model::IExecutionContext &context) const override;

    SCXML::Common::Result<std::set<std::string>> processEventInParallel(SCXML::Model::IExecutionContext &context,
                                                                        const std::string &eventName) override;

    std::vector<std::string> validate() const override;
    std::shared_ptr<IParallelNode> clone() const override;

    const std::string &getCompletionCondition() const override;
    void setCompletionCondition(const std::string &condition) override;

private:
    std::string id_;                        ///< Parallel state identifier
    std::vector<std::string> childStates_;  ///< Child state identifiers
    std::string completionCondition_;       ///< Completion condition expression
    mutable std::mutex stateMutex_;         ///< Mutex for thread-safe access

    /**
     * @brief Enter a specific child state
     * @param context Runtime context
     * @param childStateId Child state to enter
     * @return Result indicating success or failure
     */
    SCXML::Common::Result<void> enterChildState(SCXML::Runtime::RuntimeContext &context,
                                                const std::string &childStateId);

    /**
     * @brief Exit a specific child state
     * @param context Runtime context
     * @param childStateId Child state to exit
     * @return Result indicating success or failure
     */
    SCXML::Common::Result<void> exitChildState(SCXML::Runtime::RuntimeContext &context,
                                               const std::string &childStateId);

    /**
     * @brief Check if a specific child state is complete
     * @param context Runtime context
     * @param childStateId Child state to check
     * @return True if child state is complete
     */
    bool isChildStateComplete(SCXML::Runtime::RuntimeContext &context, const std::string &childStateId) const;

    /**
     * @brief Evaluate completion condition
     * @param context Runtime context
     * @return True if completion condition is met
     */
    bool evaluateCompletionCondition(SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Process event for a single child state
     * @param context Runtime context
     * @param childStateId Child state ID
     * @param eventName Event name
     * @return Future containing processing result
     */
    std::future<bool> processEventForChild(SCXML::Runtime::RuntimeContext &context, const std::string &childStateId,
                                           const std::string &eventName);
};

/**
 * @brief Concrete implementation of Parallel State Manager
 *
 * Provides centralized management of all parallel states and
 * coordinates their concurrent execution.
 */
class ParallelStateManager : public IParallelStateManager {
public:
    /**
     * @brief Constructor
     */
    ParallelStateManager();

    /**
     * @brief Destructor
     */
    virtual ~ParallelStateManager() = default;

    // IParallelStateManager interface implementation
    SCXML::Common::Result<void> registerParallelState(std::shared_ptr<IParallelNode> parallelState) override;

    SCXML::Common::Result<void> enterParallelStates(SCXML::Runtime::RuntimeContext &context,
                                                    const std::set<std::string> &statesToEnter) override;

    SCXML::Common::Result<void> exitParallelStates(SCXML::Runtime::RuntimeContext &context,
                                                   const std::set<std::string> &statesToExit) override;

    SCXML::Common::Result<std::map<std::string, bool>>
    processEventAcrossParallelStates(SCXML::Runtime::RuntimeContext &context, const std::string &eventName) override;

    std::map<std::string, bool> checkParallelStateCompletions(SCXML::Runtime::RuntimeContext &context) const override;

    std::set<std::string> getActiveParallelStates(SCXML::Runtime::RuntimeContext &context) const override;

    std::shared_ptr<IParallelNode> findParallelState(const std::string &parallelStateId) const override;
    std::vector<std::shared_ptr<IParallelNode>> getAllParallelStates() const override;
    std::vector<std::string> validateAllParallelStates() const override;
    std::string exportParallelStateInfo(SCXML::Runtime::RuntimeContext &context) const override;

    void
    setEventProcessingCallback(std::function<void(const std::string &, const std::string &, bool)> callback) override;

private:
    std::map<std::string, std::shared_ptr<IParallelNode>> parallelStates_;               ///< Registered parallel states
    std::map<std::string, SCXML::Model::ParallelExecutionContext> executionContexts_;                  ///< Execution contexts
    std::function<void(const std::string &, const std::string &, bool)> eventCallback_;  ///< Event callback
    mutable std::mutex managerMutex_;                                                    ///< Thread-safety mutex

    /**
     * @brief Create or update execution context for a parallel state
     * @param parallelStateId Parallel state ID
     * @return Reference to execution context
     */
    ParallelExecutionContext &getOrCreateExecutionContext(const std::string &parallelStateId);

    /**
     * @brief Remove execution context for a parallel state
     * @param parallelStateId Parallel state ID
     */
    void removeExecutionContext(const std::string &parallelStateId);

    /**
     * @brief Check if a state is currently active in the runtime
     * @param context Runtime context
     * @param stateId State ID to check
     * @return True if state is active
     */
    bool isStateActive(SCXML::Runtime::RuntimeContext &context, const std::string &stateId) const;

    /**
     * @brief Get all states that are children of parallel states
     * @param parallelStates Set of parallel state IDs
     * @return Set of all child state IDs
     */
    std::set<std::string> getAllChildStates(const std::set<std::string> &parallelStates) const;

    /**
     * @brief Update execution context after event processing
     * @param parallelStateId Parallel state ID
     * @param context Runtime context
     */
    void updateExecutionContext(const std::string &parallelStateId, SCXML::Runtime::RuntimeContext &context);
};

/**
 * @brief Parallel event processor for handling concurrent event execution
 */
class ParallelEventProcessor {
public:
    /**
     * @brief Process event concurrently across multiple states
     * @param context Runtime context
     * @param stateIds States to process event for
     * @param eventName Event name
     * @param maxConcurrency Maximum number of concurrent threads (0 = unlimited)
     * @return Map of state ID to processing result
     */
    static std::map<std::string, bool> processEventConcurrently(SCXML::Runtime::RuntimeContext &context,
                                                                const std::vector<std::string> &stateIds,
                                                                const std::string &eventName,
                                                                size_t maxConcurrency = 0);

    /**
     * @brief Process event with timeout
     * @param context Runtime context
     * @param stateId State to process event for
     * @param eventName Event name
     * @param timeoutMs Timeout in milliseconds
     * @return Processing result with timeout information
     */
    static std::pair<bool, bool> processEventWithTimeout(SCXML::Runtime::RuntimeContext &context,
                                                         const std::string &stateId, const std::string &eventName,
                                                         std::chrono::milliseconds timeoutMs);

private:
    /**
     * @brief Worker function for concurrent event processing
     * @param context Runtime context
     * @param stateId State ID
     * @param eventName Event name
     * @return Processing result
     */
    static bool processEventForState(SCXML::Runtime::RuntimeContext &context, const std::string &stateId,
                                     const std::string &eventName);
};

} // namespace Core
}  // namespace SCXML
