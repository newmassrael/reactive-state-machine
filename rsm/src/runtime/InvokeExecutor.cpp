#include "runtime/InvokeExecutor.h"
#include "SCXMLTypes.h"
#include "common/Logger.h"
#include "common/UniqueIdGenerator.h"
#include "events/EventDescriptor.h"
#include "runtime/EventRaiserImpl.h"
#include "runtime/InvokeExecutor.h"
#include "runtime/StateMachine.h"
#include "runtime/StateMachineBuilder.h"
#include "runtime/StateMachineContext.h"
#include "scripting/JSEngine.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace RSM {

// ============================================================================
// SCXMLInvokeHandler Implementation
// ============================================================================

SCXMLInvokeHandler::SCXMLInvokeHandler() = default;

SCXMLInvokeHandler::~SCXMLInvokeHandler() {
    // Cancel all active sessions on destruction
    for (const auto &[invokeid, session] : activeSessions_) {
        if (session.isActive) {
            JSEngine::instance().destroySession(session.sessionId);
        }
    }
}

std::string SCXMLInvokeHandler::startInvoke(const std::shared_ptr<IInvokeNode> &invoke,
                                            const std::string &parentSessionId,
                                            std::shared_ptr<IEventDispatcher> eventDispatcher) {
    if (!invoke) {
        LOG_ERROR("SCXMLInvokeHandler: Cannot start invoke - invoke node is null");
        return "";
    }

    // Generate unique child session ID
    std::string childSessionId = JSEngine::instance().generateSessionIdString("session_");

    LOG_DEBUG("SCXMLInvokeHandler: startInvoke called with parent session: {}, generated child session: {}",
              parentSessionId, childSessionId);

    // Delegate to internal method with session creation required
    return startInvokeInternal(invoke, parentSessionId, eventDispatcher, childSessionId, false);
}

std::string SCXMLInvokeHandler::startInvokeWithSessionId(const std::shared_ptr<IInvokeNode> &invoke,
                                                         const std::string &parentSessionId,
                                                         std::shared_ptr<IEventDispatcher> eventDispatcher,
                                                         const std::string &childSessionId) {
    if (!invoke) {
        LOG_ERROR("SCXMLInvokeHandler: Cannot start invoke - invoke node is null");
        return "";
    }

    LOG_DEBUG(
        "SCXMLInvokeHandler: startInvokeWithSessionId called with parent session: {}, pre-allocated child session: {}",
        parentSessionId, childSessionId);

    // Check if session exists (architectural fix for timing)
    bool sessionExists = JSEngine::instance().hasSession(childSessionId);

    // Delegate to internal method with session existence information
    return startInvokeInternal(invoke, parentSessionId, eventDispatcher, childSessionId, sessionExists);
}

