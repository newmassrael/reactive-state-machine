#include "runtime/RuntimeContext.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "runtime/DataModelEngine.h"
#include "runtime/impl/DataContextManager.h"
#include "runtime/impl/EventManager.h"
#include "runtime/impl/InvokeSessionManager.h"
#include "runtime/impl/StateManager.h"
#include <stdexcept>

using namespace SCXML;
using namespace SCXML::Runtime;

RuntimeContext::RuntimeContext(std::unique_ptr<IStateManager> stateManager, std::unique_ptr<IEventManager> eventManager,
                               std::unique_ptr<IDataContextManager> dataContextManager,
                               std::unique_ptr<IInvokeSessionManager> invokeSessionManager)
    : stateManager_(std::move(stateManager)), eventManager_(std::move(eventManager)),
      dataContextManager_(std::move(dataContextManager)), invokeSessionManager_(std::move(invokeSessionManager)) {
    // Create default managers if not provided
    if (!stateManager_) {
        stateManager_ = createDefaultStateManager();
    }
    if (!eventManager_) {
        eventManager_ = createDefaultEventManager();
    }
    if (!dataContextManager_) {
        dataContextManager_ = createDefaultDataContextManager();
    }
    if (!invokeSessionManager_) {
        invokeSessionManager_ = createDefaultInvokeSessionManager();
    }
}

RuntimeContext::~RuntimeContext() {
    shutdown();
}

// ========== IRuntimeContext Implementation ==========

IStateManager &RuntimeContext::getStateManager() {
    return *stateManager_;
}

const IStateManager &RuntimeContext::getStateManager() const {
    return *stateManager_;
}

IEventManager &RuntimeContext::getEventManager() {
    return *eventManager_;
}

const IEventManager &RuntimeContext::getEventManager() const {
    return *eventManager_;
}

IDataContextManager &RuntimeContext::getDataContextManager() {
    return *dataContextManager_;
}

const IDataContextManager &RuntimeContext::getDataContextManager() const {
    return *dataContextManager_;
}

IInvokeSessionManager &RuntimeContext::getInvokeSessionManager() {
    return *invokeSessionManager_;
}

const IInvokeSessionManager &RuntimeContext::getInvokeSessionManager() const {
    return *invokeSessionManager_;
}

void RuntimeContext::log(const std::string &level, const std::string &message) {
    // Use the Logger system
    if (level == "error") {
        SCXML::Common::Logger::error(message);
    } else if (level == "warning") {
        SCXML::Common::Logger::warning(message);
    } else if (level == "info") {
        SCXML::Common::Logger::info(message);
    } else {
        SCXML::Common::Logger::debug(message);
    }
}

bool RuntimeContext::initialize() {
    std::lock_guard<std::mutex> lock(contextMutex_);

    if (initialized_) {
        return true;
    }

    if (!validateManagers()) {
        return false;
    }

    // Initialize all managers
    // Note: Managers don't have initialize() in their interfaces yet,
    // this can be added later if needed

    initialized_ = true;
    return true;
}

void RuntimeContext::shutdown() {
    std::lock_guard<std::mutex> lock(contextMutex_);

    if (!initialized_) {
        return;
    }

    // Shutdown managers
    if (invokeSessionManager_) {
        invokeSessionManager_->terminateAllSessions();
    }

    if (eventManager_) {
        eventManager_->clearInternalQueue();
        eventManager_->clearExternalQueue();
    }

    if (dataContextManager_) {
        dataContextManager_->clearAllData();
    }

    initialized_ = false;
}

bool RuntimeContext::isInitialized() const {
    std::lock_guard<std::mutex> lock(contextMutex_);
    return initialized_;
}

std::string RuntimeContext::getStatusSummary() const {
    std::lock_guard<std::mutex> lock(contextMutex_);

    std::string status = "RuntimeContext Status:\n";
    status += "  Initialized: " + std::string(initialized_ ? "true" : "false") + "\n";

    if (initialized_) {
        std::string currentState = stateManager_->getCurrentState();
        status += "  Current State: " + currentState + "\n";

        size_t activeStatesCount = stateManager_->getActiveStates().size();
        status += "  Active States: " + std::to_string(activeStatesCount) + "\n";

        size_t activeSessionsCount = invokeSessionManager_->getActiveSessionCount();
        status += "  Active Sessions: " + std::to_string(activeSessionsCount) + "\n";
        status += "  Session ID: " + eventManager_->getSessionId() + "\n";
    }

    return status;
}

