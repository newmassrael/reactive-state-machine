#include "runtime/HistoryValidator.h"
#include "common/Logger.h"
#include "model/IStateNode.h"

namespace RSM {

HistoryValidator::HistoryValidator(std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider)
    : stateProvider_(std::move(stateProvider)) {
    LOG_DEBUG("HistoryValidator: Initialized history validator");
}

bool HistoryValidator::validateRegistration(const std::string &historyStateId, const std::string &parentStateId,
                                            HistoryType type) const {
    LOG_DEBUG("Validating registration - history: {}, parent: {}, type: {}", historyStateId, parentStateId,
              static_cast<int>(type));

    // Check for empty IDs
    if (historyStateId.empty() || parentStateId.empty()) {
        LOG_ERROR("HistoryValidator: History state ID and parent state ID cannot be empty");
        return false;
    }

    // Check for invalid history type
    if (type == HistoryType::NONE) {
        LOG_ERROR("HistoryValidator: History type cannot be NONE for registration");
        return false;
    }

    // Check if history state is already registered
    if (registeredHistoryStates_.find(historyStateId) != registeredHistoryStates_.end()) {
        LOG_WARN("History state already registered: {}", historyStateId);
        return false;
    }

    // Check if parent state exists and is a compound state
    if (!isValidCompoundState(parentStateId)) {
        LOG_ERROR("Parent state is not a valid compound state: {}", parentStateId);
        return false;
    }

    // Check if parent already has a history state of this type
    std::string parentTypeKey = generateParentTypeKey(parentStateId, type);
    if (registeredParentTypes_.find(parentTypeKey) != registeredParentTypes_.end()) {
        LOG_WARN("Parent state {} already has a history state of the specified type", parentStateId);
        return false;
    }

    LOG_INFO("Registration validation passed for {}", historyStateId);
    return true;
}

bool HistoryValidator::validateRegistrationWithDefault(const std::string &historyStateId,
                                                       const std::string &parentStateId, HistoryType type,
                                                       const std::string &defaultStateId) const {
    // First perform standard registration validation
    if (!validateRegistration(historyStateId, parentStateId, type)) {
        return false;
    }

    // W3C SCXML Section 3.6: Validate default state if provided
    if (!defaultStateId.empty()) {
        auto defaultState = stateProvider_(defaultStateId);
        if (!defaultState) {
            LOG_ERROR("Default state does not exist: {}", defaultStateId);
            return false;
        }

        // W3C SCXML 3.6: Check if default state is valid for history type
        auto parentState = stateProvider_(parentStateId);
        if (parentState) {
            bool isValid = false;

            if (type == HistoryType::SHALLOW) {
                // Shallow history: default must be direct child
                for (const auto &child : parentState->getChildren()) {
                    if (child && child->getId() == defaultStateId) {
                        isValid = true;
                        break;
                    }
                }
                if (!isValid) {
                    LOG_ERROR("Shallow history default must be direct child: {} not child of {}", defaultStateId,
                              parentStateId);
                    return false;
                }
            } else if (type == HistoryType::DEEP) {
                // Deep history: default can be any descendant
                isValid = isDescendantOf(defaultStateId, parentStateId);
                if (!isValid) {
                    LOG_ERROR("Deep history default must be descendant: {} not descendant of {}", defaultStateId,
                              parentStateId);
                    return false;
                }
            }
        }
    }

    LOG_INFO("Registration with default validation passed for {}", historyStateId);
    return true;
}

bool HistoryValidator::validateRecording(const std::string &parentStateId,
                                         const std::vector<std::string> &activeStateIds) const {
    LOG_DEBUG("Validating recording - parent: {}, active states: {}", parentStateId, activeStateIds.size());

    // Check for empty parent ID
    if (parentStateId.empty()) {
        LOG_ERROR("HistoryValidator: Parent state ID cannot be empty for recording");
        return false;
    }

    // Check if parent state exists
    if (!stateProvider_) {
        LOG_ERROR("HistoryValidator: No state provider available");
        return false;
    }

    auto parentState = stateProvider_(parentStateId);
    if (!parentState) {
        LOG_ERROR("Parent state not found: {}", parentStateId);
        return false;
    }

    // Active states can be empty (valid scenario)
    LOG_INFO("Recording validation passed for {}", parentStateId);
    return true;
}

bool HistoryValidator::validateRestoration(const std::string &historyStateId) const {
    LOG_DEBUG("Validating restoration - history: {}", historyStateId);

    // Check for empty ID
    if (historyStateId.empty()) {
        LOG_ERROR("HistoryValidator: History state ID cannot be empty for restoration");
        return false;
    }

    // Check if history state is registered
    if (registeredHistoryStates_.find(historyStateId) == registeredHistoryStates_.end()) {
        LOG_ERROR("History state not registered: {}", historyStateId);
        return false;
    }

    LOG_INFO("Restoration validation passed for {}", historyStateId);
    return true;
}

void HistoryValidator::registerHistoryStateId(const std::string &historyStateId) {
    registeredHistoryStates_.insert(historyStateId);
    LOG_DEBUG("Registered history state ID: {}", historyStateId);
}

void HistoryValidator::registerParentType(const std::string &parentStateId, HistoryType type) {
    std::string key = generateParentTypeKey(parentStateId, type);
    registeredParentTypes_.insert(key);
    LOG_DEBUG("Registered parent-type combination: {}", key);
}

bool HistoryValidator::isValidCompoundState(const std::string &stateId) const {
    if (!stateProvider_) {
        LOG_ERROR("HistoryValidator: No state provider available");
        return false;
    }

    auto state = stateProvider_(stateId);
    if (!state) {
        LOG_WARN("State not found: {}", stateId);
        return false;
    }

    // A compound state should have children or be marked as COMPOUND/PARALLEL
    Type stateType = state->getType();
    bool isCompound = (stateType == Type::COMPOUND || stateType == Type::PARALLEL) || !state->getChildren().empty();

    LOG_DEBUG("State {} is {}a compound state", stateId, (isCompound ? "" : "not "));

    return isCompound;
}

bool HistoryValidator::isDescendantOf(const std::string &stateId, const std::string &ancestorId) const {
    // Performance optimization: Check cache first
    std::string cacheKey = stateId + ":" + ancestorId;
    auto cacheIt = descendantCache_.find(cacheKey);
    if (cacheIt != descendantCache_.end()) {
        return cacheIt->second;
    }

    if (!stateProvider_) {
        LOG_ERROR("HistoryValidator: No state provider available");
        descendantCache_[cacheKey] = false;
        return false;
    }

    auto state = stateProvider_(stateId);
    if (!state) {
        LOG_WARN("State not found: {}", stateId);
        descendantCache_[cacheKey] = false;
        return false;
    }

    // Traverse up the parent chain looking for the ancestor
    auto currentParent = state->getParent();
    while (currentParent) {
        if (currentParent->getId() == ancestorId) {
            descendantCache_[cacheKey] = true;
            return true;  // Found ancestor
        }
        currentParent = currentParent->getParent();
    }

    descendantCache_[cacheKey] = false;
    return false;  // Ancestor not found in parent chain
}

std::string HistoryValidator::generateParentTypeKey(const std::string &parentStateId, HistoryType type) const {
    std::string typeStr = (type == HistoryType::SHALLOW) ? "SHALLOW" : "DEEP";
    return parentStateId + "_" + typeStr;
}

}  // namespace RSM