std::string SCXMLInvokeHandler::startInvokeInternal(const std::shared_ptr<IInvokeNode> &invoke,
                                                    const std::string &parentSessionId,
                                                    std::shared_ptr<IEventDispatcher> eventDispatcher,
                                                    const std::string &childSessionId, bool sessionAlreadyExists) {
    // Generate unique invoke ID
    std::string invokeid = invoke->getId().empty() ? generateInvokeId() : invoke->getId();

    LOG_INFO(
        "SCXMLInvokeHandler: Starting invoke - invokeid: {}, childSession: {}, parentSession: {}, sessionExists: {}",
        invokeid, childSessionId, parentSessionId, sessionAlreadyExists);

    // Get invoke content (SCXML document)
    std::string scxmlContent = invoke->getContent();
    LOG_DEBUG("SCXMLInvokeHandler: Invoke content length: {}, has src: {}, has srcexpr: {}", scxmlContent.length(),
              !invoke->getSrc().empty(), !invoke->getSrcExpr().empty());

    // Handle srcexpr evaluation first
    if (scxmlContent.empty() && !invoke->getSrcExpr().empty()) {
        // Evaluate srcexpr in parent session context
        auto future = JSEngine::instance().evaluateExpression(parentSessionId, invoke->getSrcExpr());
        auto result = future.get();

        if (result.isSuccess()) {
            std::string evaluatedSrc = result.getValue<std::string>();
            LOG_DEBUG("SCXMLInvokeHandler: srcexpr '{}' evaluated to '{}'", invoke->getSrcExpr(), evaluatedSrc);

            // Remove surrounding quotes if present
            if (evaluatedSrc.length() >= 2 && evaluatedSrc.front() == '\'' && evaluatedSrc.back() == '\'') {
                evaluatedSrc = evaluatedSrc.substr(1, evaluatedSrc.length() - 2);
            }

            // Load SCXML content from file
            scxmlContent = loadSCXMLFromFile(evaluatedSrc, parentSessionId);
            if (scxmlContent.empty()) {
                LOG_ERROR("SCXMLInvokeHandler: Failed to load SCXML from srcexpr file: {}", evaluatedSrc);
                return "";
            }
        } else {
            LOG_ERROR("SCXMLInvokeHandler: Failed to evaluate srcexpr '{}': {}", invoke->getSrcExpr(),
                      result.getErrorMessage());
            return "";
        }
    }
    // Handle static src attribute
    else if (scxmlContent.empty() && !invoke->getSrc().empty()) {
        LOG_INFO("SCXMLInvokeHandler: Loading SCXML from src file: {}", invoke->getSrc());
        scxmlContent = loadSCXMLFromFile(invoke->getSrc(), parentSessionId);
        if (scxmlContent.empty()) {
            LOG_ERROR("SCXMLInvokeHandler: Failed to load SCXML from src file: {}", invoke->getSrc());
            return "";
        }
    }

    if (scxmlContent.empty()) {
        LOG_ERROR("SCXMLInvokeHandler: No content, src, or srcexpr specified for invoke: {}", invokeid);
        return "";
    }

    // Create session if it doesn't exist
    if (!sessionAlreadyExists) {
        bool sessionCreated = JSEngine::instance().createSession(childSessionId, parentSessionId);
        if (!sessionCreated) {
            LOG_ERROR("SCXMLInvokeHandler: Failed to create child session: {}", childSessionId);
            return "";
        }
    }

    // W3C SCXML 6.4: Handle idlocation attribute - store invoke ID in parent session
    if (!invoke->getIdLocation().empty()) {
        JSEngine::instance().setVariable(parentSessionId, invoke->getIdLocation(), ScriptValue{invokeid});
        LOG_DEBUG("SCXMLInvokeHandler: Set idlocation '{}' = '{}' in parent session '{}'", invoke->getIdLocation(),
                  invokeid, parentSessionId);
    }

    // Set special variables in child session
    JSEngine::instance().setVariable(childSessionId, "_invokeid", ScriptValue{invokeid});
    JSEngine::instance().setVariable(childSessionId, "_parent", ScriptValue{parentSessionId});

    // W3C SCXML 6.2 compliance: Register EventDispatcher for delayed event cancellation
    if (eventDispatcher) {
        JSEngine::instance().registerEventDispatcher(childSessionId, eventDispatcher);
        LOG_DEBUG("SCXMLInvokeHandler: Registered EventDispatcher for child session: {}", childSessionId);
    } else {
        LOG_WARN("SCXMLInvokeHandler: No EventDispatcher provided for session: {}", childSessionId);
    }

    // W3C SCXML: Create EventRaiser for #_parent target support
    auto childEventRaiser = std::make_shared<EventRaiserImpl>();

    // Store session information for tracking
    InvokeSession session;
    session.invokeid = invokeid;
    session.sessionId = childSessionId;
    session.parentSessionId = parentSessionId;
    session.eventDispatcher = eventDispatcher;
    session.smContext = nullptr;  // Will be created later
    session.isActive = true;
    session.autoForward = invoke->isAutoForward();
    session.finalizeScript =
        invoke->getFinalize();  // W3C SCXML 6.4: Store finalize handler for execution before processing child events

    // Build StateMachine with dependency injection, then wrap in RAII context
    StateMachineBuilder builder;
    auto stateMachine = builder.withSessionId(childSessionId)
                            .withEventDispatcher(eventDispatcher)
                            .withEventRaiser(childEventRaiser)
                            .build();

    // Wrap in StateMachineContext for RAII cleanup (shared ownership)
    auto smContext = std::make_unique<StateMachineContext>(stateMachine);

    // Get weak_ptr for thread-safe callbacks (prevents use-after-free)
    std::weak_ptr<StateMachine> weakChildSM = smContext->getShared();

    LOG_DEBUG("SCXMLInvokeHandler: Created child StateMachine with StateMachineBuilder for session: {}",
              childSessionId);

    // W3C SCXML 6.5: Register completion callback for done.invoke generation
    // This callback is invoked AFTER the child's final state onexit handlers complete
    // IMPORTANT: Use weak_ptr to prevent accessing destroyed parent StateMachine (thread-safe)
    std::weak_ptr<StateMachine> weakParentSM = parentStateMachine_;
    weakChildSM.lock()->setCompletionCallback(
        [weakParentSM, invokeid, childSessionId, parentSessionId, eventDispatcher]() {
            LOG_INFO("SCXMLInvokeHandler: Child completion callback invoked - invokeid: {}, session: {}", invokeid,
                     childSessionId);
            LOG_INFO("SCXMLInvokeHandler: Parent check - weakPtr valid: {}, parentSessionId: {}",
                     !weakParentSM.expired(), parentSessionId);

            // W3C SCXML Test 192: Check if parent StateMachine is in final state (thread-safe with weak_ptr)
            // If parent already completed, don't send done.invoke (it would be ignored anyway)
            auto parentSM = weakParentSM.lock();
            if (!parentSM) {
                LOG_DEBUG("SCXMLInvokeHandler: Parent StateMachine destroyed, skipping done.invoke.{}", invokeid);
                return;
            }

            if (parentSM->isInFinalState()) {
                LOG_DEBUG("SCXMLInvokeHandler: Parent already in final state, skipping done.invoke.{}", invokeid);
                return;
            }

            // W3C SCXML 6.5: Generate done.invoke.id event
            std::string doneEvent = "done.invoke." + invokeid;

            if (eventDispatcher) {
                EventDescriptor event;
                event.eventName = doneEvent;
                event.target = "#_parent";
                event.data = "";
                event.delay = std::chrono::milliseconds(0);
                event.sessionId = childSessionId;  // Child session for context

                // W3C SCXML: Use parent session ID directly to avoid lookup issues
                // Store parent session ID in params for ParentEventTarget routing
                event.params["_parentSessionId"] = parentSessionId;

                auto resultFuture = eventDispatcher->sendEvent(event);
                LOG_INFO("SCXMLInvokeHandler: {} sent to parent after child completion (target: {}, parentSession: {})",
                         doneEvent, event.target, parentSessionId);
            } else {
                LOG_WARN("SCXMLInvokeHandler: Child reached final state but no EventDispatcher available for: {}",
                         doneEvent);
            }
        });
    LOG_DEBUG("SCXMLInvokeHandler: Registered completion callback for invoke: {}", invokeid);

    // Set up EventRaiser callback to child StateMachine's processEvent
    LOG_DEBUG("SCXMLInvokeHandler: Setting EventRaiser callback for session: {}, EventRaiser: {}", childSessionId,
              (void *)childEventRaiser.get());

    // Reuse weakParentSM from completion callback above
    childEventRaiser->setEventCallback([weakChildSM, weakParentSM, childSessionId](
                                           const std::string &eventName, const std::string &eventData) -> bool {
        // Thread-safe callback: use weak_ptr to prevent use-after-free
        auto childSM = weakChildSM.lock();
        if (!childSM) {
            LOG_DEBUG("EventRaiser callback skipped - child StateMachine destroyed, session: {}, event: '{}'",
                      childSessionId, eventName);
            return false;
        }

        LOG_DEBUG("EventRaiser callback executing - session: {}, event: '{}', isRunning: {}", childSessionId, eventName,
                  childSM->isRunning() ? "true" : "false");

        // W3C SCXML: Don't process events if parent is in final state (thread-safe with weak_ptr)
        auto parentSM = weakParentSM.lock();
        if (parentSM && parentSM->isInFinalState()) {
            LOG_DEBUG("EventRaiser callback skipped - parent in final state, session: {}, event: '{}'", childSessionId,
                      eventName);
            return false;
        }

        // W3C SCXML: Don't process events if child is in final state
        if (childSM->isRunning() && !childSM->isInFinalState()) {
            auto result = childSM->processEvent(eventName, eventData);
            LOG_DEBUG("EventRaiser callback result - session: {}, event: '{}', success: {}", childSessionId, eventName,
                      result.success);
            return result.success;
        }
        LOG_DEBUG("EventRaiser callback skipped - session: {}, event: '{}', StateMachine not running or in final state",
                  childSessionId, eventName);
        return false;
    });

    LOG_DEBUG("SCXMLInvokeHandler: EventRaiser callback setup complete for session: {}", childSessionId);

    // Assign StateMachineContext to session before loading (keeps StateMachine alive)
    session.smContext = std::move(smContext);

    // Load SCXML content into child StateMachine
    LOG_DEBUG(
        "SCXMLInvokeHandler: Loading SCXML content into child StateMachine - invokeid: {}, content size: {} bytes",
        invokeid, scxmlContent.length());
    if (!session.smContext->get()->loadSCXMLFromString(scxmlContent)) {
        LOG_ERROR("SCXMLInvokeHandler: Failed to load SCXML content for invoke: {}", invokeid);
        JSEngine::instance().destroySession(childSessionId);
        return "";
    }
    LOG_DEBUG("SCXMLInvokeHandler: Successfully loaded SCXML content for invoke: {}", invokeid);

    // W3C SCXML 6.4: Set invoke data AFTER loading but BEFORE starting
    // This ensures namelist/param values override child's datamodel initial values

    // W3C SCXML 6.4: Get child's datamodel variable names for validation
    // "If the name of a param element or the key of a namelist item do not match the name of a data
    // element in the invoked process, the Processor MUST NOT add the value to the invoked session's data model"
    std::set<std::string> childDatamodelVars;
    auto childModel = session.smContext->get()->getModel();
    if (childModel) {
        childDatamodelVars = childModel->getDataModelVariableNames();
        LOG_DEBUG("SCXMLInvokeHandler: Child session {} has {} datamodel variables", childSessionId,
                  childDatamodelVars.size());
    }

    // W3C SCXML 6.4: Handle namelist attribute - pass datamodel variables by name
    const std::string &namelist = invoke->getNamelist();
    if (!namelist.empty()) {
        std::istringstream iss(namelist);
        std::string varName;
        while (iss >> varName) {
            // W3C SCXML 6.4: Only set variable if it exists in child's datamodel
            if (childDatamodelVars.find(varName) == childDatamodelVars.end()) {
                LOG_DEBUG("SCXMLInvokeHandler: Skipping namelist variable '{}' - not defined in child's datamodel",
                          varName);
                continue;
            }

            auto future = JSEngine::instance().getVariable(parentSessionId, varName);
            auto result = future.get();

            if (JSEngine::isSuccess(result)) {
                setInvokeDataVariable(childSessionId, varName, result.getInternalValue(), "namelist");
            } else {
                LOG_WARN("SCXMLInvokeHandler: Failed to get namelist variable '{}' from parent session", varName);
            }
        }
    }

    // W3C SCXML 6.4: Set up invoke parameters in child session data model
    const auto &params = invoke->getParams();
    for (const auto &[name, expr, location] : params) {
        if (!name.empty()) {
            // W3C SCXML 6.4: Only set variable if it exists in child's datamodel
            if (childDatamodelVars.find(name) == childDatamodelVars.end()) {
                LOG_DEBUG("SCXMLInvokeHandler: Skipping param '{}' - not defined in child's datamodel", name);
                continue;
            }

            auto future = JSEngine::instance().evaluateExpression(parentSessionId, expr);
            auto result = future.get();

            if (JSEngine::isSuccess(result)) {
                setInvokeDataVariable(childSessionId, name, result.getInternalValue(), "param");
            } else {
                LOG_WARN("SCXMLInvokeHandler: Failed to evaluate param expression: {}", expr);
            }
        }
    }

    // Add session to activeSessions_
    activeSessions_.emplace(invokeid, std::move(session));

    // Get reference to the session we just added (session was moved, can't use it anymore)
    auto &activeSession = activeSessions_[invokeid];

    // Start the child StateMachine
    LOG_DEBUG("SCXMLInvokeHandler: Starting child StateMachine for invoke: {}", invokeid);
    if (!activeSession.smContext->get()->start()) {
        LOG_ERROR("SCXMLInvokeHandler: Failed to start child StateMachine for invoke: {}", invokeid);
        activeSessions_.erase(invokeid);
        JSEngine::instance().destroySession(childSessionId);
        return "";
    }

    LOG_INFO("SCXMLInvokeHandler: Child StateMachine started successfully for invoke: {} (session: {})", invokeid,
             childSessionId);

    // W3C SCXML 6.5: done.invoke generation is now handled by completion callback
    // The callback ensures proper event ordering: child onexit → done.invoke
    // No need for synchronous done.invoke generation here

    // Register invoke mapping in JSEngine for #_invokeid target support
    // For pre-allocated sessions (sessionAlreadyExists=true), mapping may already be registered by InvokeExecutor
    if (!sessionAlreadyExists) {
        JSEngine::instance().registerInvokeMapping(parentSessionId, invokeid, childSessionId);
    }

    LOG_INFO("SCXMLInvokeHandler: Successfully started SCXML invoke: {} with session: {} and running StateMachine",
             invokeid, childSessionId);

    return invokeid;
}

