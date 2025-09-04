#include "runtime/InvokeManager.h"
#include "common/Logger.h"
#include "model/IInvokeNode.h"
#include "runtime/InvokeProcessor.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>

namespace SCXML {

InvokeManager::InvokeManager() : context_(nullptr) {
    Logger::debug("InvokeManager: Constructor - Creating invoke manager");
}

InvokeManager::~InvokeManager() {
    Logger::debug("InvokeManager: Destructor - Destroying invoke manager");
    stopAllProcessors();
}

void InvokeManager::initialize(SCXML::Runtime::RuntimeContext *context) {
    if (!context) {
        Logger::error("InvokeManager: Cannot initialize with null context");
        return;
    }

    context_ = context;
    Logger::debug("InvokeManager: Initialized with runtime context");
}

bool InvokeManager::registerProcessor(std::shared_ptr<InvokeProcessor> processor) {
    if (!processor) {
        Logger::error("InvokeManager: Cannot register null processor");
        return false;
    }

    std::lock_guard<std::mutex> lock(processorsMutex_);
    const std::string &type = processor->getProcessorType();

    if (processors_.find(type) != processors_.end()) {
        Logger::warning("InvokeManager: Processor type already registered: " + type);
        return false;
    }

    processors_[type] = processor;
    setupProcessorCallbacks(processor);

    Logger::debug("InvokeManager: Registered processor: " + type);
    return true;
}

bool InvokeManager::unregisterProcessor(const std::string &processorType) {
    std::lock_guard<std::mutex> lock(processorsMutex_);
    auto it = processors_.find(processorType);
    if (it == processors_.end()) {
        Logger::warning("InvokeManager: Processor not found: " + processorType);
        return false;
    }

    // Stop the processor and terminate all its sessions
    it->second->stop();
    processors_.erase(it);

    Logger::debug("InvokeManager: Unregistered processor: " + processorType);
    return true;
}

InvokeManager::InvokeResult InvokeManager::startInvoke(std::shared_ptr<IInvokeNode> invokeNode,
                                                       const std::string &stateId) {
    InvokeResult result;

    if (!invokeNode) {
        result.errorMessage = "Invoke node is null";
        Logger::error("InvokeManager: " + result.errorMessage);
        return result;
    }

    if (!context_) {
        result.errorMessage = "InvokeManager not initialized with runtime context";
        Logger::error("InvokeManager: " + result.errorMessage);
        return result;
    }

    const std::string &invokeType = invokeNode->getType();
    if (invokeType.empty()) {
        result.errorMessage = "Invoke type is empty";
        Logger::error("InvokeManager: " + result.errorMessage);
        return result;
    }

    // Find appropriate processor
    auto processor = findProcessorForType(invokeType);
    if (!processor) {
        result.errorMessage = "No processor found for type: " + invokeType;
        Logger::error("InvokeManager: " + result.errorMessage);
        return result;
    }

    try {
        // Generate unique invoke ID
        std::string invokeId = generateInvokeId(stateId);

        // Start the invoke session
        std::string sessionId = processor->startInvoke(invokeNode, *context_);
        if (sessionId.empty()) {
            result.errorMessage = "Processor failed to start invoke session";
            Logger::error("InvokeManager: " + result.errorMessage);
            incrementFailedInvokes();
            return result;
        }

        // Create active invoke record
        auto activeInvoke = std::make_shared<ActiveInvoke>(sessionId, invokeId, stateId);
        activeInvoke->processorType = invokeType;
        activeInvoke->invokeNode = invokeNode;
        activeInvoke->autoForward = invokeNode->isAutoForward();

        // Store active invoke
        {
            std::lock_guard<std::mutex> lock(invokesMutex_);
            activeInvokes_[invokeId] = activeInvoke;
            sessionToInvokeMap_[sessionId] = invokeId;
        }

        // Update statistics
        incrementTotalInvokes();
        incrementActiveInvokes();
        incrementSuccessfulInvokes();

        // Update processor usage statistics
        {
            std::lock_guard<std::mutex> lock(statisticsMutex_);
            statistics_.processorUsage[invokeType]++;
        }

        result.success = true;
        result.sessionId = sessionId;
        result.processorType = invokeType;

        Logger::debug("InvokeManager: Successfully started invoke - ID: " + invokeId + ", Session: " + sessionId +
                      ", Type: " + invokeType);

        // Trigger invoke event callback
        if (invokeEventCallback_) {
            invokeEventCallback_(invokeId, "started", "");
        }

    } catch (const std::exception &e) {
        result.errorMessage = "Exception starting invoke: " + std::string(e.what());
        Logger::error("InvokeManager: " + result.errorMessage);
        incrementFailedInvokes();
    }

    return result;
}

bool InvokeManager::sendEventToInvoke(const std::string &invokeId, SCXML::Events::EventPtr event) {
    if (!event) {
        Logger::error("InvokeManager: Cannot send null event");
        return false;
    }

    std::shared_ptr<ActiveInvoke> activeInvoke;
    {
        std::lock_guard<std::mutex> lock(invokesMutex_);
        auto it = activeInvokes_.find(invokeId);
        if (it == activeInvokes_.end()) {
            Logger::warning("InvokeManager: Active invoke not found: " + invokeId);
            return false;
        }
        activeInvoke = it->second;
    }

    // Find processor
    auto processor = findProcessorForType(activeInvoke->processorType);
    if (!processor) {
        Logger::error("InvokeManager: Processor not found for type: " + activeInvoke->processorType);
        return false;
    }

    // Send event to session
    bool success = processor->sendEvent(activeInvoke->sessionId, event);
    if (success) {
        incrementEventsForwarded();
        Logger::debug("InvokeManager: Sent event to invoke " + invokeId + " (session: " + activeInvoke->sessionId +
                      ")");
    } else {
        Logger::error("InvokeManager: Failed to send event to invoke " + invokeId);
    }

    return success;
}

bool InvokeManager::terminateInvoke(const std::string &invokeId) {
    std::shared_ptr<ActiveInvoke> activeInvoke;
    {
        std::lock_guard<std::mutex> lock(invokesMutex_);
        auto it = activeInvokes_.find(invokeId);
        if (it == activeInvokes_.end()) {
            Logger::warning("InvokeManager: Active invoke not found for termination: " + invokeId);
            return false;
        }
        activeInvoke = it->second;
    }

    // Find processor
    auto processor = findProcessorForType(activeInvoke->processorType);
    if (!processor) {
        Logger::error("InvokeManager: Processor not found for type: " + activeInvoke->processorType);
        return false;
    }

    try {
        // Terminate session
        bool success = processor->terminateInvoke(activeInvoke->sessionId);

        if (success) {
            // Remove from active invokes
            {
                std::lock_guard<std::mutex> lock(invokesMutex_);
                activeInvokes_.erase(invokeId);
                sessionToInvokeMap_.erase(activeInvoke->sessionId);
            }

            decrementActiveInvokes();

            Logger::debug("InvokeManager: Terminated invoke: " + invokeId);

            // Trigger invoke event callback
            if (invokeEventCallback_) {
                invokeEventCallback_(invokeId, "terminated", "");
            }
        } else {
            Logger::error("InvokeManager: Failed to terminate invoke: " + invokeId);
        }

        return success;

    } catch (const std::exception &e) {
        Logger::error("InvokeManager: Exception terminating invoke " + invokeId + ": " + e.what());
        return false;
    }
}

size_t InvokeManager::terminateInvokesForState(const std::string &stateId) {
    std::vector<std::string> invokesToTerminate;

    // Find all invokes for this state
    {
        std::lock_guard<std::mutex> lock(invokesMutex_);
        for (const auto &[invokeId, activeInvoke] : activeInvokes_) {
            if (activeInvoke->stateId == stateId) {
                invokesToTerminate.push_back(invokeId);
            }
        }
    }

    // Terminate each invoke
    size_t terminatedCount = 0;
    for (const std::string &invokeId : invokesToTerminate) {
        if (terminateInvoke(invokeId)) {
            terminatedCount++;
        }
    }

    Logger::debug("InvokeManager: Terminated " + std::to_string(terminatedCount) + " invokes for state: " + stateId);

    return terminatedCount;
}

std::shared_ptr<InvokeManager::ActiveInvoke> InvokeManager::getActiveInvoke(const std::string &invokeId) const {
    std::lock_guard<std::mutex> lock(invokesMutex_);
    auto it = activeInvokes_.find(invokeId);
    return (it != activeInvokes_.end()) ? it->second : nullptr;
}

std::unordered_map<std::string, std::shared_ptr<InvokeManager::ActiveInvoke>>
InvokeManager::getAllActiveInvokes() const {
    std::lock_guard<std::mutex> lock(invokesMutex_);
    return activeInvokes_;
}

std::vector<std::shared_ptr<InvokeManager::ActiveInvoke>>
InvokeManager::getInvokesForState(const std::string &stateId) const {
    std::vector<std::shared_ptr<ActiveInvoke>> result;

    std::lock_guard<std::mutex> lock(invokesMutex_);
    for (const auto &[invokeId, activeInvoke] : activeInvokes_) {
        if (activeInvoke->stateId == stateId) {
            result.push_back(activeInvoke);
        }
    }

    return result;
}

bool InvokeManager::isInvokeActive(const std::string &invokeId) const {
    std::lock_guard<std::mutex> lock(invokesMutex_);
    return activeInvokes_.find(invokeId) != activeInvokes_.end();
}

size_t InvokeManager::processParentEvent(SCXML::Events::EventPtr event) {
    if (!event) {
        return 0;
    }

    size_t forwardedCount = 0;
    std::vector<std::shared_ptr<ActiveInvoke>> autoForwardInvokes;

    // Find invokes with auto-forward enabled
    {
        std::lock_guard<std::mutex> lock(invokesMutex_);
        for (const auto &[invokeId, activeInvoke] : activeInvokes_) {
            if (activeInvoke->autoForward) {
                // Apply event filter if configured
                if (!eventFilter_ || eventFilter_(event, activeInvoke->sessionId)) {
                    autoForwardInvokes.push_back(activeInvoke);
                }
            }
        }
    }

    // Forward event to eligible invokes
    for (const auto &activeInvoke : autoForwardInvokes) {
        if (sendEventToInvoke(activeInvoke->invokeId, event)) {
            forwardedCount++;
        }
    }

    if (forwardedCount > 0) {
        Logger::debug("InvokeManager: Auto-forwarded event to " + std::to_string(forwardedCount) + " invokes");
    }

    return forwardedCount;
}

void InvokeManager::resetStatistics() {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_ = Statistics{};
    Logger::debug("InvokeManager: Statistics reset");
}

bool InvokeManager::startAllProcessors() {
    std::lock_guard<std::mutex> lock(processorsMutex_);
    bool allStarted = true;

    for (auto &[type, processor] : processors_) {
        if (!processor->start()) {
            Logger::error("InvokeManager: Failed to start processor: " + type);
            allStarted = false;
        } else {
            Logger::debug("InvokeManager: Started processor: " + type);
        }
    }

    return allStarted;
}

bool InvokeManager::stopAllProcessors() {
    std::lock_guard<std::mutex> lock(processorsMutex_);
    bool allStopped = true;

    for (auto &[type, processor] : processors_) {
        if (!processor->stop()) {
            Logger::error("InvokeManager: Failed to stop processor: " + type);
            allStopped = false;
        } else {
            Logger::debug("InvokeManager: Stopped processor: " + type);
        }
    }

    // Clear all active invokes
    {
        std::lock_guard<std::mutex> lock(invokesMutex_);
        activeInvokes_.clear();
        sessionToInvokeMap_.clear();
        statistics_.activeInvokes = 0;
    }

    return allStopped;
}

std::vector<std::string> InvokeManager::getRegisteredProcessorTypes() const {
    std::lock_guard<std::mutex> lock(processorsMutex_);
    std::vector<std::string> types;
    types.reserve(processors_.size());

    for (const auto &[type, processor] : processors_) {
        types.push_back(type);
    }

    return types;
}

std::shared_ptr<InvokeProcessor> InvokeManager::findProcessorForType(const std::string &type) const {
    std::lock_guard<std::mutex> lock(processorsMutex_);

    // First try direct match
    auto it = processors_.find(type);
    if (it != processors_.end()) {
        return it->second;
    }

    // Then try processors that can handle this type
    for (const auto &[processorType, processor] : processors_) {
        if (processor->canHandle(type)) {
            return processor;
        }
    }

    return nullptr;
}

void InvokeManager::handleChildEvent(const std::string &sessionId, SCXML::Events::EventPtr event) {
    if (!event || !context_) {
        return;
    }

    // Find the invoke ID for this session
    std::string invokeId;
    {
        std::lock_guard<std::mutex> lock(invokesMutex_);
        auto it = sessionToInvokeMap_.find(sessionId);
        if (it != sessionToInvokeMap_.end()) {
            invokeId = it->second;
        }
    }

    if (invokeId.empty()) {
        Logger::warning("InvokeManager: Received event from unknown session: " + sessionId);
        return;
    }

    incrementEventsReceived();
    Logger::debug("InvokeManager: Received event from child session " + sessionId + " (invoke: " + invokeId + ")");

    // Forward event to parent state machine
    // Note: This would need to be connected to the EventDispatcher
    // For now, just log the event reception
    Logger::debug("InvokeManager: Forwarding child event to parent state machine");
}

void InvokeManager::handleSessionStateChange(const std::string &sessionId, int oldState, int newState) {
    std::string invokeId;
    {
        std::lock_guard<std::mutex> lock(invokesMutex_);
        auto it = sessionToInvokeMap_.find(sessionId);
        if (it != sessionToInvokeMap_.end()) {
            invokeId = it->second;
        }
    }

    if (invokeId.empty()) {
        return;
    }

    Logger::debug("InvokeManager: Session state change - Session: " + sessionId + ", Invoke: " + invokeId +
                  ", State: " + std::to_string(oldState) + " -> " + std::to_string(newState));

    // Handle session completion
    if (newState == static_cast<int>(InvokeProcessor::SessionState::FINISHED)) {
        // Create done.invoke event
        auto doneEvent = createDoneInvokeEvent(invokeId);
        if (doneEvent && context_) {
            // Forward to parent state machine event system
            Logger::debug("InvokeManager: Generated done.invoke." + invokeId + " event");
        }

        // Remove from active invokes
        terminateInvoke(invokeId);
    } else if (newState == static_cast<int>(InvokeProcessor::SessionState::ERROR)) {
        // Create error.invoke event
        auto errorEvent = createErrorInvokeEvent(invokeId, "Session encountered error");
        if (errorEvent && context_) {
            // Forward to parent state machine event system
            Logger::debug("InvokeManager: Generated error.invoke." + invokeId + " event");
        }

        // Remove from active invokes
        terminateInvoke(invokeId);
    }
}

std::string InvokeManager::generateInvokeId(const std::string &stateId) {
    uint64_t counter = ++invokeCounter_;
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::stringstream ss;
    ss << stateId << "_invoke_" << counter << "_" << timestamp;
    return ss.str();
}

SCXML::Events::EventPtr InvokeManager::createDoneInvokeEvent(const std::string &invokeId, const std::string &data) {
    // Create a proper done.invoke.* event
    std::string eventName = "done.invoke." + invokeId;
    Logger::debug("InvokeManager: Creating " + eventName + " event with data: " + data);

    auto event = std::make_shared<SCXML::Events::Event>(eventName);
    event->setOrigin("invoke.manager");
    if (!data.empty()) {
        event->setData(data);
    }

    return event;
}

SCXML::Events::EventPtr InvokeManager::createErrorInvokeEvent(const std::string &invokeId,
                                                              const std::string &errorMessage) {
    // Create a proper error.invoke.* event
    std::string eventName = "error.invoke." + invokeId;
    Logger::debug("InvokeManager: Creating " + eventName + " event with error: " + errorMessage);

    auto event = std::make_shared<SCXML::Events::Event>(eventName);
    event->setOrigin("invoke.manager");
    event->setData(errorMessage);

    return event;
}

void InvokeManager::setupProcessorCallbacks(std::shared_ptr<InvokeProcessor> processor) {
    if (!processor) {
        return;
    }

    // Set up event callback
    processor->setEventCallback(
        [this](const std::string &sessionId, SCXML::Events::EventPtr event) { handleChildEvent(sessionId, event); });

    // Set up state change callback
    processor->setStateChangeCallback([this](const std::string &sessionId, InvokeProcessor::SessionState oldState,
                                             InvokeProcessor::SessionState newState) {
        handleSessionStateChange(sessionId, static_cast<int>(oldState), static_cast<int>(newState));
    });

    Logger::debug("InvokeManager: Set up callbacks for processor: " + processor->getProcessorType());
}

}  // namespace SCXML