#pragma once

#include "common/GracefulJoin.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

namespace SCXML {

// Forward declarations
namespace Core {
// Moved to Model namespace: class IInvokeNode;
}

namespace Runtime {
class RuntimeContext;
}

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}

/**
 * @brief Base interface for SCXML invoke processors
 *
 * Invoke processors handle the invocation of external services and systems
 * as specified by the W3C SCXML specification. Each processor type handles
 * a specific kind of external service (SCXML, HTTP, etc.).
 */
class InvokeProcessor {
public:
    /**
     * @brief Invoke session state
     */
    enum class SessionState {
        INACTIVE,   // Not started
        STARTING,   // Being initialized
        ACTIVE,     // Running normally
        FINISHING,  // Termination requested
        FINISHED,   // Terminated successfully
        ERROR       // Error occurred
    };

    /**
     * @brief Invoke session information
     */
    struct SessionInfo {
        std::string sessionId;
        std::string invokeId;
        std::string parentSessionId;
        SessionState state = SessionState::INACTIVE;
        std::string src;
        std::string type;
        std::unordered_map<std::string, std::string> params;
        std::string content;
        bool autoForward = false;
        std::chrono::steady_clock::time_point startTime;
        std::string errorMessage;

        SessionInfo(const std::string &id) : sessionId(id) {}
    };

    /**
     * @brief Event callback for parent-child communication
     */
    using EventCallback = std::function<void(const std::string &sessionId, Events::EventPtr event)>;

    /**
     * @brief Session state change callback
     */
    using StateChangeCallback =
        std::function<void(const std::string &sessionId, SessionState oldState, SessionState newState)>;

public:
    /**
     * @brief Constructor
     * @param processorType Type identifier for this processor
     */
    explicit InvokeProcessor(const std::string &processorType);

    /**
     * @brief Virtual destructor
     */
    virtual ~InvokeProcessor() = default;

    /**
     * @brief Get processor type identifier
     * @return Processor type string
     */
    const std::string &getProcessorType() const {
        return processorType_;
    }

    /**
     * @brief Check if this processor can handle the given type
     * @param type Invoke type to check
     * @return true if this processor can handle the type
     */
    virtual bool canHandle(const std::string &type) const = 0;

    /**
     * @brief Start an invoke session
     * @param invokeNode Invoke node containing parameters
     * @param context Runtime context
     * @return Session ID if successful, empty string if failed
     */
    virtual std::string startInvoke(std::shared_ptr<Model::IInvokeNode> invokeNode,
                                    Runtime::RuntimeContext &context) = 0;

    /**
     * @brief Send event to an active session
     * @param sessionId Target session ID
     * @param event Event to send
     * @return true if event was sent successfully
     */
    virtual bool sendEvent(const std::string &sessionId, Events::EventPtr event) = 0;

    /**
     * @brief Terminate an active session
     * @param sessionId Session to terminate
     * @return true if termination was initiated successfully
     */
    virtual bool terminateInvoke(const std::string &sessionId) = 0;

    /**
     * @brief Get session information
     * @param sessionId Session to query
     * @return Session info if found, nullptr otherwise
     */
    virtual std::shared_ptr<SessionInfo> getSessionInfo(const std::string &sessionId) const;

    /**
     * @brief Get all active sessions
     * @return Map of session ID to session info
     */
    virtual std::unordered_map<std::string, std::shared_ptr<SessionInfo>> getActiveSessions() const;

    /**
     * @brief Set event callback for parent communication
     * @param callback Function to call when events are received from sessions
     */
    void setEventCallback(EventCallback callback) {
        eventCallback_ = callback;
    }

    /**
     * @brief Set state change callback
     * @param callback Function to call when session states change
     */
    void setStateChangeCallback(StateChangeCallback callback) {
        stateChangeCallback_ = callback;
    }

    /**
     * @brief Check if processor is running
     * @return true if processor is active
     */
    bool isRunning() const {
        return running_;
    }

    /**
     * @brief Start processor (enable session creation)
     * @return true if started successfully
     */
    virtual bool start();

    /**
     * @brief Stop processor (terminate all sessions)
     * @return true if stopped successfully
     */
    virtual bool stop();

    /**
     * @brief Get processor statistics
     */
    struct Statistics {
        size_t totalSessions = 0;
        size_t activeSessions = 0;
        size_t successfulSessions = 0;
        size_t failedSessions = 0;
        size_t eventsSent = 0;
        size_t eventsReceived = 0;
    };

    /**
     * @brief Get processor statistics
     * @return Current statistics
     */
    const Statistics &getStatistics() const {
        return statistics_;
    }

protected:
    /**
     * @brief Generate unique session ID
     * @return New session ID
     */
    std::string generateSessionId();

    /**
     * @brief Create session info from invoke node
     * @param invokeNode Invoke node parameters
     * @param context Runtime context
     * @return Created session info
     */
    std::shared_ptr<SessionInfo> createSessionInfo(std::shared_ptr<Model::IInvokeNode> invokeNode,
                                                   Runtime::RuntimeContext &context);

