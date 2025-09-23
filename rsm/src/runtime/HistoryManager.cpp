#include "runtime/HistoryManager.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include "runtime/HistoryValidator.h"
#include "runtime/IHistoryManager.h"
#include <algorithm>

namespace RSM {

HistoryManager::HistoryManager(std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider,
                               std::unique_ptr<IHistoryStateFilter> shallowFilter,
                               std::unique_ptr<IHistoryStateFilter> deepFilter,
                               std::unique_ptr<IHistoryValidator> validator)
    : stateProvider_(std::move(stateProvider)), shallowFilter_(std::move(shallowFilter)),
      deepFilter_(std::move(deepFilter)), validator_(std::move(validator)) {
    Logger::info("HistoryManager: Initialized with SOLID architecture components");
}

bool HistoryManager::registerHistoryState(const std::string &historyStateId, const std::string &parentStateId,
                                          HistoryType type, const std::string &defaultStateId) {
    Logger::info("HistoryManager: Registering history state - " + historyStateId + " for parent " + parentStateId);

    std::lock_guard<std::mutex> lock(historyMutex_);

    // Validate registration using injected validator
    // Use enhanced validation if default state is provided
    bool validationResult;
    if (!defaultStateId.empty()) {
        if (auto historyValidator = dynamic_cast<HistoryValidator *>(validator_.get())) {
            validationResult =
                historyValidator->validateRegistrationWithDefault(historyStateId, parentStateId, type, defaultStateId);
        } else {
            // Fallback to basic validation if not our HistoryValidator
            validationResult = validator_->validateRegistration(historyStateId, parentStateId, type);
        }
    } else {
        validationResult = validator_->validateRegistration(historyStateId, parentStateId, type);
    }

    if (!validationResult) {
        Logger::error("HistoryManager: Registration validation failed for " + historyStateId);
        return false;
    }

    // Create history state info
    HistoryStateInfo info;
    info.historyStateId = historyStateId;
    info.parentStateId = parentStateId;
    info.type = type;
    info.defaultStateId = defaultStateId;
    info.registrationTime = std::chrono::steady_clock::now();

    // Register the history state
    historyStates_[historyStateId] = info;

    // Update validator tracking (if it's our HistoryValidator)
    if (auto historyValidator = dynamic_cast<HistoryValidator *>(validator_.get())) {
        historyValidator->registerHistoryStateId(historyStateId);
        historyValidator->registerParentType(parentStateId, type);
    }

    std::string typeStr = (type == HistoryType::SHALLOW) ? "shallow" : "deep";
    Logger::info("HistoryManager: Successfully registered " + typeStr + " history state: " + historyStateId +
                 " for parent: " + parentStateId);

    return true;
}

bool HistoryManager::recordHistory(const std::string &parentStateId, const std::vector<std::string> &activeStateIds) {
    Logger::info("HistoryManager: Recording history for parent " + parentStateId + " with " +
                 std::to_string(activeStateIds.size()) + " active states");

    std::lock_guard<std::mutex> lock(historyMutex_);

    // Validate recording using injected validator
    if (!validator_->validateRecording(parentStateId, activeStateIds)) {
        Logger::error("HistoryManager: Recording validation failed for " + parentStateId);
        return false;
    }

    bool recordedAny = false;

    // Find all history states for this parent
    auto historyStatesForParent = findHistoryStatesForParent(parentStateId);

    for (const auto &historyInfo : historyStatesForParent) {
        // Filter states based on history type using injected filters
        auto &filter = getFilter(historyInfo.type);
        auto filteredStates = filter.filterStates(activeStateIds, parentStateId);

        // W3C SCXML Section 3.6: Record history even if empty (valid scenario)
        HistoryEntry entry;
        entry.parentStateId = parentStateId;
        entry.type = historyInfo.type;
        entry.recordedStateIds = filteredStates;
        entry.timestamp = std::chrono::steady_clock::now();
        entry.isValid = true;

        recordedHistory_[historyInfo.historyStateId] = entry;
        recordedAny = true;

        std::string typeStr = (historyInfo.type == HistoryType::SHALLOW) ? "shallow" : "deep";
        Logger::info("HistoryManager: Recorded " + typeStr + " history with " + std::to_string(filteredStates.size()) +
                     " states for " + historyInfo.historyStateId);
    }

    if (!recordedAny) {
        Logger::debug("HistoryManager: No history states found or no states to record for " + parentStateId);
    }

    return recordedAny;
}

HistoryRestorationResult HistoryManager::restoreHistory(const std::string &historyStateId) {
    Logger::info("HistoryManager: Restoring history for " + historyStateId);

    std::lock_guard<std::mutex> lock(historyMutex_);

    // Validate restoration using injected validator
    if (!validator_->validateRestoration(historyStateId)) {
        return HistoryRestorationResult::createError("Restoration validation failed for " + historyStateId);
    }

    // Find history state info
    auto it = historyStates_.find(historyStateId);
    if (it == historyStates_.end()) {
        return HistoryRestorationResult::createError("History state not found: " + historyStateId);
    }

    const auto &historyInfo = it->second;

    // Check if we have recorded history
    auto historyIt = recordedHistory_.find(historyStateId);
    if (historyIt != recordedHistory_.end() && historyIt->second.isValid) {
        // Restore from recorded history
        const auto &entry = historyIt->second;
        Logger::info("HistoryManager: Restoring " + std::to_string(entry.recordedStateIds.size()) +
                     " recorded states for " + historyStateId);
        return HistoryRestorationResult::createSuccess(entry.recordedStateIds);
    } else {
        // Use default states
        auto defaultStates = getDefaultStates(historyInfo);
        Logger::info("HistoryManager: No recorded history found, using " + std::to_string(defaultStates.size()) +
                     " default states for " + historyStateId);
        return HistoryRestorationResult::createSuccess(defaultStates);
    }
}

bool HistoryManager::isHistoryState(const std::string &stateId) const {
    std::lock_guard<std::mutex> lock(historyMutex_);
    return historyStates_.find(stateId) != historyStates_.end();
}

void HistoryManager::clearAllHistory() {
    std::lock_guard<std::mutex> lock(historyMutex_);
    recordedHistory_.clear();
    Logger::info("HistoryManager: Cleared all recorded history");
}

std::vector<HistoryEntry> HistoryManager::getHistoryEntries() const {
    std::lock_guard<std::mutex> lock(historyMutex_);

    std::vector<HistoryEntry> entries;
    entries.reserve(recordedHistory_.size());

    for (const auto &pair : recordedHistory_) {
        entries.push_back(pair.second);
    }

    Logger::debug("HistoryManager: Retrieved " + std::to_string(entries.size()) + " history entries");
    return entries;
}

IHistoryStateFilter &HistoryManager::getFilter(HistoryType type) const {
    if (type == HistoryType::SHALLOW) {
        return *shallowFilter_;
    } else {
        return *deepFilter_;
    }
}

std::vector<HistoryManager::HistoryStateInfo>
HistoryManager::findHistoryStatesForParent(const std::string &parentStateId) const {
    std::vector<HistoryStateInfo> result;

    for (const auto &pair : historyStates_) {
        if (pair.second.parentStateId == parentStateId) {
            result.push_back(pair.second);
        }
    }

    Logger::debug("HistoryManager: Found " + std::to_string(result.size()) + " history states for parent " +
                  parentStateId);
    return result;
}

std::vector<std::string> HistoryManager::getDefaultStates(const HistoryStateInfo &historyStateInfo) const {
    std::vector<std::string> defaultStates;

    if (!historyStateInfo.defaultStateId.empty()) {
        defaultStates.push_back(historyStateInfo.defaultStateId);
        Logger::debug("HistoryManager: Using explicit default state: " + historyStateInfo.defaultStateId);
    } else {
        // If no explicit default, try to find the initial state of the parent
        if (stateProvider_) {
            auto parentState = stateProvider_(historyStateInfo.parentStateId);
            if (parentState) {
                std::string initialState = parentState->getInitialState();
                if (!initialState.empty()) {
                    defaultStates.push_back(initialState);
                    Logger::debug("HistoryManager: Using parent's initial state as default: " + initialState);
                } else {
                    // Fallback: use first child state
                    const auto &children = parentState->getChildren();
                    if (!children.empty()) {
                        defaultStates.push_back(children[0]->getId());
                        Logger::debug("HistoryManager: Using first child as default: " + children[0]->getId());
                    }
                }
            }
        }
    }

    if (defaultStates.empty()) {
        Logger::warn("HistoryManager: No default states available for " + historyStateInfo.historyStateId);
    }

    return defaultStates;
}

}  // namespace RSM