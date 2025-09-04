#include "runtime/InterModuleCommunicator.h"
#include "common/GracefulJoin.h"
#include "common/Logger.h"
#include "common/RuntimeContext.h"
#include <algorithm>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>

namespace SCXML {
namespace Runtime {

// InterModuleMessageQueue Implementation
InterModuleMessageQueue::InterModuleMessageQueue() = default;
InterModuleMessageQueue::~InterModuleMessageQueue() = default;

void InterModuleMessageQueue::enqueue(const InterModuleMessage &message) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    messageQueue_.push(message);
    queueCondition_.notify_one();
}

bool InterModuleMessageQueue::dequeue(InterModuleMessage &message, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(queueMutex_);

    if (queueCondition_.wait_for(lock, timeout, [this] { return !messageQueue_.empty(); })) {
        message = messageQueue_.top();
        messageQueue_.pop();
        return true;
    }

    return false;
}

size_t InterModuleMessageQueue::size() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return messageQueue_.size();
}

bool InterModuleMessageQueue::empty() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return messageQueue_.empty();
}

void InterModuleMessageQueue::clear() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    std::priority_queue<InterModuleMessage, std::vector<InterModuleMessage>, MessageComparator> empty;
    messageQueue_.swap(empty);
    queueCondition_.notify_all();
}

// InterModuleCommunicator Implementation
InterModuleCommunicator::InterModuleCommunicator()
    : messageQueue_(std::make_unique<InterModuleMessageQueue>()), isRunning_(false), threadCount_(2),
      maxHistorySize_(1000) {
    // Initialize statistics
    statistics_["messages_sent"] = 0;
    statistics_["messages_received"] = 0;
    statistics_["messages_processed"] = 0;
    statistics_["messages_failed"] = 0;
    statistics_["channels_created"] = 0;
    statistics_["routes_added"] = 0;
}

InterModuleCommunicator::~InterModuleCommunicator() {
    stop();
}

