#pragma once

#include "interfaces/IDataContextManager.h"
#include "interfaces/IEventManager.h"
#include "interfaces/IInvokeSessionManager.h"
#include "interfaces/IRuntimeContext.h"
#include "interfaces/IStateManager.h"
#include "runtime/EventScheduler.h"
#include <memory>
#include <mutex>
#include <string>

namespace SCXML {

namespace Model {
class IStateNode;
class DocumentModel;
}  // namespace Model
// Forward declarations
class Logger;
class DataModelEngine;

namespace Runtime {

/**
 * @brief Runtime context implementation using composition pattern
 *
 * This class implements the SOLID principles:
 * - Single Responsibility: Coordinates specialized managers
 * - Open/Closed: New functionality via new manager interfaces
 * - Liskov Substitution: All managers follow their contracts
 * - Interface Segregation: Each manager has focused interface
 * - Dependency Inversion: Depends on abstractions, not concretions
 */
class RuntimeContext : public IRuntimeContext {
public:
    /**
     * @brief Construct a new Runtime Context
     *
     * Uses dependency injection for all managers to enable
     * testing and flexible configuration.
     */
    RuntimeContext(std::unique_ptr<IStateManager> stateManager = nullptr,
                   std::unique_ptr<IEventManager> eventManager = nullptr,
                   std::unique_ptr<IDataContextManager> dataContextManager = nullptr,
                   std::unique_ptr<IInvokeSessionManager> invokeSessionManager = nullptr);

    virtual ~RuntimeContext();

    // ========== IRuntimeContext Implementation ==========

    IStateManager &getStateManager() override;
    const IStateManager &getStateManager() const override;

    IEventManager &getEventManager() override;
    const IEventManager &getEventManager() const override;

    IDataContextManager &getDataContextManager() override;
    const IDataContextManager &getDataContextManager() const override;

    IInvokeSessionManager &getInvokeSessionManager() override;
    const IInvokeSessionManager &getInvokeSessionManager() const override;

    void log(const std::string &level, const std::string &message) override;

    bool initialize() override;
    void shutdown() override;
    bool isInitialized() const override;
    std::string getStatusSummary() const override;

    // ========== Backward Compatibility Methods ==========
    // These methods delegate to appropriate managers for existing code compatibility

    // State management compatibility
    void setCurrentState(const std::string &stateId);
    std::string getCurrentState() const;
    std::vector<std::string> getActiveStates() const;
    bool isInState(const std::string &stateId) const;

    // Event management compatibility
    void raiseEvent(Events::EventPtr event);
    void raiseEvent(const std::string &eventName, const std::string &data = "");
    void sendEvent(Events::EventPtr event, const std::string &target = "", uint64_t delayMs = 0);
    void sendEvent(const std::string &eventName, const std::string &target = "", uint64_t delayMs = 0,
                   const std::string &data = "");

    // Data management compatibility
    void setDataValue(const std::string &id, const std::string &value);
    std::string getDataValue(const std::string &id) const;
    bool hasDataValue(const std::string &id) const;
    std::string evaluateExpression(const std::string &expression) const;
    bool evaluateCondition(const std::string &condition) const;

    // Model access compatibility
    void setModel(std::shared_ptr<Model::DocumentModel> model);
    std::shared_ptr<Model::DocumentModel> getModel() const;

    // Session management compatibility
    void setSessionName(const std::string &name);
    std::string getSessionName() const;
    void setSessionId(const std::string &sessionId);
    std::string getSessionId() const;

    // Additional compatibility methods (from original RuntimeContext)
    SCXML::DataModelEngine *getDataModelEngine() const;
    Logger *getLogger() const;
    Events::EventPtr getCurrentEvent() const;
    std::shared_ptr<Model::IStateNode> getCurrentStateNode() const;
    std::string getDataModelValue(const std::string &expression) const;
    std::string getScriptBasePath() const;

    // Additional missing methods needed for compatibility
    Events::EventPtr createEvent(const std::string &eventName, const std::string &data = "",
                                 const std::string &type = "");
    void activateState(const std::string &stateId);
    void deactivateState(const std::string &stateId);
    bool isStateActive(const std::string &stateId);
    std::vector<std::string> getDataNames() const;
    void setProperty(const std::string &name, const std::string &type);
    void setCurrentEvent(const std::string &eventName, const std::string &data = "");
    void setEventQueue(std::shared_ptr<Events::EventQueue> queue);
    void setEventDispatcher(std::shared_ptr<Events::EventDispatcher> dispatcher);
    void setIOProcessorManager(std::shared_ptr<void> manager);  // Using void* for now

    // ========== Event Scheduling ==========
    /**
     * @brief Schedule an event for delayed delivery
     * @param event Event to schedule
     * @param delayMs Delay in milliseconds
     * @param target Target for event delivery (empty for internal)
     * @param sendId Optional send ID for cancellation
     */
    void scheduleEvent(std::shared_ptr<Events::Event> event, uint64_t delayMs,
                       const std::string &target = std::string(), const std::string &sendId = std::string());

    /**
     * @brief Cancel a scheduled event by send ID
     * @param sendId Send ID to cancel
     * @return true if event was cancelled
     */
    bool cancelScheduledEvent(const std::string &sendId);

    /**
     * @brief Process all scheduled events that are ready for delivery
     * This should be called in the main event processing loop
     */
    void processScheduledEvents();

    /**
     * @brief Get the event scheduler (for advanced use)
     * @return Reference to the event scheduler
     */
    EventScheduler &getEventScheduler() {
        return eventScheduler_;
    }

    const EventScheduler &getEventScheduler() const {
        return eventScheduler_;
    }

    // ========== Final State Management ==========

    /**
     * @brief Set the final state reached flag
     * @param reached true if final state has been reached
     */
    void setFinalStateReached(bool reached);

    /**
     * @brief Check if final state has been reached
     * @return true if final state has been reached
     */
    bool isFinalStateReached() const;

    // ========== Manager Injection (for testing/configuration) ==========

    void setStateManager(std::unique_ptr<IStateManager> manager);
    void setEventManager(std::unique_ptr<IEventManager> manager);
    void setDataContextManager(std::unique_ptr<IDataContextManager> manager);
    void setInvokeSessionManager(std::unique_ptr<IInvokeSessionManager> manager);

private:
    // Manager composition
    std::unique_ptr<IStateManager> stateManager_;
    std::unique_ptr<IEventManager> eventManager_;
    std::unique_ptr<IDataContextManager> dataContextManager_;
    std::unique_ptr<IInvokeSessionManager> invokeSessionManager_;

    // Context state
    mutable std::mutex contextMutex_;
    bool initialized_ = false;
    bool finalStateReached_ = false;

    // Event scheduling
    EventScheduler eventScheduler_;

    // Factory methods for default implementations
    std::unique_ptr<IStateManager> createDefaultStateManager();
    std::unique_ptr<IEventManager> createDefaultEventManager();
    std::unique_ptr<IDataContextManager> createDefaultDataContextManager();
    std::unique_ptr<IInvokeSessionManager> createDefaultInvokeSessionManager();

    // Validation
    bool validateManagers() const;
};

}  // namespace Runtime
}  // namespace SCXML
