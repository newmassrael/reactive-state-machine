#include "runtime/HistoryValidator.h"
#include "common/Logger.h"
#include "model/IStateNode.h"

namespace RSM {

HistoryValidator::HistoryValidator(std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider)
    : stateProvider_(std::move(stateProvider)) {
    Logger::debug("HistoryValidator: Initialized history validator");
}

bool HistoryValidator::validateRegistration(const std::string &historyStateId, const std::string &parentStateId,
                                            HistoryType type) const {
    Logger::debug("HistoryValidator: Validating registration - history: " + historyStateId +
                  ", parent: " + parentStateId + ", type: " + std::to_string(static_cast<int>(type)));

    // Check for empty IDs
    if (historyStateId.empty() || parentStateId.empty()) {
        Logger::error("HistoryValidator: History state ID and parent state ID cannot be empty");
        return false;
    }

    // Check for invalid history type
    if (type == HistoryType::NONE) {
        Logger::error("HistoryValidator: History type cannot be NONE for registration");
        return false;
    }

    // Check if history state is already registered
    if (registeredHistoryStates_.find(historyStateId) != registeredHistoryStates_.end()) {
        Logger::warn("HistoryValidator: History state already registered: " + historyStateId);
        return false;
    }

    // Check if parent state exists and is a compound state
    if (!isValidCompoundState(parentStateId)) {
        Logger::error("HistoryValidator: Parent state is not a valid compound state: " + parentStateId);
        return false;
    }

    // Check if parent already has a history state of this type
    std::string parentTypeKey = generateParentTypeKey(parentStateId, type);
    if (registeredParentTypes_.find(parentTypeKey) != registeredParentTypes_.end()) {
        Logger::warn("HistoryValidator: Parent state " + parentStateId +
                     " already has a history state of the specified type");
        return false;
    }

    Logger::info("HistoryValidator: Registration validation passed for " + historyStateId);
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
            Logger::error("HistoryValidator: Default state does not exist: " + defaultStateId);
            return false;
        }

        // Check if default state is a child of parent state
        auto parentState = stateProvider_(parentStateId);
        if (parentState) {
            bool isChild = false;
            for (const auto &child : parentState->getChildren()) {
                if (child && child->getId() == defaultStateId) {
                    isChild = true;
                    break;
                }
            }
            if (!isChild) {
                Logger::error("HistoryValidator: Default state must be a child of parent state: " + defaultStateId +
                              " not child of " + parentStateId);
                return false;
            }
        }
    }

    Logger::info("HistoryValidator: Registration with default validation passed for " + historyStateId);
    return true;
}

bool HistoryValidator::validateRecording(const std::string &parentStateId,
                                         const std::vector<std::string> &activeStateIds) const {
    Logger::debug("HistoryValidator: Validating recording - parent: " + parentStateId +
                  ", active states: " + std::to_string(activeStateIds.size()));

    // Check for empty parent ID
    if (parentStateId.empty()) {
        Logger::error("HistoryValidator: Parent state ID cannot be empty for recording");
        return false;
    }

    // Check if parent state exists
    if (!stateProvider_) {
        Logger::error("HistoryValidator: No state provider available");
        return false;
    }

    auto parentState = stateProvider_(parentStateId);
    if (!parentState) {
        Logger::error("HistoryValidator: Parent state not found: " + parentStateId);
        return false;
    }

    // Active states can be empty (valid scenario)
    Logger::info("HistoryValidator: Recording validation passed for " + parentStateId);
    return true;
}

bool HistoryValidator::validateRestoration(const std::string &historyStateId) const {
    Logger::debug("HistoryValidator: Validating restoration - history: " + historyStateId);

    // Check for empty ID
    if (historyStateId.empty()) {
        Logger::error("HistoryValidator: History state ID cannot be empty for restoration");
        return false;
    }

    // Check if history state is registered
    if (registeredHistoryStates_.find(historyStateId) == registeredHistoryStates_.end()) {
        Logger::error("HistoryValidator: History state not registered: " + historyStateId);
        return false;
    }

    Logger::info("HistoryValidator: Restoration validation passed for " + historyStateId);
    return true;
}

void HistoryValidator::registerHistoryStateId(const std::string &historyStateId) {
    registeredHistoryStates_.insert(historyStateId);
    Logger::debug("HistoryValidator: Registered history state ID: " + historyStateId);
}

void HistoryValidator::registerParentType(const std::string &parentStateId, HistoryType type) {
    std::string key = generateParentTypeKey(parentStateId, type);
    registeredParentTypes_.insert(key);
    Logger::debug("HistoryValidator: Registered parent-type combination: " + key);
}

bool HistoryValidator::isValidCompoundState(const std::string &stateId) const {
    if (!stateProvider_) {
        Logger::error("HistoryValidator: No state provider available");
        return false;
    }

    auto state = stateProvider_(stateId);
    if (!state) {
        Logger::warn("HistoryValidator: State not found: " + stateId);
        return false;
    }

    // A compound state should have children or be marked as COMPOUND/PARALLEL
    Type stateType = state->getType();
    bool isCompound = (stateType == Type::COMPOUND || stateType == Type::PARALLEL) || !state->getChildren().empty();

    Logger::debug("HistoryValidator: State " + stateId + " is " + (isCompound ? "" : "not ") + "a compound state");

    return isCompound;
}

std::string HistoryValidator::generateParentTypeKey(const std::string &parentStateId, HistoryType type) const {
    std::string typeStr = (type == HistoryType::SHALLOW) ? "SHALLOW" : "DEEP";
    return parentStateId + "_" + typeStr;
}

}  // namespace RSM