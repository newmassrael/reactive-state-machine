#include "core/ParallelNode.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <thread>

namespace SCXML {
namespace Core {

// ============================================================================
// ParallelNode Implementation
// ============================================================================

ParallelNode::ParallelNode(const std::string &id) : id_(id), completionCondition_("") {}

const std::string &ParallelNode::getId() const {
    return id_;
}

void ParallelNode::setId(const std::string &id) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    id_ = id;
}

void ParallelNode::addChildState(const std::string &childStateId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (std::find(childStates_.begin(), childStates_.end(), childStateId) == childStates_.end()) {
        childStates_.push_back(childStateId);
    }
}

bool ParallelNode::removeChildState(const std::string &childStateId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = std::find(childStates_.begin(), childStates_.end(), childStateId);
    if (it != childStates_.end()) {
        childStates_.erase(it);
        return true;
    }
    return false;
}

std::vector<std::string> ParallelNode::getChildStates() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return childStates_;
}

bool ParallelNode::isChildState(const std::string &stateId) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return std::find(childStates_.begin(), childStates_.end(), stateId) != childStates_.end();
}

SCXML::Common::Result<void> ParallelNode::enter(SCXML::Model::IExecutionContext &context) {
    try {
        std::lock_guard<std::mutex> lock(stateMutex_);

        context.getLogger().info("Entering parallel state: " + id_);

        // Enter all child states concurrently
        std::vector<std::future<SCXML::Common::Result<void>>> futures;

        for (const auto &childStateId : childStates_) {
            futures.push_back(std::async(std::launch::async, [this, &context, childStateId]() {
                return enterChildState(context, childStateId);
            }));
        }

        // Wait for all child states to enter and collect results
        for (auto &future : futures) {
            auto result = future.get();
            if (!result.isSuccess()) {
                return SCXML::Common::Result<void>::failure("Failed to enter child state in parallel state " + id_ +
                                                            ": " + result.getError());
            }
        }

        context.getLogger().info("Successfully entered parallel state: " + id_ + " with " +
                                 std::to_string(childStates_.size()) + " child states");

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Exception entering parallel state " + id_ + ": " +
                                                    std::string(e.what()));
    }
}

SCXML::Common::Result<void> ParallelNode::exit(SCXML::Model::IExecutionContext &context) {
    try {
        std::lock_guard<std::mutex> lock(stateMutex_);

        context.getLogger().info("Exiting parallel state: " + id_);

        // Exit all child states concurrently
        std::vector<std::future<SCXML::Common::Result<void>>> futures;

        for (const auto &childStateId : childStates_) {
            futures.push_back(std::async(std::launch::async, [this, &context, childStateId]() {
                return exitChildState(context, childStateId);
            }));
        }

        // Wait for all child states to exit and collect results
        for (auto &future : futures) {
            auto result = future.get();
            if (!result.isSuccess()) {
                context.getLogger().warning("Failed to exit child state in parallel state " + id_ + ": " +
                                            result.getError());
                // Continue with other children even if one fails
            }
        }

        context.getLogger().info("Successfully exited parallel state: " + id_);

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Exception exiting parallel state " + id_ + ": " +
                                                    std::string(e.what()));
    }
}

bool ParallelNode::isComplete(SCXML::Model::IExecutionContext &context) const {
    try {
        std::lock_guard<std::mutex> lock(stateMutex_);

        // If there's a completion condition, evaluate it
        if (!completionCondition_.empty()) {
            return evaluateCompletionCondition(context);
        }

        // Default: complete when all child states are complete
        for (const auto &childStateId : childStates_) {
            if (!isChildStateComplete(context, childStateId)) {
                return false;
            }
        }

        return true;

    } catch (const std::exception &e) {
        context.getLogger().error("Exception checking parallel state completion: " + std::string(e.what()));
        return false;
    }
}