bool SCXMLInvokeHandler::cancelInvoke(const std::string &invokeid) {
    auto it = activeSessions_.find(invokeid);
    if (it == activeSessions_.end()) {
        LOG_WARN("SCXMLInvokeHandler: Cannot cancel invoke - not found: {}", invokeid);
        return false;
    }

    InvokeSession &session = it->second;
    if (!session.isActive) {
        LOG_WARN("SCXMLInvokeHandler: Invoke already inactive: {}", invokeid);
        return false;
    }

    LOG_DEBUG("SCXMLInvokeHandler: Cancelling invoke: {} with session: {}", invokeid, session.sessionId);

    // Cancel pending events
    if (session.eventDispatcher) {
        size_t cancelledEvents = session.eventDispatcher->cancelEventsForSession(session.sessionId);
        LOG_DEBUG("SCXMLInvokeHandler: Cancelled {} pending events for session: {}", cancelledEvents,
                  session.sessionId);
    }

    // W3C SCXML Test 252: Track cancelled child session BEFORE destroying to filter onexit events
    // Use bounded FIFO cache to prevent memory leak
    size_t cacheSize;
    {
        std::lock_guard<std::mutex> lock(cancelledSessionsMutex_);

        // Only add if not already present (prevents duplicate entries in deque)
        if (cancelledChildSessions_.insert(session.sessionId).second) {
            // Successfully inserted new entry - add to FIFO order
            cancelledSessionsOrder_.push_back(session.sessionId);

            // Enforce bounded cache by removing oldest entries
            if (cancelledSessionsOrder_.size() > MAX_CANCELLED_SESSIONS) {
                std::string oldest = cancelledSessionsOrder_.front();
                cancelledSessionsOrder_.pop_front();
                cancelledChildSessions_.erase(oldest);
                LOG_DEBUG("SCXMLInvokeHandler: Evicted oldest cancelled session from cache: {}", oldest);
            }
        }
        cacheSize = cancelledSessionsOrder_.size();
    }
    LOG_DEBUG("SCXMLInvokeHandler: Added cancelled child session to filter list: {} (cache size: {})",
              session.sessionId, cacheSize);

    // With weak_ptr callbacks, no synchronization needed - callbacks safely check weak_ptr::lock()
    // Unregister invoke mapping from JSEngine
    JSEngine::instance().unregisterInvokeMapping(session.parentSessionId, invokeid);

    // Destroy the child session (RAII cleanup with thread-safe weak_ptr callbacks)
    // Callbacks use weak_ptr::lock() which returns nullptr if StateMachine is destroyed
    // Child's onexit handlers may send events to parent during destruction - these will be filtered
    JSEngine::instance().destroySession(session.sessionId);

    // Mark as inactive
    session.isActive = false;

    LOG_INFO("SCXMLInvokeHandler: Successfully cancelled invoke: {}", invokeid);
    return true;
}