SCXML::Common::Result<void> InterModuleCommunicator::registerModule(const std::string &moduleId,
                                                                    std::shared_ptr<RuntimeContext> context) {
    if (moduleId.empty()) {
        return SCXML::Common::Result<void>::error("Module ID cannot be empty");
    }

    if (!InterModuleUtils::isValidModuleId(moduleId)) {
        return SCXML::Common::Result<void>::error("Invalid module ID format: " + moduleId);
    }

    try {
        std::lock_guard<std::mutex> lock(modulesMutex_);

        if (registeredModules_.find(moduleId) != registeredModules_.end()) {
            return SCXML::Common::Result<void>::error("Module already registered: " + moduleId);
        }

        registeredModules_[moduleId] = context;

        SCXML::Common::Logger::info("InterModuleCommunicator", "Registered module: " + moduleId);
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to register module: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> InterModuleCommunicator::unregisterModule(const std::string &moduleId) {
    try {
        std::lock_guard<std::mutex> modulesLock(modulesMutex_);

        auto it = registeredModules_.find(moduleId);
        if (it == registeredModules_.end()) {
            return SCXML::Common::Result<void>::error("Module not registered: " + moduleId);
        }

        registeredModules_.erase(it);

        // Remove message handlers for this module
        {
            std::lock_guard<std::mutex> handlersLock(handlersMutex_);
            messageHandlers_.erase(moduleId);
        }

        // Close channels involving this module
        {
            std::lock_guard<std::mutex> channelsLock(channelsMutex_);
            for (auto channelIt = channels_.begin(); channelIt != channels_.end();) {
                if (channelIt->second.sourceModuleId == moduleId || channelIt->second.targetModuleId == moduleId) {
                    channelIt = channels_.erase(channelIt);
                } else {
                    ++channelIt;
                }
            }
        }

        SCXML::Common::Logger::info("InterModuleCommunicator", "Unregistered module: " + moduleId);
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to unregister module: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> InterModuleCommunicator::sendMessage(const InterModuleMessage &message) {
    auto validateResult = validateMessage(message);
    if (!validateResult.isSuccess()) {
        return validateResult;
    }

    try {
        messageQueue_->enqueue(message);
        updateStatistics("messages_sent");
        addToHistory(message);

        SCXML::Common::Logger::debug("InterModuleCommunicator",
                                     "Queued message from " + message.sourceModuleId + " to " + message.targetModuleId);

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        updateStatistics("messages_failed");
        return SCXML::Common::Result<void>::error("Failed to send message: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> InterModuleCommunicator::sendMessageWithResponse(const InterModuleMessage &message,
                                                                             ResponseHandler responseHandler,
                                                                             std::chrono::milliseconds timeout) {
    if (!responseHandler) {
        return SCXML::Common::Result<void>::error("Response handler cannot be null");
    }

    try {
        InterModuleMessage requestMessage = message;
        requestMessage.requiresResponse = true;
        requestMessage.correlationId = generateCorrelationId();

        // Register response handler
        {
            std::lock_guard<std::mutex> lock(responseHandlersMutex_);
            responseHandlers_[requestMessage.correlationId] = responseHandler;
        }

        // Send the message
        auto sendResult = sendMessage(requestMessage);
        if (!sendResult.isSuccess()) {
            // Remove registered handler on failure
            std::lock_guard<std::mutex> lock(responseHandlersMutex_);
            responseHandlers_.erase(requestMessage.correlationId);
            return sendResult;
        }

        // Set up timeout handling
        std::thread timeoutThread([this, correlationId = requestMessage.correlationId, timeout]() {
            std::this_thread::sleep_for(timeout);

            std::lock_guard<std::mutex> lock(responseHandlersMutex_);
            auto it = responseHandlers_.find(correlationId);
            if (it != responseHandlers_.end()) {
                // Create timeout response
                InterModuleMessage timeoutResponse;
                timeoutResponse.messageType = "response";
                timeoutResponse.correlationId = correlationId;
                timeoutResponse.messageData["error"] = "timeout";

                it->second(timeoutResponse);
                responseHandlers_.erase(it);
            }
        });
        timeoutThread.detach();

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to send message with response: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> InterModuleCommunicator::broadcastMessage(const InterModuleMessage &message) {
    try {
        InterModuleMessage broadcastMessage = message;
        broadcastMessage.targetModuleId = "";  // Empty target means broadcast

        return sendMessage(broadcastMessage);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to broadcast message: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> InterModuleCommunicator::registerMessageHandler(const std::string &moduleId,
                                                                            const std::string &messageType,
                                                                            MessageHandler handler) {
    if (moduleId.empty() || messageType.empty()) {
        return SCXML::Common::Result<void>::error("Module ID and message type cannot be empty");
    }

    if (!handler) {
        return SCXML::Common::Result<void>::error("Message handler cannot be null");
    }

    try {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        messageHandlers_[moduleId][messageType] = handler;

        SCXML::Common::Logger::info("InterModuleCommunicator",
                                    "Registered message handler for module: " + moduleId + ", type: " + messageType);

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to register message handler: " + std::string(e.what()));
    }
}

SCXML::Common::Result<std::string> InterModuleCommunicator::createChannel(const std::string &sourceModuleId,
                                                                          const std::string &targetModuleId,
                                                                          const std::string &channelType) {
    if (sourceModuleId.empty() || targetModuleId.empty()) {
        return SCXML::Common::Result<std::string>::error("Source and target module IDs cannot be empty");
    }

    try {
        std::lock_guard<std::mutex> lock(channelsMutex_);

        std::string channelId = generateChannelId(sourceModuleId, targetModuleId);

        if (channels_.find(channelId) != channels_.end()) {
            return SCXML::Common::Result<std::string>::error("Channel already exists: " + channelId);
        }

        CommunicationChannel channel(channelId, sourceModuleId, targetModuleId, channelType);
        channels_[channelId] = channel;

        updateStatistics("channels_created");

        SCXML::Common::Logger::info("InterModuleCommunicator",
                                    "Created channel: " + channelId + " (" + channelType + ")");

        return SCXML::Common::Result<std::string>::success(channelId);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>::error("Failed to create channel: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> InterModuleCommunicator::closeChannel(const std::string &channelId) {
    try {
        std::lock_guard<std::mutex> lock(channelsMutex_);

        auto it = channels_.find(channelId);
        if (it == channels_.end()) {
            return SCXML::Common::Result<void>::error("Channel not found: " + channelId);
        }

        channels_.erase(it);

        SCXML::Common::Logger::info("InterModuleCommunicator", "Closed channel: " + channelId);
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to close channel: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> InterModuleCommunicator::addRoute(const MessageRoute &route) {
    if (route.routeId.empty() || route.pattern.empty()) {
        return SCXML::Common::Result<void>::error("Route ID and pattern cannot be empty");
    }

    try {
        std::lock_guard<std::mutex> lock(routesMutex_);

        if (routes_.find(route.routeId) != routes_.end()) {
            return SCXML::Common::Result<void>::error("Route already exists: " + route.routeId);
        }

        routes_[route.routeId] = route;
        updateStatistics("routes_added");

        SCXML::Common::Logger::info("InterModuleCommunicator",
                                    "Added route: " + route.routeId + " (pattern: " + route.pattern + ")");

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to add route: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> InterModuleCommunicator::removeRoute(const std::string &routeId) {
    try {
        std::lock_guard<std::mutex> lock(routesMutex_);

        auto it = routes_.find(routeId);
        if (it == routes_.end()) {
            return SCXML::Common::Result<void>::error("Route not found: " + routeId);
        }

        routes_.erase(it);

        SCXML::Common::Logger::info("InterModuleCommunicator", "Removed route: " + routeId);
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to remove route: " + std::string(e.what()));
    }
}

std::map<std::string, size_t> InterModuleCommunicator::getStatistics() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);

    auto stats = statistics_;
    stats["queue_size"] = messageQueue_->size();
    stats["registered_modules"] = registeredModules_.size();
    stats["active_channels"] = channels_.size();
    stats["active_routes"] = routes_.size();

    return stats;
}

SCXML::Common::Result<void> InterModuleCommunicator::start() {
    if (isRunning_.load()) {
        return SCXML::Common::Result<void>::error("Inter-module communicator already running");
    }

    try {
        isRunning_.store(true);

        // Start worker threads
        workerThreads_.reserve(threadCount_);
        for (size_t i = 0; i < threadCount_; ++i) {
            workerThreads_.emplace_back(&InterModuleCommunicator::messageProcessingWorker, this);
        }

        SCXML::Common::Logger::info("InterModuleCommunicator",
                                    "Started with " + std::to_string(threadCount_) + " worker threads");

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        isRunning_.store(false);
        return SCXML::Common::Result<void>::error("Failed to start: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> InterModuleCommunicator::stop() {
    if (!isRunning_.load()) {
        return SCXML::Common::Result<void>::success();
    }

    try {
        isRunning_.store(false);
        messageQueue_->clear();

        // Wait for worker threads to finish with timeout
        for (size_t i = 0; i < workerThreads_.size(); ++i) {
            auto &thread = workerThreads_[i];
            if (thread.joinable()) {
                GracefulJoin::joinWithTimeout(thread, 3, "InterModuleCommunicator_Worker_" + std::to_string(i));
            }
        }
        workerThreads_.clear();

        SCXML::Common::Logger::info("InterModuleCommunicator", "Stopped");
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Failed to stop: " + std::string(e.what()));
    }
}

void InterModuleCommunicator::setThreadCount(size_t threadCount) {
    if (isRunning_.load()) {
        SCXML::Common::Logger::warning("InterModuleCommunicator",
                                       "Cannot change thread count while running. Stop first.");
        return;
    }

    threadCount_ = std::max(size_t(1), threadCount);
}

std::vector<CommunicationChannel> InterModuleCommunicator::getActiveChannels() const {
    std::lock_guard<std::mutex> lock(channelsMutex_);

    std::vector<CommunicationChannel> result;
    for (const auto &pair : channels_) {
        if (pair.second.isActive) {
            result.push_back(pair.second);
        }
    }

    return result;
}

std::vector<InterModuleMessage> InterModuleCommunicator::getMessageHistory(size_t maxMessages) const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);

    size_t count = std::min(maxMessages, messageHistory_.size());
    if (count == 0) {
        return {};
    }

    return std::vector<InterModuleMessage>(messageHistory_.end() - count, messageHistory_.end());
}

// Private methods
void InterModuleCommunicator::messageProcessingWorker() {
    SCXML::Common::Logger::debug("InterModuleCommunicator", "Message processing worker started");

    while (isRunning_.load()) {
        try {
            InterModuleMessage message;
            if (messageQueue_->dequeue(message, std::chrono::milliseconds(100))) {
                auto result = processMessage(message);
                if (result.isSuccess()) {
                    updateStatistics("messages_processed");
                } else {
                    updateStatistics("messages_failed");
                    SCXML::Common::Logger::error("InterModuleCommunicator",
                                                 "Failed to process message: " + result.getError());
                }
            }
        } catch (const std::exception &e) {
            updateStatistics("messages_failed");
            SCXML::Common::Logger::error("InterModuleCommunicator", "Worker thread error: " + std::string(e.what()));
        }
    }

    SCXML::Common::Logger::debug("InterModuleCommunicator", "Message processing worker stopped");
}

SCXML::Common::Result<void> InterModuleCommunicator::processMessage(const InterModuleMessage &message) {
    try {
        updateStatistics("messages_received");

        // Handle response messages
        if (message.messageType == "response" && !message.correlationId.empty()) {
            handleResponse(message);
            return SCXML::Common::Result<void>::success();
        }

        // Route message to target modules
        std::vector<std::string> targetModules = routeMessage(message);

        // Deliver to each target
        for (const auto &targetModuleId : targetModules) {
            auto deliverResult = deliverMessage(targetModuleId, message);
            if (!deliverResult.isSuccess()) {
                SCXML::Common::Logger::warning("InterModuleCommunicator", "Failed to deliver message to " +
                                                                              targetModuleId + ": " +
                                                                              deliverResult.getError());
            }
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Message processing failed: " + std::string(e.what()));
    }
}

std::vector<std::string> InterModuleCommunicator::routeMessage(const InterModuleMessage &message) {
    std::vector<std::string> targets;

    try {
        // Direct target specified
        if (!message.targetModuleId.empty()) {
            targets.push_back(message.targetModuleId);
            return targets;
        }

        // Broadcast to all registered modules
        if (message.targetModuleId.empty()) {
            std::lock_guard<std::mutex> lock(modulesMutex_);
            for (const auto &pair : registeredModules_) {
                if (pair.first != message.sourceModuleId) {  // Don't send to self
                    targets.push_back(pair.first);
                }
            }
            return targets;
        }

        // Route based on routing rules
        std::lock_guard<std::mutex> lock(routesMutex_);
        for (const auto &pair : routes_) {
            const MessageRoute &route = pair.second;
            if (route.isActive) {
                // Simple pattern matching (could be enhanced with regex)
                if (message.eventName.find(route.pattern) != std::string::npos ||
                    message.messageType.find(route.pattern) != std::string::npos) {
                    // Apply filter if present
                    if (!route.filter || route.filter(message)) {
                        targets.insert(targets.end(), route.targetModules.begin(), route.targetModules.end());
                    }
                }
            }
        }

        return targets;

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("InterModuleCommunicator", "Message routing failed: " + std::string(e.what()));
        return {};
    }
}

SCXML::Common::Result<void> InterModuleCommunicator::deliverMessage(const std::string &targetModuleId,
                                                                    const InterModuleMessage &message) {
    try {
        // Check if target module is registered
        std::shared_ptr<RuntimeContext> targetContext;
        {
            std::lock_guard<std::mutex> lock(modulesMutex_);
            auto it = registeredModules_.find(targetModuleId);
            if (it == registeredModules_.end()) {
                return SCXML::Common::Result<void>::error("Target module not registered: " + targetModuleId);
            }
            targetContext = it->second;
        }

        // Find appropriate message handler
        MessageHandler handler;
        {
            std::lock_guard<std::mutex> lock(handlersMutex_);
            auto moduleIt = messageHandlers_.find(targetModuleId);
            if (moduleIt != messageHandlers_.end()) {
                auto typeIt = moduleIt->second.find(message.messageType);
                if (typeIt != moduleIt->second.end()) {
                    handler = typeIt->second;
                }
            }
        }

        // Deliver message
        if (handler) {
            auto result = handler(message);
            if (!result.isSuccess()) {
                return SCXML::Common::Result<void>::error("Handler failed: " + result.getError());
            }
        } else {
            // Default delivery - add to target module's event queue
            if (targetContext && message.messageType == "event") {
                targetContext->getEventQueue().enqueueInternalEvent(message.eventName, message.messageData.dump());
            }
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Message delivery failed: " + std::string(e.what()));
    }
}

void InterModuleCommunicator::handleResponse(const InterModuleMessage &response) {
    try {
        std::lock_guard<std::mutex> lock(responseHandlersMutex_);

        auto it = responseHandlers_.find(response.correlationId);
        if (it != responseHandlers_.end()) {
            it->second(response);
            responseHandlers_.erase(it);
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("InterModuleCommunicator", "Response handling failed: " + std::string(e.what()));
    }
}

std::string InterModuleCommunicator::generateChannelId(const std::string &sourceModuleId,
                                                       const std::string &targetModuleId) {
    return sourceModuleId + "_to_" + targetModuleId;
}

std::string InterModuleCommunicator::generateCorrelationId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; ++i) {
        ss << dis(gen);
    }

    return ss.str();
}

void InterModuleCommunicator::updateStatistics(const std::string &statName, size_t increment) {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_[statName] += increment;
}

void InterModuleCommunicator::addToHistory(const InterModuleMessage &message) {
    std::lock_guard<std::mutex> lock(statisticsMutex_);

    messageHistory_.push_back(message);

    // Maintain maximum history size
    if (messageHistory_.size() > maxHistorySize_) {
        messageHistory_.erase(messageHistory_.begin(),
                              messageHistory_.begin() + (messageHistory_.size() - maxHistorySize_));
    }
}

SCXML::Common::Result<void> InterModuleCommunicator::validateMessage(const InterModuleMessage &message) {
    if (message.sourceModuleId.empty()) {
        return SCXML::Common::Result<void>::error("Source module ID cannot be empty");
    }

    if (message.messageType.empty()) {
        return SCXML::Common::Result<void>::error("Message type cannot be empty");
    }

    return SCXML::Common::Result<void>::success();
}

// InterModuleUtils Implementation
InterModuleMessage InterModuleUtils::createEventMessage(const std::string &sourceModuleId,
                                                        const std::string &targetModuleId, const std::string &eventName,
                                                        const nlohmann::json &eventData) {
    InterModuleMessage message(sourceModuleId, targetModuleId, "event", eventName, eventData);
    message.priority = 1;  // Events have normal priority
    return message;
}

InterModuleMessage InterModuleUtils::createDataMessage(const std::string &sourceModuleId,
                                                       const std::string &targetModuleId, const nlohmann::json &data) {
    InterModuleMessage message(sourceModuleId, targetModuleId, "data", "", data);
    message.priority = 0;  // Data messages have lower priority
    return message;
}

InterModuleMessage InterModuleUtils::createControlMessage(const std::string &sourceModuleId,
                                                          const std::string &targetModuleId,
                                                          const std::string &controlCommand,
                                                          const nlohmann::json &parameters) {
    InterModuleMessage message(sourceModuleId, targetModuleId, "control", controlCommand, parameters);
    message.priority = 2;  // Control messages have higher priority
    return message;
}

InterModuleMessage InterModuleUtils::createResponseMessage(const InterModuleMessage &originalMessage,
                                                           const nlohmann::json &responseData) {
    InterModuleMessage response(originalMessage.targetModuleId, originalMessage.sourceModuleId, "response", "",
                                responseData);
    response.correlationId = originalMessage.correlationId;
    response.priority = 1;
    return response;
}

std::string InterModuleUtils::extractModuleIdFromTarget(const std::string &target) {
    // Extract module ID from SCXML target format like "#module_id.state_id"
    std::regex moduleRegex(R"(#([^.]+)(?:\..*)??)");
    std::smatch match;

    if (std::regex_match(target, match, moduleRegex)) {
        return match[1].str();
    }

    return target;  // Return as-is if no pattern match
}

std::string InterModuleUtils::formatMessageForLog(const InterModuleMessage &message) {
    std::stringstream ss;
    ss << "[" << message.messageType << "] ";
    ss << message.sourceModuleId << " -> " << message.targetModuleId;

    if (!message.eventName.empty()) {
        ss << " (event: " << message.eventName << ")";
    }

    if (!message.correlationId.empty()) {
        ss << " (corr: " << message.correlationId.substr(0, 8) << "...)";
    }

    return ss.str();
}

bool InterModuleUtils::isValidModuleId(const std::string &moduleId) {
    if (moduleId.empty()) {
        return false;
    }

    // Simple validation: alphanumeric + underscore, must start with letter
    std::regex idRegex(R"([a-zA-Z][a-zA-Z0-9_]*)");
    return std::regex_match(moduleId, idRegex);
}

std::function<bool(const InterModuleMessage &)>
InterModuleUtils::createEventPatternFilter(const std::string &eventPattern) {
    return [eventPattern](const InterModuleMessage &message) -> bool {
        if (message.messageType != "event") {
            return false;
        }

        std::regex pattern(eventPattern);
        return std::regex_match(message.eventName, pattern);
    };
}

std::function<bool(const InterModuleMessage &)>
InterModuleUtils::createDataKeyFilter(const std::vector<std::string> &requiredKeys) {
    return [requiredKeys](const InterModuleMessage &message) -> bool {
        if (message.messageType != "data") {
            return false;
        }

        for (const auto &key : requiredKeys) {
            if (!message.messageData.contains(key)) {
                return false;
            }
        }

        return true;
    };
}

}  // namespace Runtime
}  // namespace SCXML