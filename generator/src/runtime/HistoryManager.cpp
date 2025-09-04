#include "runtime/AdvancedHistoryManager.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <json/json.h>
#include <regex>
#include <sstream>

namespace SCXML {
namespace Runtime {

AdvancedHistoryManager::AdvancedHistoryManager(SCXML::Runtime::RuntimeContext &context) : context_(context) {}

SCXML::Common::Result<void>
AdvancedHistoryManager::recordAdvancedHistory(const std::string &historyId, const std::set<std::string> &activeStates,
                                              HistoryType type,
                                              const std::unordered_map<std::string, std::string> &conditions) {
    try {
        // Extract parent state from history ID
        std::string parentState = historyId;
        if (historyId.find("_history") != std::string::npos) {
            parentState = historyId.substr(0, historyId.find("_history"));
        }

        // Filter states based on history type
        auto filteredStates = filterStatesForHistory(activeStates, parentState, type);

        // Create history record
        HistoryRecord record;
        record.states = filteredStates;
        record.timestamp = std::chrono::steady_clock::now();
        record.parentState = parentState;
        record.type = type;
        record.entryCount = historyRecords_.count(historyId) ? historyRecords_[historyId].entryCount + 1 : 1;
        record.isValid = true;
        record.metadata = conditions;

        // Store record
        historyRecords_[historyId] = record;

        // Update analytics
        if (analyticsEnabled_) {
            updateAnalytics(historyId, "record");
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to record advanced history: " + std::string(e.what()));
    }
}

SCXML::Common::Result<std::set<std::string>>
AdvancedHistoryManager::restoreAdvancedHistory(const std::string &historyId, bool validateConditions) {
    try {
        // Check if history exists
        if (historyRecords_.find(historyId) == historyRecords_.end()) {
            return SCXML::Common::Result<std::set<std::string>>::success(std::set<std::string>());
        }

        auto &record = historyRecords_[historyId];

        // Check if expired
        if (isHistoryExpired(historyId)) {
            record.isValid = false;
            if (analyticsEnabled_) {
                updateAnalytics(historyId, "expire");
            }
            return SCXML::Common::Result<std::set<std::string>>::success(std::set<std::string>());
        }

        // Validate conditions if requested
        if (validateConditions && !validateRestorationConditions(record)) {
            return SCXML::Common::Result<std::set<std::string>>::success(std::set<std::string>());
        }

        // Update analytics
        if (analyticsEnabled_) {
            updateAnalytics(historyId, "restore");
        }

        return SCXML::Common::Result<std::set<std::string>>::success(record.states);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::set<std::string>>::failure("Failed to restore advanced history: " +
                                                                     std::string(e.what()));
    }
}

void AdvancedHistoryManager::setHistoryExpiration(const std::string &historyId,
                                                  std::chrono::milliseconds expirationMs) {
    if (expirationMs.count() > 0) {
        expirationTimes_[historyId] = expirationMs;
    } else {
        expirationTimes_.erase(historyId);
    }
}

size_t AdvancedHistoryManager::clearExpiredHistory() {
    size_t clearedCount = 0;

    auto it = historyRecords_.begin();
    while (it != historyRecords_.end()) {
        if (isHistoryExpired(it->first)) {
            if (analyticsEnabled_) {
                updateAnalytics(it->first, "expire");
            }
            it = historyRecords_.erase(it);
            clearedCount++;
        } else {
            ++it;
        }
    }

    return clearedCount;
}

SCXML::Common::Result<bool> AdvancedHistoryManager::validateHistory(const std::string &historyId) {
    try {
        if (historyRecords_.find(historyId) == historyRecords_.end()) {
            return SCXML::Common::Result<bool>::success(false);
        }

        auto &record = historyRecords_[historyId];

        // Check basic validity
        if (!record.isValid) {
            return SCXML::Common::Result<bool>::success(false);
        }

        // Check expiration
        if (isHistoryExpired(historyId)) {
            record.isValid = false;
            return SCXML::Common::Result<bool>::success(false);
        }

        // Validate state existence (would need state machine model access)
        // For now, just check if states are non-empty
        if (record.states.empty()) {
            return SCXML::Common::Result<bool>::success(false);
        }

        return SCXML::Common::Result<bool>::success(true);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<bool>::failure("Failed to validate history: " + std::string(e.what()));
    }
}

AdvancedHistoryManager::HistoryMetrics AdvancedHistoryManager::getMetrics() const {
    HistoryMetrics metrics;

    if (historyRecords_.empty()) {
        return metrics;
    }

    metrics.totalRecords = historyRecords_.size();

    size_t totalStates = 0;
    auto oldestTime = std::chrono::steady_clock::time_point::max();
    auto newestTime = std::chrono::steady_clock::time_point::min();

    for (const auto &[historyId, record] : historyRecords_) {
        totalStates += record.states.size();

        if (record.type == HistoryType::DEEP) {
            metrics.deepHistoryRecords++;
        } else {
            metrics.shallowHistoryRecords++;
        }

        if (isHistoryExpired(historyId)) {
            metrics.expiredRecords++;
        }

        if (record.timestamp < oldestTime) {
            oldestTime = record.timestamp;
        }
        if (record.timestamp > newestTime) {
            newestTime = record.timestamp;
        }
    }

    metrics.averageStatesPerRecord = static_cast<double>(totalStates) / metrics.totalRecords;

    auto now = std::chrono::steady_clock::now();
    metrics.oldestRecord = std::chrono::duration_cast<std::chrono::milliseconds>(now - oldestTime);
    metrics.newestRecord = std::chrono::duration_cast<std::chrono::milliseconds>(now - newestTime);

    return metrics;
}

std::string AdvancedHistoryManager::exportHistoryState(bool includeExpired) const {
    Json::Value root;
    Json::Value records(Json::arrayValue);

    for (const auto &[historyId, record] : historyRecords_) {
        if (!includeExpired && isHistoryExpired(historyId)) {
            continue;
        }

        Json::Value recordJson;
        recordJson["historyId"] = historyId;
        recordJson["parentState"] = record.parentState;
        recordJson["type"] = (record.type == HistoryType::DEEP) ? "deep" : "shallow";
        recordJson["isValid"] = record.isValid;
        recordJson["entryCount"] = static_cast<int>(record.entryCount);
        recordJson["isExpired"] = isHistoryExpired(historyId);

        Json::Value statesArray(Json::arrayValue);
        for (const auto &state : record.states) {
            statesArray.append(state);
        }
        recordJson["states"] = statesArray;

        Json::Value metadataObj;
        for (const auto &[key, value] : record.metadata) {
            metadataObj[key] = value;
        }
        recordJson["metadata"] = metadataObj;

        records.append(recordJson);
    }

    root["records"] = records;
    root["totalCount"] = static_cast<int>(historyRecords_.size());

    auto metrics = getMetrics();
    Json::Value metricsJson;
    metricsJson["totalRecords"] = static_cast<int>(metrics.totalRecords);
    metricsJson["expiredRecords"] = static_cast<int>(metrics.expiredRecords);
    metricsJson["deepHistoryRecords"] = static_cast<int>(metrics.deepHistoryRecords);
    metricsJson["shallowHistoryRecords"] = static_cast<int>(metrics.shallowHistoryRecords);
    metricsJson["averageStatesPerRecord"] = metrics.averageStatesPerRecord;
    root["metrics"] = metricsJson;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    return Json::writeString(builder, root);
}

size_t AdvancedHistoryManager::clearHistoryForState(const std::string &parentStateId) {
    size_t clearedCount = 0;

    auto it = historyRecords_.begin();
    while (it != historyRecords_.end()) {
        if (it->second.parentState == parentStateId) {
            if (analyticsEnabled_) {
                updateAnalytics(it->first, "clear");
            }
            it = historyRecords_.erase(it);
            clearedCount++;
        } else {
            ++it;
        }
    }

    return clearedCount;
}

bool AdvancedHistoryManager::hasValidHistory(const std::string &historyId) const {
    auto it = historyRecords_.find(historyId);
    if (it == historyRecords_.end()) {
        return false;
    }

    return it->second.isValid && !isHistoryExpired(historyId);
}

void AdvancedHistoryManager::setHistoryMetadata(const std::string &historyId, const std::string &key,
                                                const std::string &value) {
    if (historyRecords_.find(historyId) != historyRecords_.end()) {
        historyRecords_[historyId].metadata[key] = value;
    }
}

// Private implementation methods

std::set<std::string> AdvancedHistoryManager::filterStatesForHistory(const std::set<std::string> &activeStates,
                                                                     const std::string &parentState,
                                                                     HistoryType type) const {
    std::set<std::string> filteredStates;

    for (const auto &state : activeStates) {
        if (!isDescendantOf(state, parentState)) {
            continue;
        }

        if (type == HistoryType::SHALLOW) {
            // Only include direct children
            if (getStateDepth(state, parentState) == 1) {
                filteredStates.insert(state);
            }
        } else {
            // Include all descendants
            filteredStates.insert(state);
        }
    }

    return filteredStates;
}

bool AdvancedHistoryManager::isHistoryExpired(const std::string &historyId) const {
    auto expirationIt = expirationTimes_.find(historyId);
    if (expirationIt == expirationTimes_.end()) {
        return false;  // No expiration set
    }

    auto historyIt = historyRecords_.find(historyId);
    if (historyIt == historyRecords_.end()) {
        return true;  // Record doesn't exist
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - historyIt->second.timestamp);

    return elapsed >= expirationIt->second;
}

bool AdvancedHistoryManager::validateRestorationConditions(const HistoryRecord &record) const {
    // Check metadata conditions
    for (const auto &[key, expectedValue] : record.metadata) {
        // This would need access to current context/data model to validate
        // For now, assume all conditions are met
        (void)key;
        (void)expectedValue;
    }

    return true;
}

void AdvancedHistoryManager::updateAnalytics(const std::string &historyId, const std::string &operation) {
    if (!analyticsEnabled_) {
        return;
    }

    // Could log to analytics system here
    // For now, just track in metadata
    if (historyRecords_.find(historyId) != historyRecords_.end()) {
        auto &record = historyRecords_[historyId];
        record.metadata["last_operation"] = operation;
        record.metadata["last_operation_time"] = std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }
}

int AdvancedHistoryManager::getStateDepth(const std::string &stateId, const std::string &parentState) const {
    // Simple implementation: count dots in state path relative to parent
    if (!isDescendantOf(stateId, parentState)) {
        return -1;
    }

    std::string relativePath = stateId.substr(parentState.length());
    if (relativePath.front() == '.') {
        relativePath = relativePath.substr(1);
    }

    return std::count(relativePath.begin(), relativePath.end(), '.') + 1;
}

bool AdvancedHistoryManager::isDescendantOf(const std::string &stateId, const std::string &parentState) const {
    if (stateId.length() <= parentState.length()) {
        return false;
    }

    return stateId.substr(0, parentState.length()) == parentState &&
           (stateId[parentState.length()] == '.' || parentState.empty());
}

}  // namespace Runtime
}  // namespace SCXML