bool SCXMLInvokeHandler::isInvokeActive(const std::string &invokeid) const {
    auto it = activeSessions_.find(invokeid);
    return it != activeSessions_.end() && it->second.isActive;
}

std::string SCXMLInvokeHandler::getType() const {
    return "scxml";
}

void SCXMLInvokeHandler::setInvokeDataVariable(const std::string &childSessionId, const std::string &varName,
                                               const ScriptValue &value, const std::string &source) {
    JSEngine::instance().setVariable(childSessionId, varName, value);
    LOG_INFO("SCXMLInvokeHandler: Set {} variable '{}' in child session", source, varName);
}

bool SCXMLInvokeHandler::shouldFilterCancelledInvokeEvent(const std::string &childSessionId) const {
    // W3C SCXML Test 252: Filter events from cancelled invoke child sessions
    bool shouldFilter;
    {
        std::lock_guard<std::mutex> lock(cancelledSessionsMutex_);
        shouldFilter = cancelledChildSessions_.find(childSessionId) != cancelledChildSessions_.end();
    }
    if (shouldFilter) {
        LOG_DEBUG("SCXMLInvokeHandler: Filtering event from cancelled child session: {}", childSessionId);
    }
    return shouldFilter;
}

std::string SCXMLInvokeHandler::generateInvokeId() const {
    // REFACTOR: Use centralized UniqueIdGenerator instead of duplicate logic
    return UniqueIdGenerator::generateInvokeId();
}

