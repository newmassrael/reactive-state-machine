#include "../../include/runtime/InternalEventProcessor.h"
#include "../common/Logger.h"
#include "../../include/common/GracefulJoin.h"
#include "../../include/common/IdGenerator.h"
#include "../../include/runtime/Processor.h"
#include <algorithm>
#include <future>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

using namespace SCXML::Runtime;
using namespace SCXML::Events;

// Include Processor definition
#include "runtime/Processor.h"

InternalEventProcessor::InternalEventProcessor()
    : IOProcessor(ProcessorType::SCXML, "http://www.w3.org/TR/scxml/#SCXMLEventProcessor") {
    // Generate default session ID
    config_.sessionId = generateSessionId();
    config_.instanceName = "SCXMLInstance_" + config_.sessionId;
}

InternalEventProcessor::InternalEventProcessor(const Config &config)
    : IOProcessor(ProcessorType::SCXML, "http://www.w3.org/TR/scxml/#SCXMLEventProcessor"), config_(config) {
    // Ensure we have a session ID
    if (config_.sessionId.empty()) {
        config_.sessionId = generateSessionId();
    }

    if (config_.instanceName.empty()) {
        config_.instanceName = "SCXMLInstance_" + config_.sessionId;
    }
}

InternalEventProcessor::~InternalEventProcessor() {
    stop();
}

bool InternalEventProcessor::start() {
    if (running_) {
        SCXML::Common::Logger::info("InternalEventProcessor: Already running");
        return true;
    }

    SCXML::Common::Logger::info("InternalEventProcessor: Starting SCXML I/O Processor (Session: " + config_.sessionId + ")");

    shutdownRequested_ = false;

    // Start background threads
    deliveryThread_ = std::thread(&InternalEventProcessor::processDeliveryQueue, this);

    if (config_.enableSessionDiscovery) {
        heartbeatThread_ = std::thread(&InternalEventProcessor::processSessionHeartbeats, this);
    }

    running_ = true;
    SCXML::Common::Logger::info("InternalEventProcessor: Operation completed successfully");

    return true;
}

bool InternalEventProcessor::stop() {
    if (!running_) {
        return true;
    }

    SCXML::Common::Logger::info("InternalEventProcessor: Stopping SCXML I/O Processor");

    std::cerr << "DEBUG: About to set shutdown flags" << std::endl;
    shutdownRequested_ = true;
    running_ = false;
    std::cerr << "DEBUG: Shutdown flags set" << std::endl;

    // Stop background threads
    SCXML::Common::Logger::info("InternalEventProcessor: Notifying all threads to stop");
    deliveryQueueCondition_.notify_all();  // 배달 스레드 깨움
    shutdownCondition_.notify_all();       // 하트비트 스레드 즉시 깨움!

    SCXML::Common::Logger::info("InternalEventProcessor: Joining delivery thread");
    if (deliveryThread_.joinable()) {
        SCXML::Common::GracefulJoin::joinWithTimeout(deliveryThread_, 3, "SCXML_DeliveryThread");
    }
    SCXML::Common::Logger::info("InternalEventProcessor: Delivery thread joined");

    SCXML::Common::Logger::info("InternalEventProcessor: Joining heartbeat thread");
    if (heartbeatThread_.joinable()) {
        SCXML::Common::GracefulJoin::joinWithTimeout(heartbeatThread_, 3, "SCXML_HeartbeatThread");
    }
    SCXML::Common::Logger::info("InternalEventProcessor: Heartbeat thread joined");

    // Clear all data structures
    SCXML::Common::Logger::info("InternalEventProcessor: Clearing data structures");
    {
        std::lock_guard<std::mutex> lock(runtimesMutex_);
        registeredRuntimes_.clear();
        sessions_.clear();
        nameToSessionId_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(deliveryQueueMutex_);
        while (!deliveryQueue_.empty()) {
            deliveryQueue_.pop();
        }
        pendingDeliveries_.clear();
    }

    SCXML::Common::Logger::info("InternalEventProcessor: SCXML I/O Processor stopped successfully");
    return true;
}

