#include "runtime/InvokeProcessor.h"
#include "common/Logger.h"
#include "common/GracefulJoin.h"
#include "common/IdGenerator.h"
#include "model/IInvokeNode.h"
#include "runtime/Processor.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

namespace SCXML {

// ========================================
// InvokeProcessor Base Class
// ========================================

InvokeProcessor::InvokeProcessor(const std::string &processorType) : processorType_(processorType) {
    Logger::debug("InvokeProcessor: Creating processor: " + processorType);
}

std::shared_ptr<InvokeProcessor::SessionInfo> InvokeProcessor::getSessionInfo(const std::string &sessionId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(sessionId);
    return (it != sessions_.end()) ? it->second : nullptr;
}

std::unordered_map<std::string, std::shared_ptr<InvokeProcessor::SessionInfo>>
InvokeProcessor::getActiveSessions() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_;
}

bool InvokeProcessor::start() {
    if (running_) {
        Logger::warning("InvokeProcessor: Processor " + processorType_ + " already running");
        return true;
    }

    running_ = true;
    Logger::debug("InvokeProcessor: Started processor: " + processorType_);
    return true;
}

bool InvokeProcessor::stop() {
    if (!running_) {
        Logger::warning("InvokeProcessor: Processor " + processorType_ + " not running");
        return true;
    }

    running_ = false;

    // Terminate all active sessions
    std::vector<std::string> sessionIds;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        for (const auto &[sessionId, sessionInfo] : sessions_) {
            sessionIds.push_back(sessionId);
        }
    }

    for (const std::string &sessionId : sessionIds) {
        terminateInvoke(sessionId);
    }

    Logger::debug("InvokeProcessor: Stopped processor: " + processorType_);
    return true;
}

std::string InvokeProcessor::generateSessionId() {
    return IdGenerator::generateSessionId(processorType_);
}

std::shared_ptr<InvokeProcessor::SessionInfo>
InvokeProcessor::createSessionInfo(std::shared_ptr<IInvokeNode> invokeNode, SCXML::Runtime::RuntimeContext &context) {
    if (!invokeNode) {
        return nullptr;
    }

    std::string sessionId = generateSessionId();
    auto sessionInfo = std::make_shared<SessionInfo>(sessionId);

    sessionInfo->invokeId = invokeNode->getId();
    sessionInfo->src = invokeNode->getSrc();
    sessionInfo->type = invokeNode->getType();
    sessionInfo->content = invokeNode->getContent();
    sessionInfo->autoForward = invokeNode->isAutoForward();
    sessionInfo->startTime = std::chrono::steady_clock::now();

    // Convert parameters
    const auto &params = invokeNode->getParams();
    for (const auto &param : params) {
        std::string name = std::get<0>(param);
        std::string expr = std::get<1>(param);
        std::string location = std::get<2>(param);

        // Evaluate parameter value
        std::string value;
        if (!location.empty()) {
            // Get from data model location
            auto *dataModel = context.getDataModelEngine();
            if (dataModel) {
                auto result = dataModel->getValue(location);
                if (result.success) {
                    value = result.value;
                }
            }
        } else if (!expr.empty()) {
            // Evaluate expression
            auto evaluator = context.getExpressionEvaluator();
            if (evaluator) {
                auto result = evaluator->evaluate(expr);
                if (result.success) {
                    value = result.value;
                }
            }
        }

        sessionInfo->params[name] = value;
    }

    return sessionInfo;
}

void InvokeProcessor::updateSessionState(const std::string &sessionId, SessionState newState) {
    SessionState oldState = SessionState::INACTIVE;

    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            oldState = it->second->state;
            it->second->state = newState;
        }
    }

    Logger::debug("InvokeProcessor: Session " + sessionId + " state changed: " +
                  std::to_string(static_cast<int>(oldState)) + " -> " + std::to_string(static_cast<int>(newState)));

    // Trigger state change callback
    if (stateChangeCallback_) {
        stateChangeCallback_(sessionId, oldState, newState);
    }
}

void InvokeProcessor::fireEventToParent(const std::string &sessionId, SCXML::Events::EventPtr event) {
    if (!event) {
        return;
    }

    incrementEventsReceived();
    Logger::debug("InvokeProcessor: Firing event to parent from session: " + sessionId);

    if (eventCallback_) {
        eventCallback_(sessionId, event);
    }
}