// ============================================================================
// InvokeHandlerFactory Implementation
// ============================================================================

std::unordered_map<std::string, std::function<std::shared_ptr<IInvokeHandler>()>> InvokeHandlerFactory::creators_;

std::shared_ptr<IInvokeHandler> InvokeHandlerFactory::createHandler(const std::string &type) {
    // Initialize default handlers on first call
    static bool initialized = false;
    if (!initialized) {
        // W3C SCXML: Register all standard SCXML invoke type variations
        auto scxmlHandler = []() { return std::make_shared<SCXMLInvokeHandler>(); };
        registerHandler("scxml", scxmlHandler);
        registerHandler("http://www.w3.org/TR/scxml/", scxmlHandler);  // With trailing slash
        registerHandler("http://www.w3.org/TR/scxml", scxmlHandler);   // Without trailing slash
        initialized = true;
    }

    auto it = creators_.find(type);
    if (it != creators_.end()) {
        return it->second();
    }

    LOG_WARN("InvokeHandlerFactory: Unknown invoke type: {}, falling back to SCXML handler", type);
    return std::make_shared<SCXMLInvokeHandler>();
}

void InvokeHandlerFactory::registerHandler(const std::string &type,
                                           std::function<std::shared_ptr<IInvokeHandler>()> creator) {
    creators_[type] = creator;
    LOG_DEBUG("InvokeHandlerFactory: Registered handler for type: {}", type);
}

// ============================================================================
// InvokeExecutor Implementation
// ============================================================================

InvokeExecutor::InvokeExecutor(std::shared_ptr<IEventDispatcher> eventDispatcher) : eventDispatcher_(eventDispatcher) {
    LOG_DEBUG("InvokeExecutor: Created with eventDispatcher: {}", eventDispatcher ? "provided" : "null");
}

InvokeExecutor::~InvokeExecutor() {
    size_t cancelled = cancelAllInvokes();
    if (cancelled > 0) {
        LOG_INFO("InvokeExecutor: Cancelled {} active invokes on destruction", cancelled);
    }
}

bool InvokeExecutor::executeInvokes(const std::vector<std::shared_ptr<IInvokeNode>> &invokes,
                                    const std::string &sessionId) {
    if (invokes.empty()) {
        LOG_DEBUG("InvokeExecutor: No invokes to execute for session: {}", sessionId);
        return true;
    }

    LOG_DEBUG("InvokeExecutor: Executing {} invokes for session: {}", invokes.size(), sessionId);

    bool allSucceeded = true;
    std::vector<std::string> sessionInvokeIds;

    for (const auto &invoke : invokes) {
        std::string invokeid = executeInvoke(invoke, sessionId);
        if (!invokeid.empty()) {
            sessionInvokeIds.push_back(invokeid);
            LOG_DEBUG("InvokeExecutor: Successfully started invoke: {} for session: {}", invokeid, sessionId);
        } else {
            LOG_ERROR("InvokeExecutor: Failed to start invoke for session: {}", sessionId);
            allSucceeded = false;
        }
    }

    // Track invokes by session for cleanup
    if (!sessionInvokeIds.empty()) {
        sessionInvokes_[sessionId] = sessionInvokeIds;
    }

    return allSucceeded;
}