    /**
     * @brief Update session state
     * @param sessionId Session to update
     * @param newState New state
     */
    void updateSessionState(const std::string &sessionId, SessionState newState);

    /**
     * @brief Fire event to parent session
     * @param sessionId Source session
     * @param event Event to send
     */
    void fireEventToParent(const std::string &sessionId, Events::EventPtr event);

    /**
     * @brief Update statistics counters
     */
    void incrementEventsSent() {
        statistics_.eventsSent++;
    }

    void incrementEventsReceived() {
        statistics_.eventsReceived++;
    }

    void incrementSuccessfulSessions() {
        statistics_.successfulSessions++;
    }

    void incrementFailedSessions() {
        statistics_.failedSessions++;
    }

protected:
    std::string processorType_;
    std::atomic<bool> running_{false};

    // Session management
    std::unordered_map<std::string, std::shared_ptr<SessionInfo>> sessions_;
    mutable std::mutex sessionsMutex_;

    // Callbacks
    EventCallback eventCallback_;
    StateChangeCallback stateChangeCallback_;

    // Statistics
    Statistics statistics_;
    mutable std::mutex statisticsMutex_;

    // Session ID generation
    std::atomic<uint64_t> sessionCounter_{0};
};

/**
 * @brief SCXML invoke processor for invoking other SCXML state machines
 */
class SCXMLInvokeProcessor : public InvokeProcessor {
public:
    SCXMLInvokeProcessor();

    bool canHandle(const std::string &type) const override;

    std::string startInvoke(std::shared_ptr<Model::IInvokeNode> invokeNode, Runtime::RuntimeContext &context) override;

    bool sendEvent(const std::string &sessionId, Events::EventPtr event) override;

    bool terminateInvoke(const std::string &sessionId) override;

private:
    // SCXML-specific session data
    struct SCXMLSession {
        std::unique_ptr<RuntimeContext> childContext;
        std::thread executionThread;
        std::atomic<bool> shouldTerminate{false};

        SCXMLSession() = default;

        ~SCXMLSession() {
            shouldTerminate = true;
            if (executionThread.joinable()) {
                GracefulJoin::joinWithTimeout(executionThread, 3, "SCXML_ExecutionThread");
            }
        }
    };

    std::unordered_map<std::string, std::unique_ptr<SCXMLSession>> scxmlSessions_;

    void runSCXMLSession(const std::string &sessionId, std::shared_ptr<Model::IInvokeNode> invokeNode);
};

/**
 * @brief HTTP invoke processor for invoking web services
 */
class HTTPInvokeProcessor : public InvokeProcessor {
public:
    HTTPInvokeProcessor();

    bool canHandle(const std::string &type) const override;

    std::string startInvoke(std::shared_ptr<Model::IInvokeNode> invokeNode, Runtime::RuntimeContext &context) override;

    bool sendEvent(const std::string &sessionId, Events::EventPtr event) override;

    bool terminateInvoke(const std::string &sessionId) override;

private:
    struct HTTPSession {
        std::string url;
        std::string method;
        std::unordered_map<std::string, std::string> headers;
        std::thread requestThread;
        std::atomic<bool> shouldTerminate{false};

        ~HTTPSession() {
            shouldTerminate = true;
            if (requestThread.joinable()) {
                GracefulJoin::joinWithTimeout(requestThread, 3, "HTTP_RequestThread");
            }
        }
    };

    std::unordered_map<std::string, std::unique_ptr<HTTPSession>> httpSessions_;

    void executeHTTPRequest(const std::string &sessionId, std::shared_ptr<Model::IInvokeNode> invokeNode);
    std::string eventToJSON(Events::EventPtr event) const;
};

/**
 * @brief Basic invoke processor for simple external processes
 */
class ProcessInvokeProcessor : public InvokeProcessor {
public:
    ProcessInvokeProcessor();

    bool canHandle(const std::string &type) const override;

    std::string startInvoke(std::shared_ptr<Model::IInvokeNode> invokeNode, Runtime::RuntimeContext &context) override;

    bool sendEvent(const std::string &sessionId, Events::EventPtr event) override;

    bool terminateInvoke(const std::string &sessionId) override;

private:
    struct ProcessSession {
        std::string command;
        std::vector<std::string> args;
        std::thread processThread;
        std::atomic<bool> shouldTerminate{false};
        int processId = -1;

        ~ProcessSession() {
            shouldTerminate = true;
            if (processThread.joinable()) {
                GracefulJoin::joinWithTimeout(processThread, 3, "Process_Thread");
            }
        }
    };

    std::unordered_map<std::string, std::unique_ptr<ProcessSession>> processSessions_;

    void executeProcess(const std::string &sessionId, std::shared_ptr<Model::IInvokeNode> invokeNode);
};

}  // namespace Runtime
}  // namespace SCXML