// ========================================
// SCXMLInvokeProcessor Implementation
// ========================================

SCXMLInvokeProcessor::SCXMLInvokeProcessor() : InvokeProcessor("scxml") {}

bool SCXMLInvokeProcessor::canHandle(const std::string &type) const {
    return type == "scxml" || type == "http://www.w3.org/TR/scxml/";
}

std::string SCXMLInvokeProcessor::startInvoke(std::shared_ptr<IInvokeNode> invokeNode,
                                              SCXML::Runtime::RuntimeContext &context) {
    if (!invokeNode || !running_) {
        return "";
    }

    try {
        auto sessionInfo = createSessionInfo(invokeNode, context);
        if (!sessionInfo) {
            Logger::error("SCXMLInvokeProcessor: Failed to create session info");
            incrementFailedSessions();
            return "";
        }

        // Create SCXML session
        auto scxmlSession = std::make_unique<SCXMLSession>();

        // Store session info
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            sessions_[sessionInfo->sessionId] = sessionInfo;
            scxmlSessions_[sessionInfo->sessionId] = std::move(scxmlSession);
        }

        updateSessionState(sessionInfo->sessionId, SessionState::STARTING);

        // Start execution thread
        auto &storedSession = scxmlSessions_[sessionInfo->sessionId];
        storedSession->executionThread =
            std::thread([this, sessionInfo, invokeNode]() { runSCXMLSession(sessionInfo->sessionId, invokeNode); });

        updateSessionState(sessionInfo->sessionId, SessionState::ACTIVE);
        incrementSuccessfulSessions();

        Logger::debug("SCXMLInvokeProcessor: Started SCXML session: " + sessionInfo->sessionId);
        return sessionInfo->sessionId;

    } catch (const std::exception &e) {
        Logger::error("SCXMLInvokeProcessor: Exception starting session: " + std::string(e.what()));
        incrementFailedSessions();
        return "";
    }
}

bool SCXMLInvokeProcessor::sendEvent(const std::string &sessionId, SCXML::Events::EventPtr event) {
    if (!event) {
        return false;
    }

    auto sessionInfo = getSessionInfo(sessionId);
    if (!sessionInfo || sessionInfo->state != SessionState::ACTIVE) {
        Logger::warning("SCXMLInvokeProcessor: Cannot send event to inactive session: " + sessionId);
        return false;
    }

    try {
        // Get the SCXML session
        auto scxmlIt = scxmlSessions_.find(sessionId);
        if (scxmlIt == scxmlSessions_.end()) {
            Logger::error("SCXMLInvokeProcessor: SCXML session not found: " + sessionId);
            return false;
        }

        auto &scxmlSession = scxmlIt->second;
        if (!scxmlSession->childContext) {
            Logger::error("SCXMLInvokeProcessor: Child context not available for session: " + sessionId);
            return false;
        }

        // Send event to child context
        // Note: This would need proper event queue integration
        incrementEventsSent();
        Logger::debug("SCXMLInvokeProcessor: Sent event to session: " + sessionId);
        return true;

    } catch (const std::exception &e) {
        Logger::error("SCXMLInvokeProcessor: Exception sending event to session " + sessionId + ": " + e.what());
        return false;
    }
}

bool SCXMLInvokeProcessor::terminateInvoke(const std::string &sessionId) {
    try {
        updateSessionState(sessionId, SessionState::FINISHING);

        // Signal termination
        auto scxmlIt = scxmlSessions_.find(sessionId);
        if (scxmlIt != scxmlSessions_.end()) {
            auto &scxmlSession = scxmlIt->second;
            scxmlSession->shouldTerminate = true;

            // Wait for thread to finish
            if (scxmlSession->executionThread.joinable()) {
                SCXML::GracefulJoin::joinWithTimeout(scxmlSession->executionThread, 3, "SCXML_Execution");
            }
        }

        // Remove session
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            sessions_.erase(sessionId);
            scxmlSessions_.erase(sessionId);
        }

        updateSessionState(sessionId, SessionState::FINISHED);

        Logger::debug("SCXMLInvokeProcessor: Terminated session: " + sessionId);
        return true;

    } catch (const std::exception &e) {
        Logger::error("SCXMLInvokeProcessor: Exception terminating session " + sessionId + ": " + e.what());
        updateSessionState(sessionId, SessionState::ERROR);
        return false;
    }
}