std::string InvokeExecutor::executeInvoke(const std::shared_ptr<IInvokeNode> &invoke, const std::string &sessionId) {
    if (!invoke) {
        LOG_ERROR("InvokeExecutor: Cannot execute null invoke node");
        return "";
    }

    LOG_DEBUG("InvokeExecutor: executeInvoke called - session: {}, invokeId: {}, type: {}", sessionId, invoke->getId(),
              invoke->getType());

    std::string invokeType = invoke->getType();

    // W3C SCXML 1.0: Handle typeexpr attribute for dynamic type evaluation
    if (invokeType.empty() && !invoke->getTypeExpr().empty()) {
        try {
            auto future = JSEngine::instance().evaluateExpression(sessionId, invoke->getTypeExpr());
            auto jsResult = future.get();
            if (!jsResult.isSuccess()) {
                LOG_ERROR("InvokeExecutor: Failed to evaluate typeexpr '{}': {}", invoke->getTypeExpr(),
                          jsResult.getErrorMessage());
                return "";
            }
            invokeType = JSEngine::resultToString(jsResult, sessionId, invoke->getTypeExpr());
            LOG_DEBUG("InvokeExecutor: Evaluated typeexpr '{}' to type: '{}'", invoke->getTypeExpr(), invokeType);
        } catch (const std::exception &e) {
            LOG_ERROR("InvokeExecutor: Failed to evaluate typeexpr '{}': {}", invoke->getTypeExpr(), e.what());
            return "";
        }
    }

    if (invokeType.empty()) {
        invokeType = "scxml";  // Default to SCXML type
    }

    LOG_DEBUG("InvokeExecutor: Executing invoke of type: {} for session: {}", invokeType, sessionId);

    // Check if invoke is already active to prevent duplicate execution
    std::string invokeId = invoke->getId();
    LOG_DEBUG("InvokeExecutor: DETAILED DEBUG - invoke ID: '{}', isEmpty: {}", invokeId, invokeId.empty());

    if (!invokeId.empty()) {
        bool isActive = isInvokeActive(invokeId);
        LOG_DEBUG("InvokeExecutor: DETAILED DEBUG - isInvokeActive('{}') returned: {}", invokeId, isActive);

        if (isActive) {
            LOG_WARN("InvokeExecutor: Invoke {} already active, skipping duplicate execution", invokeId);
            return invokeId;
        } else {
            LOG_DEBUG("InvokeExecutor: Invoke {} is not active, proceeding with execution", invokeId);
        }
    } else {
        LOG_DEBUG("InvokeExecutor: Invoke ID is empty, proceeding with execution");
    }

    // Create appropriate handler using factory pattern
    auto handler = InvokeHandlerFactory::createHandler(invokeType);
    if (!handler) {
        LOG_ERROR("InvokeExecutor: Failed to create handler for invoke type: {}", invokeType);
        return "";
    }

    // W3C SCXML Test 192: Set parent StateMachine for completion callback state checking (thread-safe with shared_ptr)
    auto scxmlHandler = std::dynamic_pointer_cast<SCXMLInvokeHandler>(handler);
    auto parentSM = parentStateMachine_.lock();
    if (scxmlHandler && parentSM) {
        scxmlHandler->setParentStateMachine(parentSM);
        LOG_DEBUG("InvokeExecutor: Set parent StateMachine for SCXML handler");
    }

    // ARCHITECTURAL FIX: Pre-register invoke mapping BEFORE execution for immediate availability
    // This ensures that any transition actions executing during invoke startup can find the mapping
    std::string reservedInvokeId = invoke->getId();
    std::string childSessionId;  // Declare outside if block for proper scope

    if (!reservedInvokeId.empty()) {
        // Pre-register handler to prevent duplicate execution
        invokeHandlers_[reservedInvokeId] = handler;
        LOG_DEBUG("InvokeExecutor: Pre-registered invoke '{}' to prevent duplicates", reservedInvokeId);

        // CRITICAL: Generate child session ID and register invoke mapping IMMEDIATELY
        // This architectural change ensures mapping is available during transition actions
        childSessionId = JSEngine::instance().generateSessionIdString("session_");
        JSEngine::instance().registerInvokeMapping(sessionId, reservedInvokeId, childSessionId);
        LOG_DEBUG("InvokeExecutor: ARCHITECTURAL FIX - Pre-registered invoke mapping: parent={}, invoke={}, child={}",
                  sessionId, reservedInvokeId, childSessionId);
    }

    // Execute invoke using appropriate method
    std::string invokeid;
    if (!reservedInvokeId.empty()) {
        LOG_DEBUG("InvokeExecutor: Starting invoke with session ID - reservedInvokeId: {}, childSessionId: {}",
                  reservedInvokeId, childSessionId);
        // Pass the pre-allocated child session ID to the handler
        invokeid = handler->startInvokeWithSessionId(invoke, sessionId, eventDispatcher_, childSessionId);
    } else {
        LOG_DEBUG("InvokeExecutor: Starting invoke without explicit ID - generating session ID");
        // Fallback for invokes without explicit ID
        invokeid = handler->startInvoke(invoke, sessionId, eventDispatcher_);
    }
    if (invokeid.empty()) {
        LOG_ERROR("InvokeExecutor: Handler failed to start invoke of type: {}", invokeType);

        // Remove pre-registration if invoke failed
        if (!reservedInvokeId.empty()) {
            invokeHandlers_.erase(reservedInvokeId);
            LOG_DEBUG("InvokeExecutor: Removed pre-registration for failed invoke '{}'", reservedInvokeId);
        }
        return "";
    }

    // Track handler for cancellation (update if different ID was generated)
    invokeHandlers_[invokeid] = handler;

    LOG_INFO("InvokeExecutor: Successfully executed invoke: {} of type: {} for session: {}", invokeid, invokeType,
             sessionId);

    return invokeid;
}

bool InvokeExecutor::cancelInvoke(const std::string &invokeid) {
    auto handlerIt = invokeHandlers_.find(invokeid);
    if (handlerIt == invokeHandlers_.end()) {
        LOG_WARN("InvokeExecutor: Cannot cancel invoke - not found: {}", invokeid);
        return false;
    }

    bool cancelled = handlerIt->second->cancelInvoke(invokeid);
    if (cancelled) {
        cleanupInvoke(invokeid);
    }

    return cancelled;
}

size_t InvokeExecutor::cancelInvokesForSession(const std::string &sessionId) {
    auto sessionIt = sessionInvokes_.find(sessionId);
    if (sessionIt == sessionInvokes_.end()) {
        LOG_DEBUG("InvokeExecutor: No invokes to cancel for session: {}", sessionId);
        return 0;
    }

    size_t cancelledCount = 0;
    const auto &invokeIds = sessionIt->second;

    LOG_DEBUG("InvokeExecutor: Cancelling {} invokes for session: {}", invokeIds.size(), sessionId);

    for (const std::string &invokeid : invokeIds) {
        if (cancelInvoke(invokeid)) {
            cancelledCount++;
        }
    }

    // Remove session tracking
    sessionInvokes_.erase(sessionIt);

    LOG_INFO("InvokeExecutor: Cancelled {}/{} invokes for session: {}", cancelledCount, invokeIds.size(), sessionId);

    return cancelledCount;
}