std::set<std::string> ParallelNode::getActiveChildStates(SCXML::Model::IExecutionContext &context) const {
    std::set<std::string> activeChildren;

    try {
        std::lock_guard<std::mutex> lock(stateMutex_);

        for (const auto &childStateId : childStates_) {
            // Simplified check - in real implementation would query actual state machine
            if (!isChildStateComplete(context, childStateId)) {
                activeChildren.insert(childStateId);
            }
        }

    } catch (const std::exception &e) {
        context.getLogger().error("Exception getting active child states: " + std::string(e.what()));
    }

    return activeChildren;
}

SCXML::Common::Result<std::set<std::string>>
ParallelNode::processEventInParallel(SCXML::Model::IExecutionContext &context, const std::string &eventName) {
    try {
        std::lock_guard<std::mutex> lock(stateMutex_);

        context.getLogger().debug("Processing event '" + eventName + "' in parallel state: " + id_);

        // Process event concurrently across all active child states
        std::vector<std::future<bool>> futures;
        std::vector<std::string> processingStates;

        for (const auto &childStateId : childStates_) {
            if (!isChildStateComplete(context, childStateId)) {
                futures.push_back(processEventForChild(context, childStateId, eventName));
                processingStates.push_back(childStateId);
            }
        }

        // Collect results
        std::set<std::string> handlingStates;
        for (size_t i = 0; i < futures.size(); ++i) {
            try {
                if (futures[i].get()) {
                    handlingStates.insert(processingStates[i]);
                }
            } catch (const std::exception &e) {
                context.getLogger().warning("Event processing failed for child state " + processingStates[i] + ": " +
                                            std::string(e.what()));
            }
        }

        context.getLogger().debug("Event '" + eventName + "' handled by " + std::to_string(handlingStates.size()) +
                                  " child states in " + id_);

        return SCXML::Common::Result<std::set<std::string>>::success(handlingStates);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::set<std::string>>::failure("Exception processing event in parallel state " +
                                                                     id_ + ": " + std::string(e.what()));
    }
}

std::vector<std::string> ParallelNode::validate() const {
    std::vector<std::string> errors;

    // Parallel state ID is required
    if (id_.empty()) {
        errors.push_back("Parallel state must have an ID");
    }

    // Must have at least one child state
    if (childStates_.empty()) {
        errors.push_back("Parallel state must have at least one child state");
    }

    // Child state IDs must be unique
    std::set<std::string> uniqueChildren(childStates_.begin(), childStates_.end());
    if (uniqueChildren.size() != childStates_.size()) {
        errors.push_back("Parallel state has duplicate child state IDs");
    }

    // Validate completion condition if present
    if (!completionCondition_.empty()) {
        // Simple syntax check - in real implementation would use expression parser
        if (completionCondition_.find_first_not_of("abcdefghijklmnopqrstuvwxyz"
                                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                   "0123456789._() &|!<>=") != std::string::npos) {
            errors.push_back("Invalid completion condition syntax");
        }
    }

    return errors;
}

std::shared_ptr<IParallelNode> ParallelNode::clone() const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    auto cloned = std::make_shared<ParallelNode>(id_);
    cloned->childStates_ = childStates_;
    cloned->completionCondition_ = completionCondition_;
    return cloned;
}

const std::string &ParallelNode::getCompletionCondition() const {
    return completionCondition_;
}

void ParallelNode::setCompletionCondition(const std::string &condition) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    completionCondition_ = condition;
}

SCXML::Common::Result<void> ParallelNode::enterChildState(SCXML::Runtime::RuntimeContext &context,
                                                          const std::string &childStateId) {
    try {
        context.getLogger().debug("Entering child state: " + childStateId + " in parallel state: " + id_);

        // In real implementation, this would interact with the state machine to enter the child state
        // For now, we'll just log and mark as successful

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Exception entering child state " + childStateId + ": " +
                                                    std::string(e.what()));
    }
}

SCXML::Common::Result<void> ParallelNode::exitChildState(SCXML::Runtime::RuntimeContext &context,
                                                         const std::string &childStateId) {
    try {
        context.getLogger().debug("Exiting child state: " + childStateId + " in parallel state: " + id_);

        // In real implementation, this would interact with the state machine to exit the child state
        // For now, we'll just log and mark as successful

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Exception exiting child state " + childStateId + ": " +
                                                    std::string(e.what()));
    }
}