void SCXMLInvokeProcessor::runSCXMLSession(const std::string &sessionId, std::shared_ptr<IInvokeNode> invokeNode) {
    Logger::debug("SCXMLInvokeProcessor: Running SCXML session: " + sessionId);

    try {
        auto scxmlIt = scxmlSessions_.find(sessionId);
        if (scxmlIt == scxmlSessions_.end()) {
            Logger::error("SCXMLInvokeProcessor: Session not found during execution: " + sessionId);
            updateSessionState(sessionId, SessionState::ERROR);
            return;
        }

        auto &scxmlSession = scxmlIt->second;

        // Create child runtime context
        scxmlSession->childContext = std::make_unique<RuntimeContext>();

        // Initialize child context with invoke parameters
        // This is a simplified implementation - in practice would need full SCXML parsing

        // Main execution loop
        while (!scxmlSession->shouldTerminate && running_) {
            // Process SCXML execution
            // This would involve actual SCXML interpretation

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        updateSessionState(sessionId, SessionState::FINISHED);
        Logger::debug("SCXMLInvokeProcessor: SCXML session completed: " + sessionId);

    } catch (const std::exception &e) {
        Logger::error("SCXMLInvokeProcessor: Exception in session execution " + sessionId + ": " + e.what());
        updateSessionState(sessionId, SessionState::ERROR);
    }
}

// ========================================
// HTTPInvokeProcessor Implementation
// ========================================

HTTPInvokeProcessor::HTTPInvokeProcessor() : InvokeProcessor("http") {}

bool HTTPInvokeProcessor::canHandle(const std::string &type) const {
    return type == "http" || type == "https" || type.find("http://") == 0 || type.find("https://") == 0;
}

std::string HTTPInvokeProcessor::startInvoke(std::shared_ptr<IInvokeNode> invokeNode,
                                             SCXML::Runtime::RuntimeContext &context) {
    if (!invokeNode || !running_) {
        return "";
    }

    try {
        auto sessionInfo = createSessionInfo(invokeNode, context);
        if (!sessionInfo) {
            Logger::error("HTTPInvokeProcessor: Failed to create session info");
            incrementFailedSessions();
            return "";
        }

        // Create HTTP session
        auto httpSession = std::make_unique<HTTPSession>();
        httpSession->url = invokeNode->getSrc();
        httpSession->method = "POST";  // Default method
        httpSession->headers["Content-Type"] = "application/json";

        // Store session info
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            sessions_[sessionInfo->sessionId] = sessionInfo;
            httpSessions_[sessionInfo->sessionId] = std::move(httpSession);
        }

        updateSessionState(sessionInfo->sessionId, SessionState::STARTING);

        // Start HTTP request thread
        auto &storedSession = httpSessions_[sessionInfo->sessionId];
        storedSession->requestThread =
            std::thread([this, sessionInfo, invokeNode]() { executeHTTPRequest(sessionInfo->sessionId, invokeNode); });

        updateSessionState(sessionInfo->sessionId, SessionState::ACTIVE);
        incrementSuccessfulSessions();

        Logger::debug("HTTPInvokeProcessor: Started HTTP session: " + sessionInfo->sessionId);
        return sessionInfo->sessionId;

    } catch (const std::exception &e) {
        Logger::error("HTTPInvokeProcessor: Exception starting session: " + std::string(e.what()));
        incrementFailedSessions();
        return "";
    }
}

bool HTTPInvokeProcessor::sendEvent(const std::string &sessionId, SCXML::Events::EventPtr event) {
    if (!event) {
        return false;
    }

    auto sessionInfo = getSessionInfo(sessionId);
    if (!sessionInfo || sessionInfo->state != SessionState::ACTIVE) {
        Logger::warning("HTTPInvokeProcessor: Cannot send event to inactive session: " + sessionId);
        return false;
    }

    try {
        // Convert event to JSON and send via HTTP
        std::string jsonEvent = eventToJSON(event);

        incrementEventsSent();
        Logger::debug("HTTPInvokeProcessor: Sent event to session: " + sessionId);
        return true;

    } catch (const std::exception &e) {
        Logger::error("HTTPInvokeProcessor: Exception sending event to session " + sessionId + ": " + e.what());
        return false;
    }
}