size_t InvokeExecutor::cancelAllInvokes() {
    size_t cancelledCount = 0;

    // Cancel all active invokes
    for (auto &[invokeid, handler] : invokeHandlers_) {
        if (handler->isInvokeActive(invokeid)) {
            if (handler->cancelInvoke(invokeid)) {
                cancelledCount++;
            }
        }
    }

    // Clear all tracking
    invokeHandlers_.clear();
    sessionInvokes_.clear();

    LOG_INFO("InvokeExecutor: Cancelled {} invokes", cancelledCount);
    return cancelledCount;
}

bool InvokeExecutor::isInvokeActive(const std::string &invokeid) const {
    // Simply check if the invoke ID is registered in invokeHandlers_
    // This handles both pre-registered (to prevent duplicates) and fully active invokes
    auto handlerIt = invokeHandlers_.find(invokeid);
    return handlerIt != invokeHandlers_.end();
}

std::string InvokeExecutor::getStatistics() const {
    std::ostringstream stats;
    stats << "InvokeExecutor Statistics:\n";
    stats << "  Active invokes: " << invokeHandlers_.size() << "\n";
    stats << "  Sessions with invokes: " << sessionInvokes_.size() << "\n";
    stats << "  EventDispatcher: " << (eventDispatcher_ ? "available" : "null") << "\n";

    return stats.str();
}

void InvokeExecutor::setEventDispatcher(std::shared_ptr<IEventDispatcher> eventDispatcher) {
    eventDispatcher_ = eventDispatcher;
    LOG_DEBUG("InvokeExecutor: EventDispatcher set: {}", eventDispatcher ? "provided" : "null");
}

void InvokeExecutor::setParentStateMachine(std::shared_ptr<StateMachine> stateMachine) {
    parentStateMachine_ = stateMachine;  // Store as weak_ptr
    LOG_DEBUG("InvokeExecutor: Parent StateMachine set: {}", (void *)stateMachine.get());

    // Forward to all handlers that support it (currently only SCXML handler)
    for (const auto &[invokeid, handler] : invokeHandlers_) {
        auto scxmlHandler = std::dynamic_pointer_cast<SCXMLInvokeHandler>(handler);
        if (scxmlHandler) {
            scxmlHandler->setParentStateMachine(stateMachine);
        }
    }
}

std::string InvokeExecutor::generateInvokeId() const {
    // REFACTOR: Use centralized UniqueIdGenerator instead of duplicate logic
    return UniqueIdGenerator::generateInvokeId();
}

std::vector<StateMachine *> InvokeExecutor::getAutoForwardSessions(const std::string &parentSessionId) {
    std::vector<StateMachine *> result;

    // Iterate through all handlers and collect autoForward sessions
    for (const auto &[invokeid, handler] : invokeHandlers_) {
        // Check if handler is SCXML type
        if (handler->getType() == "scxml") {
            auto scxmlHandler = std::dynamic_pointer_cast<SCXMLInvokeHandler>(handler);
            if (scxmlHandler) {
                auto sessions = scxmlHandler->getAutoForwardSessions(parentSessionId);
                result.insert(result.end(), sessions.begin(), sessions.end());
            }
        }
    }

    return result;
}

void InvokeExecutor::cleanupInvoke(const std::string &invokeid) {
    // Remove from handler tracking
    invokeHandlers_.erase(invokeid);

    // Remove from session tracking
    for (auto &[sessionId, invokeIds] : sessionInvokes_) {
        auto it = std::find(invokeIds.begin(), invokeIds.end(), invokeid);
        if (it != invokeIds.end()) {
            invokeIds.erase(it);
            if (invokeIds.empty()) {
                sessionInvokes_.erase(sessionId);
            }
            break;
        }
    }

    LOG_DEBUG("InvokeExecutor: Cleaned up invoke: {}", invokeid);
}

std::string InvokeExecutor::getFinalizeScriptForChildSession(const std::string &childSessionId) const {
    // Iterate through all handlers to find the one with matching child session
    for (const auto &[invokeid, handler] : invokeHandlers_) {
        // Check if handler is SCXML type
        if (handler->getType() == "scxml") {
            auto scxmlHandler = std::dynamic_pointer_cast<SCXMLInvokeHandler>(handler);
            if (scxmlHandler) {
                std::string finalizeScript = scxmlHandler->getFinalizeScriptForChildSession(childSessionId);
                if (!finalizeScript.empty()) {
                    LOG_DEBUG("InvokeExecutor: Found finalize script for child session: {}", childSessionId);
                    return finalizeScript;
                }
            }
        }
    }

    LOG_DEBUG("InvokeExecutor: No finalize script found for child session: {}", childSessionId);
    return "";
}

bool InvokeExecutor::shouldFilterCancelledInvokeEvent(const std::string &childSessionId) const {
    // W3C SCXML Test 252: Check all handlers to see if child session is from cancelled invoke
    for (const auto &[invokeid, handler] : invokeHandlers_) {
        if (handler->getType() == "scxml") {
            auto scxmlHandler = std::dynamic_pointer_cast<SCXMLInvokeHandler>(handler);
            if (scxmlHandler && scxmlHandler->shouldFilterCancelledInvokeEvent(childSessionId)) {
                return true;
            }
        }
    }
    return false;
}

void SCXMLInvokeHandler::setParentStateMachine(std::shared_ptr<StateMachine> stateMachine) {
    parentStateMachine_ = stateMachine;  // Store as weak_ptr
    LOG_DEBUG("SCXMLInvokeHandler: Parent StateMachine set: {}", (void *)stateMachine.get());
}