bool ParallelNode::isChildStateComplete(SCXML::Runtime::RuntimeContext &context,
                                        const std::string &childStateId) const {
    try {
        // Check if child state is actually active in the runtime context
        if (!context.isStateActive(childStateId)) {
            return true;  // Inactive states are considered complete
        }

        // Get the state machine model to check state properties
        auto model = context.getModel();
        if (model) {
            auto stateNode = model->findStateById(childStateId);
            if (stateNode) {
                // Check if it's a final state
                if (stateNode->isFinalState()) {
                    return true;
                }

                // For compound states, check if all children are complete
                const auto &children = stateNode->getChildren();
                if (!children.empty()) {
                    for (const auto &child : children) {
                        if (child && context.isStateActive(child->getId())) {
                            // If any child is still active, compound state is not complete
                            if (!isChildStateComplete(context, child->getId())) {
                                return false;
                            }
                        }
                    }
                    return true;  // All children are complete
                }

                // For atomic states that are active, they are not complete unless final
                return false;
            }
        }

        // Fallback: if we can't determine state type, check name pattern
        // This maintains backward compatibility but logs a warning
        context.getLogger().warn("ParallelNode: Using fallback name-based completion check for state: " + childStateId);
        return childStateId.find("final") != std::string::npos;

    } catch (const std::exception &e) {
        context.getLogger().error("Exception checking child state completion: " + std::string(e.what()));
        return false;
    }
}

bool ParallelNode::evaluateCompletionCondition(SCXML::Runtime::RuntimeContext &context) const {
    try {
        if (completionCondition_.empty()) {
            return false;
        }

        EnhancedExpressionEvaluator evaluator;
        auto result = evaluator.evaluateBooleanExpression(context, completionCondition_);

        if (result.isSuccess()) {
            return result.getValue();
        } else {
            context.getLogger().error("Failed to evaluate completion condition: " + result.getError());
            return false;
        }

    } catch (const std::exception &e) {
        context.getLogger().error("Exception evaluating completion condition: " + std::string(e.what()));
        return false;
    }
}

std::future<bool> ParallelNode::processEventForChild(SCXML::Runtime::RuntimeContext &context,
                                                     const std::string &childStateId, const std::string &eventName) {
    return std::async(std::launch::async, [&context, childStateId, eventName]() -> bool {
        try {
            // Simplified event processing - in real implementation would delegate to state machine
            context.getLogger().debug("Processing event '" + eventName + "' for child state: " + childStateId);

            // Simulate some processing time
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // Return success for demonstration
            return true;

        } catch (const std::exception &e) {
            return false;
        }
    });
}

// ============================================================================
// ParallelStateManager Implementation
// ============================================================================

ParallelStateManager::ParallelStateManager() {}

SCXML::Common::Result<void> ParallelStateManager::registerParallelState(std::shared_ptr<IParallelNode> parallelState) {
    if (!parallelState) {
        return SCXML::Common::Result<void>::failure("Cannot register null parallel state");
    }

    std::lock_guard<std::mutex> lock(managerMutex_);

    const std::string &id = parallelState->getId();
    if (id.empty()) {
        return SCXML::Common::Result<void>::failure("Parallel state must have an ID");
    }

    if (parallelStates_.count(id) > 0) {
        return SCXML::Common::Result<void>::failure("Parallel state ID already registered: " + id);
    }

    // Validate the parallel state
    auto validationErrors = parallelState->validate();
    if (!validationErrors.empty()) {
        std::string errorMsg = "Parallel state validation failed: ";
        for (const auto &error : validationErrors) {
            errorMsg += error + "; ";
        }
        return SCXML::Common::Result<void>::failure(errorMsg);
    }

    parallelStates_[id] = parallelState;
    return SCXML::Common::Result<void>::success();
}