// ========== Backward Compatibility Methods ==========

void RuntimeContext::setCurrentState(const std::string &stateId) {
    stateManager_->setCurrentState(stateId);
}

std::string RuntimeContext::getCurrentState() const {
    return stateManager_->getCurrentState();
}

std::vector<std::string> RuntimeContext::getActiveStates() const {
    return stateManager_->getActiveStates();
}

bool RuntimeContext::isInState(const std::string &stateId) const {
    return stateManager_->isInState(stateId);
}

void RuntimeContext::raiseEvent(Events::EventPtr event) {
    eventManager_->raiseEvent(event);
}

void RuntimeContext::raiseEvent(const std::string &eventName, const std::string &data) {
    eventManager_->raiseEvent(eventName, data);
}

void RuntimeContext::sendEvent(Events::EventPtr event, const std::string &target, uint64_t delayMs) {
    eventManager_->sendEvent(event, target, delayMs);
}

void RuntimeContext::sendEvent(const std::string &eventName, const std::string &target, uint64_t delayMs,
                               const std::string &data) {
    eventManager_->sendEvent(eventName, target, delayMs, data);
}

void RuntimeContext::setDataValue(const std::string &id, const std::string &value) {
    dataContextManager_->setDataValue(id, value);
}

std::string RuntimeContext::getDataValue(const std::string &id) const {
    return dataContextManager_->getDataValue(id);
}

bool RuntimeContext::hasDataValue(const std::string &id) const {
    return dataContextManager_->hasDataValue(id);
}

std::string RuntimeContext::evaluateExpression(const std::string &expression) const {
    return dataContextManager_->evaluateExpression(expression);
}

bool RuntimeContext::evaluateCondition(const std::string &condition) const {
    return dataContextManager_->evaluateCondition(condition);
}

void RuntimeContext::setModel(std::shared_ptr<Model::DocumentModel> model) {
    stateManager_->initializeFromModel(model);
    dataContextManager_->initializeFromModel(model);
}

std::shared_ptr<Model::DocumentModel> RuntimeContext::getModel() const {
    return stateManager_->getModel();
}

void RuntimeContext::setSessionName(const std::string &name) {
    dataContextManager_->setSessionName(name);
}

std::string RuntimeContext::getSessionName() const {
    return dataContextManager_->getSessionName();
}

void RuntimeContext::setSessionId(const std::string &sessionId) {
    eventManager_->setSessionId(sessionId);
}

std::string RuntimeContext::getSessionId() const {
    return eventManager_->getSessionId();
}

// ========== Manager Injection ==========

void RuntimeContext::setStateManager(std::unique_ptr<IStateManager> manager) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    stateManager_ = std::move(manager);
}

void RuntimeContext::setEventManager(std::unique_ptr<IEventManager> manager) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    eventManager_ = std::move(manager);
}

void RuntimeContext::setDataContextManager(std::unique_ptr<IDataContextManager> manager) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    dataContextManager_ = std::move(manager);
}

void RuntimeContext::setInvokeSessionManager(std::unique_ptr<IInvokeSessionManager> manager) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    invokeSessionManager_ = std::move(manager);
}

// ========== Private Methods ==========

std::unique_ptr<IStateManager> RuntimeContext::createDefaultStateManager() {
    return std::make_unique<StateManager>();
}

std::unique_ptr<IEventManager> RuntimeContext::createDefaultEventManager() {
    return std::make_unique<EventManager>();
}

std::unique_ptr<IDataContextManager> RuntimeContext::createDefaultDataContextManager() {
    return std::make_unique<DataContextManager>();
}

std::unique_ptr<IInvokeSessionManager> RuntimeContext::createDefaultInvokeSessionManager() {
    return std::make_unique<InvokeSessionManager>();
}

bool RuntimeContext::validateManagers() const {
    return stateManager_ != nullptr && eventManager_ != nullptr && dataContextManager_ != nullptr &&
           invokeSessionManager_ != nullptr;
}

// ========== Additional Compatibility Methods ==========

SCXML::DataModelEngine *RuntimeContext::getDataModelEngine() const {
    return dataContextManager_->getDataModelEngine();
}

Logger *RuntimeContext::getLogger() const {
    // Logger is a static class, not a singleton - return nullptr for now
    // In practice, this method might not be needed since Logger is accessed statically
    return nullptr;
}

Events::EventPtr RuntimeContext::getCurrentEvent() const {
    return eventManager_->getCurrentEvent();
}

