#include "runtime/HistoryManager.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include "runtime/HistoryValidator.h"

#include <algorithm>

namespace RSM {

HistoryManager::HistoryManager(std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider,
                               std::unique_ptr<IHistoryStateFilter> shallowFilter,
                               std::unique_ptr<IHistoryStateFilter> deepFilter,
                               std::unique_ptr<IHistoryValidator> validator)
    : stateProvider_(std::move(stateProvider)), shallowFilter_(std::move(shallowFilter)),
      deepFilter_(std::move(deepFilter)), validator_(std::move(validator)) {
    LOG_INFO("HistoryManager: Initialized with SOLID architecture components");
}

bool HistoryManager::registerHistoryState(const std::string &historyStateId, const std::string &parentStateId,
                                          HistoryType type, const std::string &defaultStateId) {
    LOG_INFO("HistoryManager: Registering history state - {} for parent {}", historyStateId, parentStateId);

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
        LOG_ERROR("HistoryManager: Registration validation failed for {}", historyStateId);
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
    LOG_INFO("HistoryManager: Successfully registered {} history state: {} for parent: {}", typeStr, historyStateId,
             parentStateId);

    return true;
}

bool HistoryManager::recordHistory(const std::string &parentStateId, const std::vector<std::string> &activeStateIds) {
    LOG_INFO("HistoryManager: Recording history for parent {} with {} active states", parentStateId,
             activeStateIds.size());

    std::lock_guard<std::mutex> lock(historyMutex_);

    // Validate recording using injected validator
    if (!validator_->validateRecording(parentStateId, activeStateIds)) {
        LOG_ERROR("HistoryManager: Recording validation failed for {}", parentStateId);
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
        LOG_INFO("HistoryManager: Recorded {} history with {} states for {}", typeStr, filteredStates.size(),
                 historyInfo.historyStateId);
    }

    if (!recordedAny) {
        LOG_DEBUG("HistoryManager: No history states found or no states to record for {}", parentStateId);
    }

    return recordedAny;
}

HistoryRestorationResult HistoryManager::restoreHistory(const std::string &historyStateId) {
    LOG_INFO("HistoryManager: Restoring history for {}", historyStateId);

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
        LOG_INFO("HistoryManager: Restoring {} recorded states for {}", entry.recordedStateIds.size(), historyStateId);
        return HistoryRestorationResult::createSuccess(entry.recordedStateIds, true);  // fromRecording = true
    } else {
        // Use default states
        auto defaultStates = getDefaultStates(historyInfo);
        LOG_INFO("HistoryManager: No recorded history found, using {} default states for {}", defaultStates.size(),
                 historyStateId);
        return HistoryRestorationResult::createSuccess(defaultStates, false);  // fromRecording = false
    }
}

bool HistoryManager::isHistoryState(const std::string &stateId) const {
    std::lock_guard<std::mutex> lock(historyMutex_);
    return historyStates_.find(stateId) != historyStates_.end();
}

void HistoryManager::clearAllHistory() {
    std::lock_guard<std::mutex> lock(historyMutex_);
    recordedHistory_.clear();
    LOG_INFO("HistoryManager: Cleared all recorded history");
}

std::vector<HistoryEntry> HistoryManager::getHistoryEntries() const {
    std::lock_guard<std::mutex> lock(historyMutex_);

    std::vector<HistoryEntry> entries;
    entries.reserve(recordedHistory_.size());

    for (const auto &pair : recordedHistory_) {
        entries.push_back(pair.second);
    }

    LOG_DEBUG("HistoryManager: Retrieved {} history entries", entries.size());
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

    LOG_DEBUG("HistoryManager: Found {} history states for parent {}", result.size(), parentStateId);
    return result;
}

std::vector<std::string> HistoryManager::getDefaultStates(const HistoryStateInfo &historyStateInfo) const {
    std::vector<std::string> defaultStates;

    if (!historyStateInfo.defaultStateId.empty()) {
        defaultStates.push_back(historyStateInfo.defaultStateId);
        LOG_DEBUG("HistoryManager: Using explicit default state: {}", historyStateInfo.defaultStateId);
    } else {
        // If no explicit default, try to find the initial state of the parent
        if (stateProvider_) {
            auto parentState = stateProvider_(historyStateInfo.parentStateId);
            if (parentState) {
                std::string initialState = parentState->getInitialState();
                if (!initialState.empty()) {
                    defaultStates.push_back(initialState);
                    LOG_DEBUG("HistoryManager: Using parent's initial state as default: {}", initialState);
                } else {
                    // Fallback: use first child state
                    const auto &children = parentState->getChildren();
                    if (!children.empty()) {
                        defaultStates.push_back(children[0]->getId());
                        LOG_DEBUG("HistoryManager: Using first child as default: {}", children[0]->getId());
                    }
                }
            }
        }
    }

    if (defaultStates.empty()) {
        LOG_WARN("HistoryManager: No default states available for {}", historyStateInfo.historyStateId);
    }

    return defaultStates;
}

}  // namespace RSM