bool InternalEventProcessor::send(const SendParameters &params) {
    if (!running_) {
        SCXML::Common::Logger::error("InternalEventProcessor: SCXML processor not running");
        return false;
    }

    if (params.target.empty()) {
        SCXML::Common::Logger::error("InternalEventProcessor: Empty target in send parameters");
        return false;
    }

    // Handle delayed sends
    if (params.delayMs > 0) {
        PendingDelivery delivery;
        delivery.deliveryId = generateDeliveryId();
        delivery.targetSessionId = resolveTarget(params.target);
        delivery.event = createSCXMLEvent(params);
        delivery.originalParams = params;
        delivery.scheduledTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(params.delayMs);
        delivery.confirmationRequired = config_.enableEventConfirmation;

        if (delivery.targetSessionId.empty()) {
            SCXML::Common::Logger::error("InternalEventProcessor: Failed to create event for delivery: " + params.sendId);
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(deliveryQueueMutex_);
            pendingDeliveries_[params.sendId] = delivery;
            deliveryQueue_.emplace(std::move(delivery));
        }

        deliveryQueueCondition_.notify_one();
        SCXML::Common::Logger::info("InternalEventProcessor: Scheduled delayed SCXML event: " + params.sendId);
        return true;
    }

    // Execute immediate SCXML delivery
    std::thread deliveryThread(&InternalEventProcessor::executeSCXMLDelivery, this, params);
    deliveryThread.detach();

    return true;
}