std::string SCXMLInvokeHandler::loadSCXMLFromFile(const std::string &filepath, const std::string &parentSessionId) {
    std::string cleanPath = filepath;

    // Remove "file:" prefix if present
    if (cleanPath.starts_with("file:")) {
        cleanPath = cleanPath.substr(5);
    }

    // Security: Validate path to prevent directory traversal attacks
    if (cleanPath.find("..") != std::string::npos) {
        LOG_ERROR("SCXMLInvokeHandler: Invalid file path - directory traversal detected: '{}'", cleanPath);
        return "";
    }

    // Security: Only allow .scxml and .txml file extensions
    std::filesystem::path pathCheck(cleanPath);
    std::string ext = pathCheck.extension().string();
    if (!ext.empty() && ext != ".scxml" && ext != ".txml") {
        LOG_ERROR("SCXMLInvokeHandler: Invalid file extension - only .scxml and .txml allowed: '{}'", cleanPath);
        return "";
    }

    // Handle relative path resolution using parent session file path
    std::filesystem::path resolvedPath;
    if (std::filesystem::path(cleanPath).is_relative()) {
        // Get parent session's file path for relative resolution
        std::string parentFilePath = RSM::JSEngine::instance().getSessionFilePath(parentSessionId);

        if (!parentFilePath.empty()) {
            // Resolve relative to parent SCXML file directory
            std::filesystem::path parentDir = std::filesystem::path(parentFilePath).parent_path();
            resolvedPath = parentDir / cleanPath;

            // Security: Normalize path and validate it stays within allowed boundaries
            try {
                resolvedPath = std::filesystem::weakly_canonical(resolvedPath);
                std::filesystem::path normalizedParentDir = std::filesystem::weakly_canonical(parentDir);

                // Check if resolved path is still within the parent directory tree
                auto relativePath = std::filesystem::relative(resolvedPath, normalizedParentDir);
                if (relativePath.string().starts_with("..")) {
                    LOG_ERROR("SCXMLInvokeHandler: Security violation - path escapes parent directory: '{}'",
                              resolvedPath.string());
                    return "";
                }
            } catch (const std::filesystem::filesystem_error &e) {
                LOG_ERROR("SCXMLInvokeHandler: Filesystem error during path normalization: {}", e.what());
                return "";
            }

            LOG_DEBUG("SCXMLInvokeHandler: Resolving relative path '{}' to '{}' (parent: '{}')", cleanPath,
                      resolvedPath.string(), parentFilePath);
        } else {
            // Production engine requires proper file path tracking - no fallback
            LOG_ERROR(
                "SCXMLInvokeHandler: No parent file path found for session '{}' - cannot resolve relative path: '{}'",
                parentSessionId, cleanPath);
            return "";
        }
    } else {
        resolvedPath = cleanPath;
    }

    // Check if SCXML or TXML file exists
    std::filesystem::path finalPath = resolvedPath;
    if (!finalPath.has_extension()) {
        // Try .scxml first, then .txml
        std::filesystem::path scxmlPath = finalPath;
        scxmlPath += ".scxml";
        std::filesystem::path txmlPath = finalPath;
        txmlPath += ".txml";

        if (std::filesystem::exists(scxmlPath)) {
            finalPath = scxmlPath;
        } else if (std::filesystem::exists(txmlPath)) {
            finalPath = txmlPath;
        } else {
            LOG_ERROR("SCXMLInvokeHandler: SCXML/TXML file not found: '{}' (tried .scxml and .txml)",
                      finalPath.string());
            return "";
        }
    } else {
        // Extension specified, verify file exists
        if (!std::filesystem::exists(finalPath)) {
            LOG_ERROR("SCXMLInvokeHandler: File not found: '{}'", finalPath.string());
            return "";
        }
    }

    LOG_DEBUG("SCXMLInvokeHandler: Found file at '{}'", finalPath.string());

    // Read file content
    std::ifstream file(finalPath);
    if (!file.is_open()) {
        LOG_ERROR("SCXMLInvokeHandler: Failed to open file: '{}'", finalPath.string());
        return "";
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (content.empty()) {
        LOG_WARN("SCXMLInvokeHandler: File is empty: '{}'", finalPath.string());
        return "";
    }

    // Production engine only handles pure SCXML files

    LOG_INFO("SCXMLInvokeHandler: Successfully loaded SCXML content from file: '{}' ({} bytes)", finalPath.string(),
             content.length());

    return content;
}

std::vector<StateMachine *> SCXMLInvokeHandler::getAutoForwardSessions(const std::string &parentSessionId) {
    std::vector<StateMachine *> result;
    for (auto &[invokeid, session] : activeSessions_) {
        if (session.isActive && session.parentSessionId == parentSessionId && session.autoForward) {
            if (session.smContext) {
                result.push_back(session.smContext->get());
            }
        }
    }
    return result;
}

std::string SCXMLInvokeHandler::getFinalizeScriptForChildSession(const std::string &childSessionId) const {
    for (const auto &[invokeid, session] : activeSessions_) {
        if (session.isActive && session.sessionId == childSessionId) {
            LOG_DEBUG("SCXMLInvokeHandler: Found finalize script for child session: {} (invokeid: {})", childSessionId,
                      invokeid);
            return session.finalizeScript;
        }
    }
    return "";
}

}  // namespace RSM