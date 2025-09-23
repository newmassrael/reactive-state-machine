#pragma once

#include "runtime/IHistoryManager.h"
#include <functional>
#include <memory>
#include <unordered_set>

namespace RSM {

// Forward declarations
class IStateNode;

/**
 * @brief Default history validator implementation (Single Responsibility)
 *
 * Validates history operations according to SCXML W3C specification requirements:
 * - History states must have valid parent compound states
 * - Each parent can have at most one history state of each type
 * - State IDs must be valid and non-empty
 */
class HistoryValidator : public IHistoryValidator {
public:
    /**
     * @brief Constructor with state hierarchy access
     * @param stateProvider Function to get state by ID (Dependency Injection)
     */
    explicit HistoryValidator(std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider);

    /**
     * @brief Validate history state registration
     * @param historyStateId History state ID
     * @param parentStateId Parent state ID
     * @param type History type
     * @return true if registration is valid
     */
    bool validateRegistration(const std::string &historyStateId, const std::string &parentStateId,
                              HistoryType type) const override;

    /**
     * @brief Validate history state registration with default state
     * @param historyStateId History state ID
     * @param parentStateId Parent state ID
     * @param type History type
     * @param defaultStateId Default state ID
     * @return true if registration is valid
     */
    bool validateRegistrationWithDefault(const std::string &historyStateId, const std::string &parentStateId,
                                         HistoryType type, const std::string &defaultStateId) const;

    /**
     * @brief Validate history recording operation
     * @param parentStateId Parent state ID
     * @param activeStateIds Active states
     * @return true if recording is valid
     */
    bool validateRecording(const std::string &parentStateId,
                           const std::vector<std::string> &activeStateIds) const override;

    /**
     * @brief Validate history restoration operation
     * @param historyStateId History state ID
     * @return true if restoration is valid
     */
    bool validateRestoration(const std::string &historyStateId) const override;

    /**
     * @brief Register a history state ID for tracking
     * @param historyStateId History state ID to register
     */
    void registerHistoryStateId(const std::string &historyStateId);

    /**
     * @brief Register a parent-type combination to prevent duplicates
     * @param parentStateId Parent state ID
     * @param type History type
     */
    void registerParentType(const std::string &parentStateId, HistoryType type);

private:
    std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider_;
    mutable std::unordered_set<std::string> registeredHistoryStates_;
    mutable std::unordered_set<std::string> registeredParentTypes_;

    /**
     * @brief Check if a state exists and is a compound state
     * @param stateId State ID to check
     * @return true if valid compound state
     */
    bool isValidCompoundState(const std::string &stateId) const;

    /**
     * @brief Generate key for parent-type combination
     * @param parentStateId Parent state ID
     * @param type History type
     * @return Unique key string
     */
    std::string generateParentTypeKey(const std::string &parentStateId, HistoryType type) const;
};

}  // namespace RSM