#pragma once

#include "interfaces/IRuntimeContext.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace SCXML {

namespace Model {
class DocumentModel;
}
namespace Runtime {
// Forward declarations for implementations
class StateManager;
class EventManager;
class DataContextManager;

/**
 * @brief Main runtime execution context for SCXML state machines
 *
 * This class demonstrates the SOLID principles in action:
 *
 * S - Single Responsibility: Coordinates specialized managers
 * O - Open/Closed: New functionality via new manager interfaces
 * L - Liskov Substitution: All managers follow their contracts
 * I - Interface Segregation: Each manager has focused interface
 * D - Dependency Inversion: Depends on abstractions, not concretions
 *
 * Architecture Benefits:
 * - Each manager can be unit tested independently
 * - Managers can be mocked for testing
 * - New functionality doesn't require changing existing code
 * - Clear separation of concerns
 * - Easy to reason about and maintain
 */
class Context : public IRuntimeContext {
public:
    /**
     * @brief Construct a new runtime context
     *
     * Uses dependency injection for all managers to enable
     * testing and flexible configuration.
     *
     * @param stateManager State management implementation
     * @param eventManager Event processing implementation
     * @param dataManager Data context implementation
     * @param invokeManager Invocation session implementation
     */
    Context(std::unique_ptr<IStateManager> stateManager, std::unique_ptr<IEventManager> eventManager,
            std::unique_ptr<IDataContextManager> dataManager, std::unique_ptr<IInvokeSessionManager> invokeManager);

    /**
     * @brief Destructor - ensures proper cleanup
     */
    virtual ~Context();

    // ====== IRuntimeContext Implementation ======

    /**
     * @brief Initialize the runtime context with SCXML model
     * @param model The parsed SCXML model
     * @return true if initialization successful
     */
    bool initialize(std::shared_ptr<::SCXML::Model::DocumentModel> model) override;

    /**
     * @brief Get current active state IDs
     * @return Set of active state IDs
     */
    std::set<std::string> getActiveStates() const override;

    /**
     * @brief Check if a state is currently active
     * @param stateId State ID to check
     * @return true if state is active
     */
    bool isStateActive(const std::string &stateId) const override;

    /**
     * @brief Get data model value by location expression
     * @param location Location expression (e.g., "user.name")
     * @return Data value as string, empty if not found
     */
    std::string getDataValue(const std::string &location) const override;

    /**
     * @brief Set data model value by location expression
     * @param location Location expression
     * @param value New value to set
     * @return true if assignment successful
     */
    bool setDataValue(const std::string &location, const std::string &value) override;

    /**
     * @brief Send an internal event
     * @param eventName Event name
     * @param data Optional event data
     * @return true if event was queued successfully
     */
    bool sendInternalEvent(const std::string &eventName, const std::string &data = "") override;

    /**
     * @brief Get runtime session ID
     * @return Session ID string
     */
    std::string getSessionId() const override;

    // ====== Extended Context API ======

    /**
     * @brief Get the state manager instance
     * @return Reference to state manager
     */
    IStateManager &getStateManager() const;

    /**
     * @brief Get the event manager instance
     * @return Reference to event manager
     */
    IEventManager &getEventManager() const;

    /**
     * @brief Get the data context manager instance
     * @return Reference to data manager
     */
    IDataContextManager &getDataManager() const;

    /**
     * @brief Get the invoke session manager instance
     * @return Reference to invoke manager
     */
    IInvokeSessionManager &getInvokeManager() const;

    /**
     * @brief Set runtime session ID
     * @param sessionId New session ID
     */
    void setSessionId(const std::string &sessionId);

    /**
     * @brief Get runtime statistics
     * @return JSON-formatted statistics string
     */
    std::string getStatistics() const;

    /**
     * @brief Enable or disable debug mode
     * @param enabled True to enable debugging
     */
    void setDebugMode(bool enabled);

    /**
     * @brief Check if debug mode is enabled
     * @return true if debug mode is on
     */
    bool isDebugMode() const;

private:
    // Manager instances (using SOLID principles)
    std::unique_ptr<IStateManager> stateManager_;
    std::unique_ptr<IEventManager> eventManager_;
    std::unique_ptr<IDataContextManager> dataManager_;
    std::unique_ptr<IInvokeSessionManager> invokeManager_;

    // Runtime configuration
    std::string sessionId_;
    bool debugMode_;
    bool initialized_;

    // Statistics tracking
    mutable std::mutex statsMutex_;
    uint64_t eventCount_;
    uint64_t stateTransitions_;
    std::chrono::steady_clock::time_point creationTime_;

    // Helper methods
    void validateManagers();
    void initializeManagers(std::shared_ptr<::SCXML::Model::DocumentModel> model);
    void logDebug(const std::string &message) const;
};

}  // namespace Runtime
}  // namespace SCXML