bool HTTPInvokeProcessor::terminateInvoke(const std::string &sessionId) {
    try {
        updateSessionState(sessionId, SessionState::FINISHING);

        // Signal termination
        auto httpIt = httpSessions_.find(sessionId);
        if (httpIt != httpSessions_.end()) {
            auto &httpSession = httpIt->second;
            httpSession->shouldTerminate = true;

            // Wait for thread to finish
            if (httpSession->requestThread.joinable()) {
                SCXML::GracefulJoin::joinWithTimeout(httpSession->requestThread, 3, "HTTP_Request");
            }
        }

        // Remove session
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            sessions_.erase(sessionId);
            httpSessions_.erase(sessionId);
        }

        updateSessionState(sessionId, SessionState::FINISHED);

        Logger::debug("HTTPInvokeProcessor: Terminated session: " + sessionId);
        return true;

    } catch (const std::exception &e) {
        Logger::error("HTTPInvokeProcessor: Exception terminating session " + sessionId + ": " + e.what());
        updateSessionState(sessionId, SessionState::ERROR);
        return false;
    }
}

void HTTPInvokeProcessor::executeHTTPRequest(const std::string &sessionId, std::shared_ptr<IInvokeNode> invokeNode) {
    Logger::debug("HTTPInvokeProcessor: Executing HTTP request for session: " + sessionId);

    try {
        auto httpIt = httpSessions_.find(sessionId);
        if (httpIt == httpSessions_.end()) {
            Logger::error("HTTPInvokeProcessor: Session not found during execution: " + sessionId);
            updateSessionState(sessionId, SessionState::ERROR);
            return;
        }

        auto &httpSession = httpIt->second;

        // Simplified HTTP request execution
        // In practice, this would use a proper HTTP client library
        Logger::debug("HTTPInvokeProcessor: Making HTTP request to: " + httpSession->url);

        // Simulate HTTP request delay
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        if (httpSession->shouldTerminate) {
            updateSessionState(sessionId, SessionState::FINISHED);
            return;
        }

        // Simulate successful response
        // Would fire completion event to parent
        updateSessionState(sessionId, SessionState::FINISHED);
        Logger::debug("HTTPInvokeProcessor: HTTP request completed for session: " + sessionId);

    } catch (const std::exception &e) {
        Logger::error("HTTPInvokeProcessor: Exception in HTTP execution " + sessionId + ": " + e.what());
        updateSessionState(sessionId, SessionState::ERROR);
    }
}

std::string HTTPInvokeProcessor::eventToJSON(SCXML::Events::EventPtr event) const {
    if (!event) {
        return "{}";
    }

    // Basic JSON serialization of event
    // In practice, this would be more sophisticated
    std::stringstream json;
    json << "{"
         << "\"name\":\"" << "event_name" << "\","
         << "\"data\":{}"
         << "}";
    return json.str();
}

// ========================================
// ProcessInvokeProcessor Implementation
// ========================================

ProcessInvokeProcessor::ProcessInvokeProcessor() : InvokeProcessor("process") {}

bool ProcessInvokeProcessor::canHandle(const std::string &type) const {
    return type == "process" || type == "exec";
}

std::string ProcessInvokeProcessor::startInvoke(std::shared_ptr<IInvokeNode> invokeNode,
                                                SCXML::Runtime::RuntimeContext &context) {
    if (!invokeNode || !running_) {
        return "";
    }

    try {
        auto sessionInfo = createSessionInfo(invokeNode, context);
        if (!sessionInfo) {
            Logger::error("ProcessInvokeProcessor: Failed to create session info");
            incrementFailedSessions();
            return "";
        }

        // Create process session
        auto processSession = std::make_unique<ProcessSession>();

        // Parse command from src attribute
        std::string src = invokeNode->getSrc();
        if (!src.empty()) {
            // Simple command parsing - split by spaces
            std::istringstream iss(src);
            std::string token;
            bool isFirst = true;
            while (std::getline(iss, token, ' ')) {
                if (isFirst) {
                    processSession->command = token;
                    isFirst = false;
                } else {
                    processSession->args.push_back(token);
                }
            }
        }

        // Store session info
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            sessions_[sessionInfo->sessionId] = sessionInfo;
            processSessions_[sessionInfo->sessionId] = std::move(processSession);
        }

        updateSessionState(sessionInfo->sessionId, SessionState::STARTING);

        // Start process execution thread
        auto &storedSession = processSessions_[sessionInfo->sessionId];
        storedSession->processThread =
            std::thread([this, sessionInfo, invokeNode]() { executeProcess(sessionInfo->sessionId, invokeNode); });

        updateSessionState(sessionInfo->sessionId, SessionState::ACTIVE);
        incrementSuccessfulSessions();

        Logger::debug("ProcessInvokeProcessor: Started process session: " + sessionInfo->sessionId);
        return sessionInfo->sessionId;

    } catch (const std::exception &e) {
        Logger::error("ProcessInvokeProcessor: Exception starting session: " + std::string(e.what()));
        incrementFailedSessions();
        return "";
    }
}

