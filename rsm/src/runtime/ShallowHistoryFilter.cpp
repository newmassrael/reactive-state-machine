#include "runtime/ShallowHistoryFilter.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include <algorithm>

namespace RSM {

ShallowHistoryFilter::ShallowHistoryFilter(
    std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider)
    : stateProvider_(std::move(stateProvider)) {
    Logger::debug("ShallowHistoryFilter: Initialized shallow history filter");
}

std::vector<std::string> ShallowHistoryFilter::filterStates(const std::vector<std::string> &activeStateIds,
                                                            const std::string &parentStateId) const {
    Logger::debug("ShallowHistoryFilter: Filtering states for shallow history - parent: " + parentStateId +
                  ", active states: " + std::to_string(activeStateIds.size()));

    if (parentStateId.empty()) {
        Logger::warn("ShallowHistoryFilter: Empty parent state ID provided");
        return {};
    }

    std::vector<std::string> filteredStates;

    // For shallow history, we only record immediate child states of the parent
    for (const auto &stateId : activeStateIds) {
        if (isImmediateChild(stateId, parentStateId)) {
            filteredStates.push_back(stateId);
            Logger::debug("ShallowHistoryFilter: Recording immediate child state: " + stateId);
        }
    }

    Logger::info("ShallowHistoryFilter: Filtered " + std::to_string(activeStateIds.size()) + " states to " +
                 std::to_string(filteredStates.size()) + " immediate children");

    return filteredStates;
}

bool ShallowHistoryFilter::isImmediateChild(const std::string &stateId, const std::string &parentStateId) const {
    if (!stateProvider_) {
        Logger::error("ShallowHistoryFilter: No state provider available");
        return false;
    }

    auto state = stateProvider_(stateId);
    if (!state) {
        Logger::warn("ShallowHistoryFilter: State not found: " + stateId);
        return false;
    }

    auto parent = state->getParent();
    if (!parent) {
        Logger::debug("ShallowHistoryFilter: State " + stateId + " has no parent");
        return false;
    }

    bool isImmediate = (parent->getId() == parentStateId);
    Logger::debug("ShallowHistoryFilter: State " + stateId + " is " + (isImmediate ? "" : "not ") +
                  "immediate child of " + parentStateId);

    return isImmediate;
}

}  // namespace RSM