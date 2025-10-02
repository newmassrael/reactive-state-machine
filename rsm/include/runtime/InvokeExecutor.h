#pragma once

#include "events/IEventDispatcher.h"
#include "model/IInvokeNode.h"
#include "scripting/JSEngine.h"
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RSM {

// Forward declarations
class StateMachine;
class StateMachineContext;

/**
 * @brief Interface for invoke handler implementations (Open/Closed Principle)
 *
 * Allows extension for different invoke types (SCXML, HTTP, etc.)
 * without modifying existing code.
 */
class IInvokeHandler {
public:
    virtual ~IInvokeHandler() = default;

    /**
     * @brief Start an invoke operation
     * @param invoke Invoke node containing configuration
     * @param parentSessionId Parent session ID for hierarchical sessions
     * @param eventDispatcher Event dispatcher for communication
     * @return Generated invokeid for tracking
     */
    virtual std::string startInvoke(const std::shared_ptr<IInvokeNode> &invoke, const std::string &parentSessionId,
                                    std::shared_ptr<IEventDispatcher> eventDispatcher) = 0;

    /**
     * @brief Start an invoke operation with pre-allocated session ID (architectural fix for timing)
     * @param invoke Invoke node containing configuration
     * @param parentSessionId Parent session ID for hierarchical sessions
     * @param eventDispatcher Event dispatcher for communication
     * @param childSessionId Pre-allocated child session ID to ensure mapping consistency
     * @return Generated invokeid for tracking (should match invoke ID)
     */
    virtual std::string startInvokeWithSessionId(const std::shared_ptr<IInvokeNode> &invoke,
                                                 const std::string &parentSessionId,
                                                 std::shared_ptr<IEventDispatcher> eventDispatcher,
                                                 const std::string &childSessionId) = 0;

    /**
     * @brief Cancel an ongoing invoke operation
     * @param invokeid ID of the invoke to cancel
     * @return true if successfully cancelled
     */
    virtual bool cancelInvoke(const std::string &invokeid) = 0;

    /**
     * @brief Check if invoke is still active
     * @param invokeid ID of the invoke to check
     * @return true if invoke is active
     */
    virtual bool isInvokeActive(const std::string &invokeid) const = 0;

    /**
     * @brief Get supported invoke type
     * @return Type string (e.g., "scxml", "http")
     */
    virtual std::string getType() const = 0;
};

/**
 * @brief SCXML invoke handler implementation
 *
 * Handles SCXML-to-SCXML invocation using JSEngine sessions
 * and hierarchical parent-child relationships.
 */
class SCXMLInvokeHandler : public IInvokeHandler {
public:
    SCXMLInvokeHandler();
    ~SCXMLInvokeHandler() override;

    std::string startInvoke(const std::shared_ptr<IInvokeNode> &invoke, const std::string &parentSessionId,
                            std::shared_ptr<IEventDispatcher> eventDispatcher) override;

    std::string startInvokeWithSessionId(const std::shared_ptr<IInvokeNode> &invoke, const std::string &parentSessionId,
                                         std::shared_ptr<IEventDispatcher> eventDispatcher,
                                         const std::string &childSessionId) override;

    bool cancelInvoke(const std::string &invokeid) override;
    bool isInvokeActive(const std::string &invokeid) const override;
    std::string getType() const override;

    /**
     * @brief Get all active child state machines with autoForward enabled
     * @param parentSessionId Parent session ID
     * @return Vector of child StateMachine pointers with autoForward=true
     */
    std::vector<StateMachine *> getAutoForwardSessions(const std::string &parentSessionId);

    /**
     * @brief Get finalize script for an event from an invoked child session
     * @param childSessionId Child session ID that sent the event
     * @return Finalize script if found, empty string otherwise
     */
    std::string getFinalizeScriptForChildSession(const std::string &childSessionId) const;

    /**
     * @brief Set parent StateMachine for completion callback state checking
     * @param stateMachine Shared pointer to parent StateMachine
     */
    void setParentStateMachine(std::shared_ptr<StateMachine> stateMachine);

    /**
     * @brief Check if event should be filtered due to cancelled invoke
     * @param childSessionId Child session ID that sent the event
     * @return true if event should be filtered (from cancelled invoke), false otherwise
     */
    bool shouldFilterCancelledInvokeEvent(const std::string &childSessionId) const;

private:
    struct InvokeSession {
        std::string invokeid;
        std::string sessionId;
        std::string parentSessionId;
        std::shared_ptr<IEventDispatcher> eventDispatcher;
        std::unique_ptr<StateMachineContext> smContext;  // RAII wrapper (shared_ptr ownership)
        bool isActive = true;
        bool autoForward = false;
        std::string finalizeScript;  // W3C SCXML: finalize handler script to execute before processing child events
    };

    std::unordered_map<std::string, InvokeSession> activeSessions_;

    // W3C SCXML Test 252: Track cancelled invoke child sessions to filter their events
    // Bounded FIFO cache to prevent memory leak while maintaining safety for queued events
    static constexpr size_t MAX_CANCELLED_SESSIONS = 10000;
    std::deque<std::string> cancelledSessionsOrder_;          // FIFO order for eviction
    std::unordered_set<std::string> cancelledChildSessions_;  // Fast lookup
    mutable std::mutex cancelledSessionsMutex_;               // Thread safety

    // W3C SCXML Test 192: Parent StateMachine weak_ptr for completion callback state checking (thread-safe)
    std::weak_ptr<StateMachine> parentStateMachine_;

    std::string generateInvokeId() const;

    /**
     * @brief Helper to set invoke data variables in child session (DRY for namelist/param processing)
     * @param childSessionId Target child session ID
     * @param varName Variable name to set
     * @param value ScriptValue to set
     * @param source Source description for logging ("namelist" or "param")
     */
    void setInvokeDataVariable(const std::string &childSessionId, const std::string &varName, const ScriptValue &value,
                               const std::string &source);

    /**
     * @brief Internal method containing shared invoke logic (DRY principle)
     * @param invoke Invoke node configuration
     * @param parentSessionId Parent session ID
     * @param eventDispatcher Event dispatcher for communication
     * @param childSessionId Child session ID (either generated or pre-allocated)
     * @param sessionAlreadyExists Whether the child session was pre-created
     * @return Generated invokeid for tracking
     */
    std::string startInvokeInternal(const std::shared_ptr<IInvokeNode> &invoke, const std::string &parentSessionId,
                                    std::shared_ptr<IEventDispatcher> eventDispatcher,
                                    const std::string &childSessionId, bool sessionAlreadyExists);

    /**
     * @brief Load SCXML content from file, resolving relative paths
     * @param filepath File path (may be relative or have file: prefix)
     * @param parentSessionId Parent session ID for context
     * @return SCXML content as string, or empty string on error
     */
    std::string loadSCXMLFromFile(const std::string &filepath, const std::string &parentSessionId);
};

/**
 * @brief Factory for creating invoke handlers (Factory Pattern)
 */
class InvokeHandlerFactory {
public:
    static std::shared_ptr<IInvokeHandler> createHandler(const std::string &type);
    static void registerHandler(const std::string &type, std::function<std::shared_ptr<IInvokeHandler>()> creator);

private:
    static std::unordered_map<std::string, std::function<std::shared_ptr<IInvokeHandler>()>> creators_;
};

/**
 * @brief Main invoke execution coordinator (Single Responsibility Principle)
 *
 * Coordinates invoke lifecycle management by delegating to appropriate handlers
 * while maintaining SCXML W3C compliance. Leverages existing infrastructure:
 * - JSEngine for session management
 * - IEventDispatcher for event communication
 * - IInvokeNode for parsed invoke data
 */
class InvokeExecutor {
public:
    /**
     * @brief Constructor with dependency injection (Dependency Inversion Principle)
     * @param eventDispatcher Event dispatcher for inter-session communication
     */
    explicit InvokeExecutor(std::shared_ptr<IEventDispatcher> eventDispatcher = nullptr);

    /**
     * @brief Destructor - ensures cleanup of active invokes
     */
    ~InvokeExecutor();

    /**
     * @brief Execute invoke nodes for a state entry
     * @param invokes Vector of invoke nodes to execute
     * @param sessionId Current session ID (parent for child invokes)
     * @return true if all invokes started successfully
     */
    bool executeInvokes(const std::vector<std::shared_ptr<IInvokeNode>> &invokes, const std::string &sessionId);

    /**
     * @brief Execute a single invoke node
     * @param invoke Invoke node to execute
     * @param sessionId Current session ID (parent for child invoke)
     * @return Generated invokeid, empty string on failure
     */
    std::string executeInvoke(const std::shared_ptr<IInvokeNode> &invoke, const std::string &sessionId);

    /**
     * @brief Set parent StateMachine for invoke completion callback
     * @param stateMachine Shared pointer to parent StateMachine
     */
    void setParentStateMachine(std::shared_ptr<StateMachine> stateMachine);

    /**
     * @brief Cancel specific invoke by ID
     * @param invokeid ID of invoke to cancel
     * @return true if successfully cancelled
     */
    bool cancelInvoke(const std::string &invokeid);

    /**
     * @brief Cancel all invokes for a session (W3C SCXML compliance)
     * @param sessionId Session whose invokes should be cancelled
     * @return Number of invokes cancelled
     */
    size_t cancelInvokesForSession(const std::string &sessionId);

    /**
     * @brief Cancel all active invokes
     * @return Number of invokes cancelled
     */
    size_t cancelAllInvokes();

    /**
     * @brief Check if invoke is active
     * @param invokeid ID of invoke to check
     * @return true if invoke is active
     */
    bool isInvokeActive(const std::string &invokeid) const;

    /**
     * @brief Get statistics for monitoring
     * @return Statistics string
     */
    std::string getStatistics() const;

    /**
     * @brief Set event dispatcher (for late binding)
     * @param eventDispatcher Event dispatcher instance
     */
    void setEventDispatcher(std::shared_ptr<IEventDispatcher> eventDispatcher);

    /**
     * @brief Get all active invoke sessions with autoForward enabled
     * @param parentSessionId Parent session ID
     * @return Vector of child StateMachine pointers with autoForward=true
     */
    std::vector<StateMachine *> getAutoForwardSessions(const std::string &parentSessionId);

    /**
     * @brief Get finalize script for an event from an invoked child session
     * @param childSessionId Child session ID that sent the event
     * @return Finalize script if found, empty string otherwise
     */
    std::string getFinalizeScriptForChildSession(const std::string &childSessionId) const;

    /**
     * @brief Check if event should be filtered due to cancelled invoke (W3C SCXML Test 252)
     * @param childSessionId Child session ID that sent the event
     * @return true if event should be filtered (from cancelled invoke), false otherwise
     */
    bool shouldFilterCancelledInvokeEvent(const std::string &childSessionId) const;

private:
    std::shared_ptr<IEventDispatcher> eventDispatcher_;

    // W3C SCXML 6.5: Parent StateMachine weak_ptr for completion callback state checking (thread-safe)
    std::weak_ptr<StateMachine> parentStateMachine_;

    // Track invoke sessions by parent session (for cancellation on state exit)
    std::unordered_map<std::string, std::vector<std::string>> sessionInvokes_;

    // Track handlers by invokeid for cancellation
    std::unordered_map<std::string, std::shared_ptr<IInvokeHandler>> invokeHandlers_;

    std::string generateInvokeId() const;
    void cleanupInvoke(const std::string &invokeid);
};

}  // namespace RSM