bool ProcessInvokeProcessor::sendEvent(const std::string &sessionId, SCXML::Events::EventPtr event) {
    if (!event) {
        return false;
    }

    auto sessionInfo = getSessionInfo(sessionId);
    if (!sessionInfo || sessionInfo->state != SessionState::ACTIVE) {
        Logger::warning("ProcessInvokeProcessor: Cannot send event to inactive session: " + sessionId);
        return false;
    }

    try {
        // For process invokes, events could be sent as stdin input
        incrementEventsSent();
        Logger::debug("ProcessInvokeProcessor: Sent event to session: " + sessionId);
        return true;

    } catch (const std::exception &e) {
        Logger::error("ProcessInvokeProcessor: Exception sending event to session " + sessionId + ": " + e.what());
        return false;
    }
}

bool ProcessInvokeProcessor::terminateInvoke(const std::string &sessionId) {
    try {
        updateSessionState(sessionId, SessionState::FINISHING);

        // Signal termination
        auto processIt = processSessions_.find(sessionId);
        if (processIt != processSessions_.end()) {
            auto &processSession = processIt->second;
            processSession->shouldTerminate = true;

            // Terminate process if running
            if (processSession->processId > 0) {
                // Would use system calls to terminate process
                Logger::debug("ProcessInvokeProcessor: Terminating process ID: " +
                              std::to_string(processSession->processId));
            }

            // Wait for thread to finish
            if (processSession->processThread.joinable()) {
                SCXML::GracefulJoin::joinWithTimeout(processSession->processThread, 3, "Process_Session");
            }
        }

        // Remove session
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            sessions_.erase(sessionId);
            processSessions_.erase(sessionId);
        }

        updateSessionState(sessionId, SessionState::FINISHED);

        Logger::debug("ProcessInvokeProcessor: Terminated session: " + sessionId);
        return true;

    } catch (const std::exception &e) {
        Logger::error("ProcessInvokeProcessor: Exception terminating session " + sessionId + ": " + e.what());
        updateSessionState(sessionId, SessionState::ERROR);
        return false;
    }
}

void ProcessInvokeProcessor::executeProcess(const std::string &sessionId, std::shared_ptr<IInvokeNode> invokeNode) {
    Logger::debug("ProcessInvokeProcessor: Executing process for session: " + sessionId);

    try {
        auto processIt = processSessions_.find(sessionId);
        if (processIt == processSessions_.end()) {
            Logger::error("ProcessInvokeProcessor: Session not found during execution: " + sessionId);
            updateSessionState(sessionId, SessionState::ERROR);
            return;
        }

        auto &processSession = processIt->second;

        // Execute external process
        Logger::debug("ProcessInvokeProcessor: Executing command: " + processSession->command);

        // Simplified process execution
        // In practice, this would use system() or fork()/exec() calls

        // Simulate process execution
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        if (processSession->shouldTerminate) {
            updateSessionState(sessionId, SessionState::FINISHED);
            return;
        }

        // Simulate successful completion
        updateSessionState(sessionId, SessionState::FINISHED);
        Logger::debug("ProcessInvokeProcessor: Process completed for session: " + sessionId);

    } catch (const std::exception &e) {
        Logger::error("ProcessInvokeProcessor: Exception in process execution " + sessionId + ": " + e.what());
        updateSessionState(sessionId, SessionState::ERROR);
    }
}

}  // namespace SCXML