SCXML::Common::Result<void> ParallelStateManager::enterParallelStates(SCXML::Runtime::RuntimeContext &context,
                                                                      const std::set<std::string> &statesToEnter) {
    try {
        std::lock_guard<std::mutex> lock(managerMutex_);

        for (const auto &stateId : statesToEnter) {
            auto it = parallelStates_.find(stateId);
            if (it != parallelStates_.end()) {
                auto result = it->second->enter(context);
                if (!result.isSuccess()) {
                    return SCXML::Common::Result<void>::failure("Failed to enter parallel state " + stateId + ": " +
                                                                result.getError());
                }

                // Create execution context
                getOrCreateExecutionContext(stateId);
            }
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Exception entering parallel states: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> ParallelStateManager::exitParallelStates(SCXML::Runtime::RuntimeContext &context,
                                                                     const std::set<std::string> &statesToExit) {
    try {
        std::lock_guard<std::mutex> lock(managerMutex_);

        for (const auto &stateId : statesToExit) {
            auto it = parallelStates_.find(stateId);
            if (it != parallelStates_.end()) {
                auto result = it->second->exit(context);
                if (!result.isSuccess()) {
                    context.getLogger().warning("Failed to exit parallel state " + stateId + ": " + result.getError());
                    // Continue with other states even if one fails
                }

                // Remove execution context
                removeExecutionContext(stateId);
            }
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Exception exiting parallel states: " + std::string(e.what()));
    }
}

SCXML::Common::Result<std::map<std::string, bool>>
ParallelStateManager::processEventAcrossParallelStates(SCXML::Runtime::RuntimeContext &context,
                                                       const std::string &eventName) {
    try {
        std::lock_guard<std::mutex> lock(managerMutex_);

        std::map<std::string, bool> results;

        // Get active parallel states
        auto activeParallelStates = getActiveParallelStates(context);

        // Process event concurrently across all active parallel states
        std::vector<std::future<std::pair<std::string, std::set<std::string>>>> futures;

        for (const auto &parallelStateId : activeParallelStates) {
            auto it = parallelStates_.find(parallelStateId);
            if (it != parallelStates_.end()) {
                futures.push_back(std::async(
                    std::launch::async, [parallelStateId, &parallelState = it->second, &context, &eventName]() {
                        auto result = parallelState->processEventInParallel(context, eventName);
                        std::set<std::string> handlingStates;
                        if (result.isSuccess()) {
                            handlingStates = result.getValue();
                        }
                        return std::make_pair(parallelStateId, handlingStates);
                    }));
            }
        }

        // Collect results
        for (auto &future : futures) {
            try {
                auto [parallelStateId, handlingStates] = future.get();
                bool handled = !handlingStates.empty();
                results[parallelStateId] = handled;

                // Update execution context
                updateExecutionContext(parallelStateId, context);

                // Call callback if set
                if (eventCallback_) {
                    eventCallback_(parallelStateId, eventName, handled);
                }

            } catch (const std::exception &e) {
                context.getLogger().error("Event processing failed for parallel state: " + std::string(e.what()));
            }
        }

        return SCXML::Common::Result<std::map<std::string, bool>>::success(results);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::map<std::string, bool>>::failure(
            "Exception processing event across parallel states: " + std::string(e.what()));
    }
}

std::map<std::string, bool>
ParallelStateManager::checkParallelStateCompletions(SCXML::Runtime::RuntimeContext &context) const {
    std::map<std::string, bool> completions;

    try {
        std::lock_guard<std::mutex> lock(managerMutex_);

        for (const auto &pair : parallelStates_) {
            const std::string &parallelStateId = pair.first;
            const auto &parallelState = pair.second;

            if (isStateActive(context, parallelStateId)) {
                completions[parallelStateId] = parallelState->isComplete(context);
            }
        }

    } catch (const std::exception &e) {
        context.getLogger().error("Exception checking parallel state completions: " + std::string(e.what()));
    }

    return completions;
}

std::set<std::string> ParallelStateManager::getActiveParallelStates(SCXML::Runtime::RuntimeContext &context) const {
    std::set<std::string> activeStates;

    try {
        std::lock_guard<std::mutex> lock(managerMutex_);

        for (const auto &pair : parallelStates_) {
            const std::string &parallelStateId = pair.first;
            if (isStateActive(context, parallelStateId)) {
                activeStates.insert(parallelStateId);
            }
        }

    } catch (const std::exception &e) {
        context.getLogger().error("Exception getting active parallel states: " + std::string(e.what()));
    }

    return activeStates;
}

std::shared_ptr<IParallelNode> ParallelStateManager::findParallelState(const std::string &parallelStateId) const {
    std::lock_guard<std::mutex> lock(managerMutex_);

    auto it = parallelStates_.find(parallelStateId);
    return it != parallelStates_.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<IParallelNode>> ParallelStateManager::getAllParallelStates() const {
    std::lock_guard<std::mutex> lock(managerMutex_);

    std::vector<std::shared_ptr<IParallelNode>> states;
    for (const auto &pair : parallelStates_) {
        states.push_back(pair.second);
    }
    return states;
}

std::vector<std::string> ParallelStateManager::validateAllParallelStates() const {
    std::lock_guard<std::mutex> lock(managerMutex_);

    std::vector<std::string> allErrors;

    for (const auto &pair : parallelStates_) {
        auto errors = pair.second->validate();
        for (const auto &error : errors) {
            allErrors.push_back(pair.first + ": " + error);
        }
    }

    return allErrors;
}

std::string ParallelStateManager::exportParallelStateInfo(SCXML::Runtime::RuntimeContext &context) const {
    std::lock_guard<std::mutex> lock(managerMutex_);

    std::ostringstream json;
    json << "{\"parallelStates\":[";

    bool first = true;
    for (const auto &pair : parallelStates_) {
        if (!first) {
            json << ",";
        }
        first = false;

        const std::string &parallelStateId = pair.first;
        const auto &parallelState = pair.second;

        json << "{";
        json << "\"id\":\"" << parallelStateId << "\",";
        json << "\"isActive\":" << (isStateActive(context, parallelStateId) ? "true" : "false") << ",";
        json << "\"isComplete\":" << (parallelState->isComplete(context) ? "true" : "false") << ",";
        json << "\"childStates\":[";

        auto childStates = parallelState->getChildStates();
        for (size_t i = 0; i < childStates.size(); ++i) {
            if (i > 0) {
                json << ",";
            }
            json << "\"" << childStates[i] << "\"";
        }
        json << "],";

        json << "\"completionCondition\":\"" << parallelState->getCompletionCondition() << "\"";

        // Add execution context if available
        auto contextIt = executionContexts_.find(parallelStateId);
        if (contextIt != executionContexts_.end()) {
            json << ",\"executionContext\":" << contextIt->second.toJSON();
        }

        json << "}";
    }

    json << "]}";
    return json.str();
}

void ParallelStateManager::setEventProcessingCallback(
    std::function<void(const std::string &, const std::string &, bool)> callback) {
    std::lock_guard<std::mutex> lock(managerMutex_);
    eventCallback_ = callback;
}

ParallelExecutionContext &ParallelStateManager::getOrCreateExecutionContext(const std::string &parallelStateId) {
    auto it = executionContexts_.find(parallelStateId);
    if (it != executionContexts_.end()) {
        return it->second;
    }

    // Create new execution context
    auto result = executionContexts_.emplace(parallelStateId, ParallelExecutionContext(parallelStateId));
    return result.first->second;
}

void ParallelStateManager::removeExecutionContext(const std::string &parallelStateId) {
    executionContexts_.erase(parallelStateId);
}

bool ParallelStateManager::isStateActive(SCXML::Runtime::RuntimeContext &context, const std::string &stateId) const {
    // Simplified implementation - in real implementation would query actual state machine
    // For now, assume state is active if it's in the execution contexts
    return executionContexts_.count(stateId) > 0;
}

std::set<std::string> ParallelStateManager::getAllChildStates(const std::set<std::string> &parallelStates) const {
    std::set<std::string> allChildStates;

    for (const auto &parallelStateId : parallelStates) {
        auto it = parallelStates_.find(parallelStateId);
        if (it != parallelStates_.end()) {
            auto childStates = it->second->getChildStates();
            allChildStates.insert(childStates.begin(), childStates.end());
        }
    }

    return allChildStates;
}

void ParallelStateManager::updateExecutionContext(const std::string &parallelStateId,
                                                  SCXML::Runtime::RuntimeContext &context) {
    auto it = executionContexts_.find(parallelStateId);
    if (it != executionContexts_.end()) {
        it->second.processedEvents++;

        // Update active children and completion status
        auto parallelState = findParallelState(parallelStateId);
        if (parallelState) {
            it->second.activeChildren = parallelState->getActiveChildStates(context);

            // Update completion status for each child
            for (const auto &childId : parallelState->getChildStates()) {
                // Simplified completion check
                it->second.childCompletionStatus[childId] = childId.find("final") != std::string::npos;
            }
        }
    }
}

// ============================================================================
// ParallelEventProcessor Implementation
// ============================================================================

std::map<std::string, bool> ParallelEventProcessor::processEventConcurrently(SCXML::Runtime::RuntimeContext &context,
                                                                             const std::vector<std::string> &stateIds,
                                                                             const std::string &eventName,
                                                                             size_t maxConcurrency) {
    std::map<std::string, bool> results;

    if (stateIds.empty()) {
        return results;
    }

    // Determine actual concurrency level
    size_t concurrency = maxConcurrency > 0 ? std::min(maxConcurrency, stateIds.size()) : stateIds.size();

    std::vector<std::future<std::pair<std::string, bool>>> futures;

    // Process in batches if concurrency is limited
    for (size_t i = 0; i < stateIds.size(); i += concurrency) {
        size_t batchEnd = std::min(i + concurrency, stateIds.size());

        // Launch batch of futures
        for (size_t j = i; j < batchEnd; ++j) {
            const std::string &stateId = stateIds[j];

            futures.push_back(std::async(std::launch::async, [&context, stateId, eventName]() {
                bool result = processEventForState(context, stateId, eventName);
                return std::make_pair(stateId, result);
            }));
        }

        // Wait for batch completion if we have more batches to process
        if (batchEnd < stateIds.size()) {
            // Collect current batch results
            for (size_t k = 0; k < (batchEnd - i); ++k) {
                try {
                    auto [stateId, result] = futures[futures.size() - (batchEnd - i) + k].get();
                    results[stateId] = result;
                } catch (const std::exception &e) {
                    context.getLogger().error("Concurrent event processing failed: " + std::string(e.what()));
                }
            }
        }
    }

    // Collect any remaining results
    size_t remainingStart = results.size();
    for (size_t i = remainingStart; i < futures.size(); ++i) {
        try {
            auto [stateId, result] = futures[i].get();
            results[stateId] = result;
        } catch (const std::exception &e) {
            context.getLogger().error("Concurrent event processing failed: " + std::string(e.what()));
        }
    }

    return results;
}

std::pair<bool, bool> ParallelEventProcessor::processEventWithTimeout(SCXML::Runtime::RuntimeContext &context,
                                                                      const std::string &stateId,
                                                                      const std::string &eventName,
                                                                      std::chrono::milliseconds timeoutMs) {
    auto future = std::async(std::launch::async, [&context, stateId, eventName]() {
        return processEventForState(context, stateId, eventName);
    });

    auto status = future.wait_for(timeoutMs);

    if (status == std::future_status::ready) {
        try {
            bool result = future.get();
            return {result, false};  // {processing_result, timed_out}
        } catch (const std::exception &e) {
            context.getLogger().error("Event processing failed: " + std::string(e.what()));
            return {false, false};
        }
    } else {
        // Timed out
        return {false, true};
    }
}

bool ParallelEventProcessor::processEventForState(SCXML::Runtime::RuntimeContext &context, const std::string &stateId,
                                                  const std::string &eventName) {
    try {
        // Simplified event processing - in real implementation would delegate to state machine
        context.getLogger().debug("Processing event '" + eventName + "' for state: " + stateId);

        // Simulate processing time
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Return success for demonstration
        return true;

    } catch (const std::exception &e) {
        context.getLogger().error("Event processing failed for state " + stateId + ": " + std::string(e.what()));
        return false;
    }
}

}  // namespace Core
}  // namespace SCXML