std::shared_ptr<Model::IStateNode> RuntimeContext::getCurrentStateNode() const {
    return stateManager_->getCurrentStateNode();
}

std::string RuntimeContext::getDataModelValue(const std::string &expression) const {
    return dataContextManager_->evaluateExpression(expression);
}

std::string RuntimeContext::getScriptBasePath() const {
    return dataContextManager_->getScriptBasePath();
}

// ========== Additional Compatibility Methods ==========

Events::EventPtr RuntimeContext::createEvent(const std::string &eventName, const std::string &data,
                                             const std::string &type) {
    // Note: type parameter is currently unused but kept for API compatibility
    (void)type;
    auto event = std::make_shared<Events::Event>(eventName);
    if (!data.empty()) {
        event->setData(data);
    }
    return event;
}

void RuntimeContext::activateState(const std::string &stateId) {
    stateManager_->enterState(stateId);
}

void RuntimeContext::deactivateState(const std::string &stateId) {
    stateManager_->exitState(stateId);
}

bool RuntimeContext::isStateActive(const std::string &stateId) {
    return stateManager_->isInState(stateId);
}

std::vector<std::string> RuntimeContext::getDataNames() const {
    auto allData = dataContextManager_->getAllData();
    std::vector<std::string> names;
    for (const auto &pair : allData) {
        names.push_back(pair.first);
    }
    return names;
}

void RuntimeContext::setProperty(const std::string &name, const std::string &type) {
    // Placeholder - store properties in data context
    if (auto *dcm = dynamic_cast<DataContextManager *>(dataContextManager_.get())) {
        dcm->setDataValue("_property_" + name, type);
    }
}

void RuntimeContext::setCurrentEvent(const std::string &eventName, const std::string &data) {
    auto event = createEvent(eventName, data);
    // Set as current event in event manager - would need to add this capability
}

void RuntimeContext::setEventQueue(std::shared_ptr<Events::EventQueue> queue) {
    // Placeholder - would need to enhance event manager to accept external queue
    (void)queue;  // Suppress unused parameter warning
    SCXML::Common::Logger::debug("RuntimeContext::setEventQueue - External queue injection not fully implemented");
}

void RuntimeContext::setEventDispatcher(std::shared_ptr<Events::EventDispatcher> dispatcher) {
    // Placeholder - would need to enhance event manager to accept external dispatcher
    (void)dispatcher;  // Suppress unused parameter warning
    SCXML::Common::Logger::debug(
        "RuntimeContext::setEventDispatcher - External dispatcher injection not fully implemented");
}

void RuntimeContext::setIOProcessorManager(std::shared_ptr<void> manager) {
    // Placeholder - would need to store in invoke session manager
    (void)manager;  // Suppress unused parameter warning
    SCXML::Common::Logger::debug(
        "RuntimeContext::setIOProcessorManager - IO processor manager injection not fully implemented");
}

// ========== Final State Management Implementation ==========

void RuntimeContext::setFinalStateReached(bool reached) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    finalStateReached_ = reached;
    SCXML::Common::Logger::debug("RuntimeContext::setFinalStateReached - Final state reached flag set to: " +
                                 std::to_string(reached));
}

bool RuntimeContext::isFinalStateReached() const {
    std::lock_guard<std::mutex> lock(contextMutex_);
    return finalStateReached_;
}

// ========== Event Scheduling Implementation ==========

void RuntimeContext::scheduleEvent(std::shared_ptr<Events::Event> event, uint64_t delayMs, const std::string &target,
                                   const std::string &sendId) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    eventScheduler_.scheduleEvent(event, delayMs, target, sendId);
}

bool RuntimeContext::cancelScheduledEvent(const std::string &sendId) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    return eventScheduler_.cancelEvent(sendId);
}

void RuntimeContext::processScheduledEvents() {
    std::vector<EventScheduler::DelayedEvent> readyEvents;

    // Get ready events under lock
    {
        std::lock_guard<std::mutex> lock(contextMutex_);
        readyEvents = eventScheduler_.getReadyEvents();
    }

    // Process events without holding the lock
    for (const auto &delayedEvent : readyEvents) {
        if (delayedEvent.target.empty() || delayedEvent.target == "#_internal") {
            // Internal event
            raiseEvent(delayedEvent.event);
        } else {
            // External event
            sendEvent(delayedEvent.event, delayedEvent.target);
        }
    }
}