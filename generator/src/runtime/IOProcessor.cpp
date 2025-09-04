#include "runtime/IOProcessor.h"
#include "common/Logger.h"
#include "events/Event.h"
#include <chrono>

using namespace SCXML::Runtime;
using namespace SCXML::Events;

IOProcessor::IOProcessor(ProcessorType processorType, const std::string &typeURI)
    : processorType_(processorType), typeURI_(typeURI) {}

bool IOProcessor::canHandle(const std::string &typeURI) const {
    return typeURI_ == typeURI;
}

void IOProcessor::fireIncomingEvent(const EventPtr &event) {
    if (eventCallback_) {
        incrementEventsReceived();
        eventCallback_(event);
    } else {
        SCXML::Common::Logger::warning("IOProcessor: No event callback set - dropping incoming event: " + event->getName());
        incrementReceiveFailures();
    }
}

void IOProcessor::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = Statistics{};
}

EventPtr IOProcessor::createErrorEvent(const std::string &originalSendId, const std::string &errorType,
                                       const std::string &errorMessage) {
    auto errorEvent = std::make_shared<Event>("error.communication");

    // Set error message as structured string data
    std::string errorMsg = "type=" + errorType + ";sendid=" + originalSendId + ";message=" + errorMessage;
    errorEvent->setData(errorMsg);
    return errorEvent;
}

// IOProcessorManager implementation
IOProcessorManager &IOProcessorManager::getInstance() {
    static IOProcessorManager instance;
    return instance;
}

bool IOProcessorManager::registerProcessor(std::shared_ptr<IOProcessor> processor) {
    if (!processor) {
        SCXML::Common::Logger::error("IOProcessorManager: Cannot register null processor");
        return false;
    }

    std::lock_guard<std::mutex> lock(processorsMutex_);

    const std::string &typeURI = processor->getTypeURI();

    if (processors_.find(typeURI) != processors_.end()) {
        SCXML::Common::Logger::warning("IOProcessorManager: Processor already registered for type: " + typeURI);
        return false;
    }

    // Set global event callback if available
    if (globalEventCallback_) {
        processor->setEventCallback(globalEventCallback_);
    }

    processors_[typeURI] = processor;

    SCXML::Common::Logger::info("IOProcessorManager: Registered I/O Processor: " + typeURI);
    return true;
}

bool IOProcessorManager::unregisterProcessor(const std::string &typeURI) {
    std::lock_guard<std::mutex> lock(processorsMutex_);

    auto it = processors_.find(typeURI);
    if (it == processors_.end()) {
        SCXML::Common::Logger::warning("IOProcessorManager: No processor registered for type: " + typeURI);
        return false;
    }

    // Stop the processor before removing
    if (it->second->isRunning()) {
        it->second->stop();
    }

    processors_.erase(it);

    // Clean up sendId mappings
    for (auto sendIt = sendIdToProcessorType_.begin(); sendIt != sendIdToProcessorType_.end();) {
        if (sendIt->second == typeURI) {
            sendIt = sendIdToProcessorType_.erase(sendIt);
        } else {
            ++sendIt;
        }
    }

    SCXML::Common::Logger::info("IOProcessorManager: Unregistered I/O Processor: " + typeURI);
    return true;
}

std::shared_ptr<IOProcessor> IOProcessorManager::getProcessor(const std::string &typeURI) {
    std::lock_guard<std::mutex> lock(processorsMutex_);

    auto it = processors_.find(typeURI);
    if (it != processors_.end()) {
        return it->second;
    }

    // Try to find a processor that can handle this type URI
    for (const auto &pair : processors_) {
        if (pair.second->canHandle(typeURI)) {
            return pair.second;
        }
    }

    return nullptr;
}

bool IOProcessorManager::send(const IOProcessor::SendParameters &params) {
    auto processor = getProcessor(params.type);
    if (!processor) {
        SCXML::Common::Logger::error("IOProcessorManager: No processor available for type: " + params.type);
        return false;
    }

    // Track which processor handles this sendId
    if (!params.sendId.empty()) {
        std::lock_guard<std::mutex> lock(processorsMutex_);
        sendIdToProcessorType_[params.sendId] = params.type;
    }

    bool success = processor->send(params);
    if (!success) {
        // Remove tracking if send failed
        if (!params.sendId.empty()) {
            std::lock_guard<std::mutex> lock(processorsMutex_);
            sendIdToProcessorType_.erase(params.sendId);
        }
        SCXML::Common::Logger::error("IOProcessorManager: Failed to send event '" + params.event + "' to " + params.target);
    }

    return success;
}

bool IOProcessorManager::cancelSend(const std::string &sendId) {
    if (sendId.empty()) {
        SCXML::Common::Logger::warning("IOProcessorManager: Cannot cancel send with empty sendId");
        return false;
    }

    std::lock_guard<std::mutex> lock(processorsMutex_);

    auto it = sendIdToProcessorType_.find(sendId);
    if (it == sendIdToProcessorType_.end()) {
        SCXML::Common::Logger::warning("IOProcessorManager: No send operation found for sendId: " + sendId);
        return false;
    }

    auto processor = getProcessor(it->second);
    if (!processor) {
        SCXML::Common::Logger::error("IOProcessorManager: Processor not found for sendId: " + sendId);
        sendIdToProcessorType_.erase(it);
        return false;
    }

    bool success = processor->cancelSend(sendId);
    if (success) {
        sendIdToProcessorType_.erase(it);
        SCXML::Common::Logger::info("IOProcessorManager: Cancelled send operation: " + sendId);
    }

    return success;
}

bool IOProcessorManager::startAll() {
    std::lock_guard<std::mutex> lock(processorsMutex_);

    bool allSuccess = true;
    for (const auto &pair : processors_) {
        if (!pair.second->isRunning()) {
            bool started = pair.second->start();
            if (!started) {
                SCXML::Common::Logger::error("IOProcessorManager: Failed to start processor: " + pair.first);
                allSuccess = false;
            } else {
                SCXML::Common::Logger::info("IOProcessorManager: Started processor: " + pair.first);
            }
        }
    }

    return allSuccess;
}

bool IOProcessorManager::stopAll() {
    std::lock_guard<std::mutex> lock(processorsMutex_);

    bool allSuccess = true;
    for (const auto &pair : processors_) {
        if (pair.second->isRunning()) {
            bool stopped = pair.second->stop();
            if (!stopped) {
                SCXML::Common::Logger::error("IOProcessorManager: Failed to stop processor: " + pair.first);
                allSuccess = false;
            } else {
                SCXML::Common::Logger::info("IOProcessorManager: Stopped processor: " + pair.first);
            }
        }
    }

    // Clear all send tracking
    sendIdToProcessorType_.clear();

    return allSuccess;
}

std::vector<std::string> IOProcessorManager::getRegisteredTypes() const {
    std::lock_guard<std::mutex> lock(processorsMutex_);

    std::vector<std::string> types;
    types.reserve(processors_.size());

    for (const auto &pair : processors_) {
        types.push_back(pair.first);
    }

    return types;
}

void IOProcessorManager::setGlobalEventCallback(IOProcessor::EventCallback callback) {
    std::lock_guard<std::mutex> lock(processorsMutex_);

    globalEventCallback_ = callback;

    // Set callback on all existing processors
    for (const auto &pair : processors_) {
        pair.second->setEventCallback(callback);
    }

    SCXML::Common::Logger::info("IOProcessorManager: Set global event callback for all I/O Processors");
}