bool InternalEventProcessor::cancelSend(const std::string &sendId) {
    if (sendId.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(deliveryQueueMutex_);

    auto it = pendingDeliveries_.find(sendId);
    if (it != pendingDeliveries_.end()) {
        it->second.cancelled = true;
        SCXML::Common::Logger::info("InternalEventProcessor: Cancelled pending delivery: " + sendId);
        return true;
    }

    return false;
}

bool InternalEventProcessor::canHandle(const std::string &typeURI) const {
    return typeURI == "http://www.w3.org/TR/scxml/#SCXMLEventProcessor" || typeURI == "scxml" ||
           typeURI.find("#_scxml_") == 0;  // Session-specific targets
}

bool InternalEventProcessor::registerRuntime(std::shared_ptr<Processor> runtime) {
    if (!runtime) {
        SCXML::Common::Logger::error("InternalEventProcessor: Cannot register null runtime");
        return false;
    }

    std::lock_guard<std::mutex> lock(runtimesMutex_);

    std::string sessionId = runtime->getSessionId();
    std::string instanceName = sessionId;  // Use sessionId as instanceName

    if (sessionId.empty()) {
        SCXML::Common::Logger::error("InternalEventProcessor: Runtime has empty session ID");
        return false;
    }

    // Check if already registered
    if (registeredRuntimes_.find(sessionId) != registeredRuntimes_.end()) {
        SCXML::Common::Logger::info("InternalEventProcessor: Runtime already registered: " + sessionId);
        return false;
    }

    // Register the runtime
    registeredRuntimes_[sessionId] = runtime;

    // Create session info
    auto sessionInfo = std::make_shared<SessionInfo>();
    sessionInfo->sessionId = sessionId;
    sessionInfo->instanceName = instanceName;
    sessionInfo->targetURI = "#_scxml_" + sessionId;
    sessionInfo->lastHeartbeat = std::chrono::steady_clock::now();
    sessionInfo->isActive = true;

    sessions_[sessionId] = sessionInfo;

    // Add name-to-session mapping if name is provided
    if (!instanceName.empty() && instanceName != sessionId) {
        nameToSessionId_[instanceName] = sessionId;
    }

    SCXML::Common::Logger::info("InternalEventProcessor: Registered SCXML runtime: " + sessionId + " (" + instanceName + ")");
    return true;
}

bool InternalEventProcessor::unregisterRuntime(const std::string &sessionId) {
    if (sessionId.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(runtimesMutex_);

    auto runtimeIt = registeredRuntimes_.find(sessionId);
    if (runtimeIt == registeredRuntimes_.end()) {
        SCXML::Common::Logger::info("InternalEventProcessor: Runtime not found for unregistration: " + sessionId);
        return false;
    }

    // Remove from all maps
    registeredRuntimes_.erase(runtimeIt);

    auto sessionIt = sessions_.find(sessionId);
    if (sessionIt != sessions_.end()) {
        // Remove name mapping if it exists
        const std::string &instanceName = sessionIt->second->instanceName;
        if (!instanceName.empty()) {
            nameToSessionId_.erase(instanceName);
        }

        sessions_.erase(sessionIt);
    }

    SCXML::Common::Logger::info("InternalEventProcessor: Runtime unregistered successfully: " + sessionId);
    return true;
}

std::vector<InternalEventProcessor::SessionInfo> InternalEventProcessor::getActiveSessions() const {
    std::lock_guard<std::mutex> lock(runtimesMutex_);

    std::vector<SessionInfo> activeSessions;
    activeSessions.reserve(sessions_.size());

    for (const auto &pair : sessions_) {
        if (pair.second->isActive && isSessionAlive(*pair.second)) {
            activeSessions.push_back(*pair.second);
        }
    }

    return activeSessions;
}

std::shared_ptr<InternalEventProcessor::SessionInfo>
InternalEventProcessor::getSession(const std::string &sessionId) const {
    std::lock_guard<std::mutex> lock(runtimesMutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end() && it->second->isActive) {
        return it->second;
    }

    return nullptr;
}

bool InternalEventProcessor::updateConfig(const Config &config) {
    if (running_) {
        SCXML::Common::Logger::info("InternalEventProcessor: Operation completed successfully");
        return false;
    }

    config_ = config;
    SCXML::Common::Logger::info("InternalEventProcessor: Operation completed successfully");
    return true;
}

void InternalEventProcessor::executeSCXMLDelivery(const SendParameters &params) {
    if (shutdownRequested_) {
        return;
    }

    SCXML::Common::Logger::info("InternalEventProcessor: Operation completed successfully");
    incrementActiveRequests();

    try {
        std::string targetSessionId = resolveTarget(params.target);
        if (targetSessionId.empty()) {
            SCXML::Common::Logger::info("InternalEventProcessor: Operation completed successfully");
            auto errorEvent =
                createErrorEvent(params.sendId, "scxml.target.invalid", "Cannot resolve target: " + params.target);
            fireIncomingEvent(errorEvent);
            return;
        }

        auto event = createSCXMLEvent(params);
        std::string deliveryId = deliverEventToTarget(targetSessionId, event, config_.enableEventConfirmation);

        if (!deliveryId.empty()) {
            incrementEventsSent();
            SCXML::Common::Logger::info("InternalEventProcessor: Operation completed successfully");
        } else {
            incrementSendFailures();
            auto errorEvent = createErrorEvent(params.sendId, "scxml.delivery.failed",
                                               "Failed to deliver event to: " + targetSessionId);
            fireIncomingEvent(errorEvent);
        }

    } catch (const std::exception &e) {
        incrementSendFailures();
        auto errorEvent =
            createErrorEvent(params.sendId, "scxml.exception", std::string("SCXML delivery exception: ") + e.what());
        fireIncomingEvent(errorEvent);
    }

    decrementActiveRequests();
}

std::string InternalEventProcessor::deliverEventToTarget(const std::string &targetSessionId,
                                                         const Events::EventPtr &event, bool confirmationRequired) {
    std::lock_guard<std::mutex> lock(runtimesMutex_);

    auto runtimeIt = registeredRuntimes_.find(targetSessionId);
    if (runtimeIt == registeredRuntimes_.end()) {
        SCXML::Common::Logger::info("InternalEventProcessor: Operation completed successfully");
        return "";
    }

    auto runtime = runtimeIt->second.lock();
    if (!runtime || !runtime->isRunning()) {
        SCXML::Common::Logger::info("InternalEventProcessor: Operation completed successfully");
        return "";
    }

    // Deliver the event
    bool delivered = runtime->sendEvent(event->getName(), event->getDataAsString());

    if (delivered) {
        // Update session statistics
        auto sessionIt = sessions_.find(targetSessionId);
        if (sessionIt != sessions_.end()) {
            sessionIt->second->eventsReceived++;
            updateSessionHeartbeat(targetSessionId);
        }

        std::string deliveryId = generateDeliveryId();

        if (confirmationRequired) {
            // Record delivery confirmation
            DeliveryConfirmation confirmation;
            confirmation.deliveryId = deliveryId;
            confirmation.delivered = true;
            confirmation.timestamp = std::chrono::steady_clock::now();

            std::lock_guard<std::mutex> confirmLock(confirmationsMutex_);
            deliveryConfirmations_[deliveryId] = confirmation;
        }

        return deliveryId;
    }

    return "";
}

void InternalEventProcessor::performHeartbeatMaintenance() {
    // 세션 건강 체크 및 데드 세션 정리
    std::lock_guard<std::mutex> runtimesLock(runtimesMutex_);
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (!isSessionAlive(*it->second)) {
            SCXML::Common::Logger::info("InternalEventProcessor: Cleaning up inactive session: " + it->first);
            it->second->isActive = false;

            // Clean up name mapping
            const std::string &instanceName = it->second->instanceName;
            if (!instanceName.empty()) {
                auto nameIt = nameToSessionId_.find(instanceName);
                if (nameIt != nameToSessionId_.end() && nameIt->second == it->first) {
                    nameToSessionId_.erase(nameIt);
                }
            }

            // Remove from registered runtimes
            registeredRuntimes_.erase(it->first);

            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

void InternalEventProcessor::processDeliveryQueue() {
    while (!shutdownRequested_) {
        std::unique_lock<std::mutex> lock(deliveryQueueMutex_);

        // Wait for deliveries or shutdown
        deliveryQueueCondition_.wait(lock, [this] { return !deliveryQueue_.empty() || shutdownRequested_; });

        if (shutdownRequested_) {
            break;
        }

        auto now = std::chrono::steady_clock::now();

        while (!deliveryQueue_.empty()) {
            auto &delivery = deliveryQueue_.front();

            if (delivery.cancelled) {
                deliveryQueue_.pop();
                continue;
            }

            if (delivery.scheduledTime <= now) {
                // Execute the delivery
                SendParameters params = delivery.originalParams;
                deliveryQueue_.pop();

                lock.unlock();
                executeSCXMLDelivery(params);
                lock.lock();
            } else {
                // Wait until next scheduled delivery
                break;
            }
        }
    }
}

void InternalEventProcessor::processSessionHeartbeats() {
    SCXML::Common::Logger::info("InternalEventProcessor: Heartbeat thread started with proper condition variable");

    // 정확한 condition variable 패턴
    std::unique_lock<std::mutex> lock(shutdownMutex_);

    while (!shutdownRequested_) {
        // wait_for with predicate - 공식 문서 권장 방식
        // 1초마다 하트비트 체크, 하지만 shutdown 시 즉시 깨어남
        auto result = shutdownCondition_.wait_for(lock,
                                                  std::chrono::milliseconds(1000),  // 1초 하트비트 간격
                                                  [this]() { return shutdownRequested_.load(); });

        // result가 true면 predicate가 true가 되어서 깨어난 것 (shutdown)
        // result가 false면 timeout으로 깨어난 것 (정상 하트비트)
        if (result) {
            // Shutdown 요청으로 깨어남 - 즉시 종료!
            SCXML::Common::Logger::info("InternalEventProcessor: Received shutdown signal via condition variable");
            break;
        }

        // Timeout으로 깨어남 - 정상 하트비트 수행
        // 이 시점에서는 lock이 이미 획득된 상태이므로 unlock하고 작업
        lock.unlock();

        SCXML::Common::Logger::info("InternalEventProcessor: Performing scheduled heartbeat check");
        performHeartbeatMaintenance();

        // 다시 lock 획득하여 다음 루프 준비
        lock.lock();
    }

    SCXML::Common::Logger::info("InternalEventProcessor: Heartbeat thread finished with condition variable");
}

void InternalEventProcessor::handleDeliveryConfirmation(const DeliveryConfirmation &confirmation) {
    std::lock_guard<std::mutex> lock(confirmationsMutex_);
    deliveryConfirmations_[confirmation.deliveryId] = confirmation;

    SCXML::Common::Logger::info("InternalEventProcessor: Received delivery confirmation: " + confirmation.deliveryId +
                 " (delivered: " + (confirmation.delivered ? "true" : "false") + ")");
}

std::string InternalEventProcessor::resolveTarget(const std::string &target) const {
    std::lock_guard<std::mutex> lock(runtimesMutex_);

    // Direct session ID format: #_scxml_sessionId
    if (target.find("#_scxml_") == 0) {
        std::string sessionId = target.substr(7);  // Remove "#_scxml_"
        if (sessions_.find(sessionId) != sessions_.end()) {
            return sessionId;
        }
    }

    // Instance name lookup
    auto nameIt = nameToSessionId_.find(target);
    if (nameIt != nameToSessionId_.end()) {
        return nameIt->second;
    }

    // Direct session ID
    if (sessions_.find(target) != sessions_.end()) {
        return target;
    }

    // Target not found
    return "";
}

EventPtr InternalEventProcessor::createSCXMLEvent(const SendParameters &params) const {
    auto event = std::make_shared<Event>(params.event);

    // Create JSON string for event data
    std::string jsonData = "{";

    if (!params.content.empty()) {
        jsonData += "\"content\":\"" + params.content + "\",";
    }

    // Add parameters as event data
    for (const auto &param : params.params) {
        jsonData += "\"" + param.first + "\":\"" + param.second + "\",";
    }

    // Add SCXML-specific metadata
    jsonData += "\"sendid\":\"" + params.sendId + "\",";
    jsonData += "\"origin\":\"" + config_.sessionId + "\",";
    jsonData += "\"originname\":\"" + config_.instanceName + "\",";
    jsonData += "\"type\":\"scxml.event\"";
    jsonData += "}";

    event->setData(jsonData);
    return event;
}

std::string InternalEventProcessor::generateDeliveryId() const {
    std::lock_guard<std::mutex> lock(idMutex_);
    return "scxml_delivery_" + std::to_string(nextDeliveryId_++);
}

std::string InternalEventProcessor::generateSessionId() const {
    return IdGenerator::generateSessionId("scxml_session");
}

void InternalEventProcessor::updateSessionHeartbeat(const std::string &sessionId) {
    auto sessionIt = sessions_.find(sessionId);
    if (sessionIt != sessions_.end()) {
        sessionIt->second->lastHeartbeat = std::chrono::steady_clock::now();
    }
}

bool InternalEventProcessor::isSessionAlive(const SessionInfo &session) const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - session.lastHeartbeat).count();

    return elapsed < (config_.sessionHeartbeatMs * 3);  // Allow 3x heartbeat interval
}
