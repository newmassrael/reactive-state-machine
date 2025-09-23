#pragma once

#include "IHistoryStateAutoRegistrar.h"
#include "types.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

// Forward declarations
class SCXMLModel;
class IHistoryManager;
class IStateNode;

/**
 * @brief SOLID implementation of automatic history state registration
 *
 * This implementation follows SOLID principles:
 * - Single Responsibility: Only handles auto-registration logic
 * - Open/Closed: Extensible through strategy patterns
 * - Liskov Substitution: Fully substitutable for interface
 * - Interface Segregation: Uses focused interfaces
 * - Dependency Inversion: Depends on abstractions, not concretions
 *
 * Long-term Architecture Benefits:
 * - Testable in isolation
 * - Configurable registration strategies
 * - Extensible for future SCXML features
 * - Clear separation of concerns
 */
class HistoryStateAutoRegistrar : public IHistoryStateAutoRegistrar {
public:
    /**
     * @brief Constructor with dependency injection
     * @param stateProvider Function to retrieve state nodes (dependency injection)
     */
    explicit HistoryStateAutoRegistrar(std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider);

    /**
     * @brief Destructor
     */
    ~HistoryStateAutoRegistrar() override = default;

    // IHistoryStateAutoRegistrar interface
    bool autoRegisterHistoryStates(const std::shared_ptr<SCXMLModel> &model, IHistoryManager *historyManager) override;

    size_t getRegisteredHistoryStateCount() const override;
    bool isAutoRegistrationEnabled() const override;
    void setAutoRegistrationEnabled(bool enabled) override;

private:
    // State provider for dependency injection
    std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider_;

    // Configuration
    bool autoRegistrationEnabled_ = true;

    // Statistics
    mutable size_t registeredHistoryStateCount_ = 0;

    /**
     * @brief Register a single history state
     * @param historyStateId ID of the history state
     * @param parentStateId ID of the parent compound state
     * @param historyType Type of history (shallow/deep)
     * @param defaultStateId Default state for the history
     * @param historyManager History manager to register with
     * @return true if registration succeeded
     */
    bool registerSingleHistoryState(const std::string &historyStateId, const std::string &parentStateId,
                                    HistoryType historyType, const std::string &defaultStateId,
                                    IHistoryManager *historyManager);

    /**
     * @brief Extract history states from SCXML model
     * @param model SCXML model to extract from
     * @return Vector of history state information
     */
    struct HistoryStateInfo {
        std::string historyStateId;
        std::string parentStateId;
        HistoryType historyType;
        std::string defaultStateId;
    };

    std::vector<HistoryStateInfo> extractHistoryStatesFromModel(const std::shared_ptr<SCXMLModel> &model);

    /**
     * @brief Find parent state ID for a given history state
     * @param historyStateId ID of the history state
     * @param model SCXML model to search in
     * @return Parent state ID, empty if not found
     */
    std::string findParentStateId(const std::string &historyStateId, const std::shared_ptr<SCXMLModel> &model);

    /**
     * @brief Extract default state from history transitions
     * @param historyState History state node
     * @return Default state ID, empty if none specified
     */
    std::string extractDefaultStateId(const std::shared_ptr<IStateNode> &historyState);
};

}  // namespace RSM