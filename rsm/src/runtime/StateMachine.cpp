#include "runtime/StateMachine.h"
#include "common/Logger.h"
#include "events/EventRaiserService.h"

#include "factory/NodeFactory.h"
#include "model/SCXMLModel.h"
#include "parsing/SCXMLParser.h"
#include "parsing/XIncludeProcessor.h"
#include "runtime/ActionExecutorImpl.h"
#include "runtime/DeepHistoryFilter.h"
#include "runtime/EventRaiserImpl.h"
#include "runtime/ExecutionContextImpl.h"
#include "runtime/HistoryManager.h"
#include "runtime/HistoryStateAutoRegistrar.h"
#include "runtime/HistoryValidator.h"
#include "runtime/ImmediateModeGuard.h"
#include "runtime/ShallowHistoryFilter.h"
#include "scripting/JSEngine.h"
#include "states/ConcurrentRegion.h"
#include "states/ConcurrentStateNode.h"
#include <algorithm>
#include <fstream>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <unordered_set>

namespace RSM {

// RAII guard for exception-safe initial configuration flag management
namespace {
class InitialConfigurationGuard {
public:
    explicit InitialConfigurationGuard(bool &flag) : flag_(flag) {
        flag_ = true;
    }

    ~InitialConfigurationGuard() {
        flag_ = false;
    }

    // Non-copyable and non-movable
    InitialConfigurationGuard(const InitialConfigurationGuard &) = delete;
    InitialConfigurationGuard &operator=(const InitialConfigurationGuard &) = delete;
    InitialConfigurationGuard(InitialConfigurationGuard &&) = delete;
    InitialConfigurationGuard &operator=(InitialConfigurationGuard &&) = delete;

private:
    bool &flag_;
};
}  // anonymous namespace

StateMachine::StateMachine() : isRunning_(false), jsEnvironmentReady_(false) {
    sessionId_ = JSEngine::instance().generateSessionIdString("sm_");
    // JS 환경은 지연 초기화로 변경
    // ActionExecutor와 ExecutionContext는 setupJSEnvironment에서 초기화

    // Initialize History Manager with SOLID architecture (Dependency Injection)
    initializeHistoryManager();

    // Initialize InvokeExecutor with SOLID architecture (W3C SCXML invoke support)
    invokeExecutor_ = std::make_unique<InvokeExecutor>(nullptr);  // EventDispatcher will be set later if needed
}

// Constructor with session ID injection for invoke scenarios
StateMachine::StateMachine(const std::string &sessionId) : isRunning_(false), jsEnvironmentReady_(false) {
    if (sessionId.empty()) {
        throw std::invalid_argument("StateMachine: Session ID cannot be empty when using injection constructor");
    }

    sessionId_ = sessionId;
    LOG_DEBUG("StateMachine: Created with injected session ID: {}", sessionId_);

    // JS environment uses lazy initialization - use existing session
    // ActionExecutor and ExecutionContext are initialized in setupJSEnvironment

    // Initialize History Manager with SOLID architecture (Dependency Injection)
    initializeHistoryManager();

    // Initialize InvokeExecutor with SOLID architecture (W3C SCXML invoke support)
    invokeExecutor_ = std::make_unique<InvokeExecutor>(nullptr);  // EventDispatcher will be set later if needed
}

StateMachine::~StateMachine() {
    // Clear callbacks first to prevent execution during destruction
    completionCallback_ = nullptr;

    if (isRunning_) {
        stop();
    }
    // Clean up session only if JS environment was initialized
    if (jsEnvironmentReady_) {
        RSM::JSEngine::instance().destroySession(sessionId_);
    }
}

bool StateMachine::loadSCXML(const std::string &filename) {
    try {
        auto nodeFactory = std::make_shared<NodeFactory>();
        auto xincludeProcessor = std::make_shared<XIncludeProcessor>();
        SCXMLParser parser(nodeFactory, xincludeProcessor);

        model_ = parser.parseFile(filename);
        if (!model_) {
            LOG_ERROR("Failed to parse SCXML file: {}", filename);
            return false;
        }

        // Register file path for this session to enable relative path resolution
        RSM::JSEngine::instance().registerSessionFilePath(sessionId_, filename);
        LOG_DEBUG("StateMachine: Registered file path '{}' for session '{}'", filename, sessionId_);

        return initializeFromModel();
    } catch (const std::exception &e) {
        LOG_ERROR("Exception loading SCXML: {}", e.what());
        return false;
    }
}

bool StateMachine::loadSCXMLFromString(const std::string &scxmlContent) {
    try {
        auto nodeFactory = std::make_shared<NodeFactory>();
        auto xincludeProcessor = std::make_shared<XIncludeProcessor>();
        SCXMLParser parser(nodeFactory, xincludeProcessor);

        // Use parseContent method which exists in SCXMLParser
        model_ = parser.parseContent(scxmlContent);
        if (!model_) {
            LOG_ERROR("StateMachine: Failed to parse SCXML content");
            return false;
        }

        return initializeFromModel();
    } catch (const std::exception &e) {
        LOG_ERROR("Exception parsing SCXML content: {}", e.what());
        return false;
    }
}

bool StateMachine::start() {
    if (initialState_.empty()) {
        LOG_ERROR("StateMachine: Cannot start - no initial state defined");
        return false;
    }

    // JS 환경 초기화 보장
    if (!ensureJSEnvironment()) {
        LOG_ERROR("StateMachine: Cannot start - JavaScript environment initialization failed");
        return false;
    }

    LOG_DEBUG("Starting with initial state: {}", initialState_);

    // Check EventRaiser status at StateMachine start
    if (eventRaiser_) {
        LOG_DEBUG("StateMachine: EventRaiser status check - EventRaiser: {}, sessionId: {}", (void *)eventRaiser_.get(),
                  sessionId_);
    } else {
        LOG_WARN("StateMachine: EventRaiser is null - sessionId: {}", sessionId_);
    }

    // Set running state before entering initial state to handle immediate done.state events
    isRunning_ = true;

    // W3C SCXML 3.3: Support multiple initial states for parallel regions
    // W3C SCXML 3.2: If no initial attribute specified, use first state in document order
    const auto &modelInitialStates = model_->getInitialStates();
    std::vector<std::string> initialStates;

    if (modelInitialStates.empty()) {
        // W3C SCXML 3.2: No initial attribute - auto-select first state in document order
        const auto &allStates = model_->getAllStates();
        if (allStates.empty()) {
            LOG_ERROR("StateMachine: No states found in SCXML model");
            isRunning_ = false;
            return false;
        }

        initialStates.push_back(allStates[0]->getId());
        LOG_DEBUG("W3C SCXML 3.2: No initial attribute, auto-selected first state: '{}'", initialStates[0]);
    } else {
        // W3C SCXML 3.3: Use explicitly specified initial states
        initialStates = modelInitialStates;
    }

    // W3C SCXML: For initial state entry, add ancestor states to configuration first
    // This ensures ancestor onentry actions are executed (e.g., test 388 requires s0 onentry)
    if (model_ && hierarchyManager_) {
        // Collect all unique ancestors from all initial states
        std::vector<std::string> ancestorChain;
        std::set<std::string> seenAncestors;

        for (const auto &initialStateId : initialStates) {
            auto stateNode = model_->findStateById(initialStateId);
            IStateNode *current = stateNode ? stateNode->getParent() : nullptr;

            std::vector<std::string> currentAncestors;
            while (current) {
                const std::string &ancestorId = current->getId();
                if (!ancestorId.empty() && seenAncestors.find(ancestorId) == seenAncestors.end()) {
                    currentAncestors.push_back(ancestorId);
                    seenAncestors.insert(ancestorId);
                }
                current = current->getParent();
            }

            // Reverse to get parent->child order
            std::reverse(currentAncestors.begin(), currentAncestors.end());

            // Merge into main ancestor chain
            for (const auto &ancestorId : currentAncestors) {
                if (std::find(ancestorChain.begin(), ancestorChain.end(), ancestorId) == ancestorChain.end()) {
                    ancestorChain.push_back(ancestorId);
                }
            }
        }

        // Add ancestors to configuration (without onentry yet)
        for (const auto &ancestorId : ancestorChain) {
            hierarchyManager_->addStateToConfigurationWithoutOnEntry(ancestorId);
            LOG_DEBUG("Added ancestor state to configuration: {}", ancestorId);
        }

        // Execute onentry for ancestors in order (parent to child)
        for (const auto &ancestorId : ancestorChain) {
            executeOnEntryActions(ancestorId);
            LOG_DEBUG("Executed onentry for ancestor state: {}", ancestorId);
        }
    }

    // W3C SCXML 3.3: Enter all initial states (supports parallel initial configuration)
    // RAII guard ensures flag is reset even on exception
    InitialConfigurationGuard guard(isEnteringInitialConfiguration_);

    for (const auto &initialStateId : initialStates) {
        if (!enterState(initialStateId)) {
            LOG_ERROR("Failed to enter initial state: {}", initialStateId);
            isRunning_ = false;
            return false;  // Guard destructor will reset isEnteringInitialConfiguration_
        }
        LOG_DEBUG("Entered initial state: {}", initialStateId);
    }

    // Guard destructor will automatically reset isEnteringInitialConfiguration_ to false

    // W3C SCXML 3.13: Macrostep execution order after initial state entry
    // Per W3C SCXML specification, invokes must only execute for states "entered and not exited":
    //
    // Execution sequence:
    // 1. Enter initial states (compound states → initial children via recursive entry)
    //    - Invokes are deferred during state entry (not executed yet)
    // 2. Check eventless transitions (states may exit before invokes execute - test 422)
    //    - Example: s11 has eventless transition to s12, s11 exits immediately
    // 3. Execute pending invokes (only for states still active after step 2)
    //    - Filter: invoke executes only if isStateActive(stateId) returns true
    // 4. Process queued events (invokes may raise internal events)
    // 5. Repeat eventless transition checks until stable configuration reached
    //
    // This order ensures W3C SCXML 3.13 compliance: "invokes execute in document order
    // in all states that have been entered (and not exited) since last macrostep"

    // W3C SCXML 3.13: Repeat eventless transitions until stable configuration reached
    // This is critical for parallel states where entering a parallel state may enable
    // new eventless transitions in its regions (e.g., test 448)
    int eventlessIterations = 0;
    const int MAX_EVENTLESS_ITERATIONS = 1000;
    while (checkEventlessTransitions()) {
        if (++eventlessIterations > MAX_EVENTLESS_ITERATIONS) {
            LOG_ERROR("StateMachine: checkEventlessTransitions exceeded max iterations ({}) - possible infinite loop",
                      MAX_EVENTLESS_ITERATIONS);
            break;
        }
        LOG_DEBUG("StateMachine: Eventless transition executed (iteration {})", eventlessIterations);
    }
    LOG_DEBUG("StateMachine: Reached stable configuration after {} eventless iterations", eventlessIterations);

    // W3C SCXML compliance: Execute deferred invokes after eventless transitions
    // Only states that remain active after eventless transitions should have invokes executed
    LOG_DEBUG("StateMachine: Executing pending invokes after eventless transitions for session: {}", sessionId_);
    executePendingInvokes();

    // W3C SCXML: Process all remaining queued events after initial state entry
    // This ensures the state machine reaches a stable state before returning,
    // eliminating the need for external callers to explicitly call processQueuedEvents()
    if (eventRaiser_) {
        auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
        if (eventRaiserImpl) {
            int iterations = 0;
            const int MAX_START_ITERATIONS = 1000;

            while (eventRaiserImpl->hasQueuedEvents()) {
                if (++iterations > MAX_START_ITERATIONS) {
                    LOG_ERROR("StateMachine: start() exceeded max iterations ({}) - possible infinite event loop",
                              MAX_START_ITERATIONS);
                    break;
                }

                LOG_DEBUG("StateMachine: Processing queued events after start (iteration {})", iterations);
                eventRaiserImpl->processQueuedEvents();

                // Check for eventless transitions after processing events
                checkEventlessTransitions();
            }

            if (iterations > 0) {
                LOG_DEBUG("StateMachine: All queued events processed after start ({} iterations)", iterations);
            }
        }
    }

    updateStatistics();

    LOG_INFO("StateMachine: Started successfully");
    return true;
}

void StateMachine::stop() {
    if (!isRunning_) {
        return;
    }

    LOG_DEBUG("StateMachine: Stopping state machine");

    // Use StateHierarchyManager to check current state
    std::string currentState = getCurrentState();
    if (!currentState.empty()) {
        exitState(currentState);
    }

    isRunning_ = false;
    // State management delegated to StateHierarchyManager
    if (hierarchyManager_) {
        hierarchyManager_->reset();
    }

    // Unregister from JSEngine
    RSM::JSEngine::instance().setStateMachine(nullptr, sessionId_);
    LOG_DEBUG("StateMachine: Unregistered from JSEngine");

    updateStatistics();
    LOG_INFO("StateMachine: Stopped");
}

StateMachine::TransitionResult StateMachine::processEvent(const std::string &eventName, const std::string &eventData) {
    // W3C SCXML 6.4: Check if there's an origin session ID from EventRaiser thread-local storage
    std::string originSessionId = EventRaiserImpl::getCurrentOriginSessionId();

    // W3C SCXML 5.10: Check if there's a send ID from EventRaiser thread-local storage (for error events)
    std::string sendId = EventRaiserImpl::getCurrentSendId();

    // W3C SCXML 5.10: Check if there's an invoke ID from EventRaiser thread-local storage (test 338)
    std::string invokeId = EventRaiserImpl::getCurrentInvokeId();

    // W3C SCXML 5.10: Check if there's an origin type from EventRaiser thread-local storage (test 253, 331, 352, 372)
    std::string originType = EventRaiserImpl::getCurrentOriginType();

    // Delegate to overload with originSessionId (may be empty for non-invoke events)
    return processEvent(eventName, eventData, originSessionId, sendId, invokeId, originType);
}

StateMachine::TransitionResult StateMachine::processEvent(const std::string &eventName, const std::string &eventData,
                                                          const std::string &originSessionId, const std::string &sendId,
                                                          const std::string &invokeId, const std::string &originType) {
    // W3C SCXML 5.10: Get event type from EventRaiser thread-local storage (test 331)
    std::string eventType = EventRaiserImpl::getCurrentEventType();
    if (!isRunning_) {
        LOG_WARN("StateMachine: Cannot process event - state machine not running");
        TransitionResult result;
        result.success = false;
        result.errorMessage = "State machine not running";
        return result;
    }

    // JS 환경 확인
    if (!jsEnvironmentReady_) {
        LOG_ERROR("StateMachine: Cannot process event - JavaScript environment not ready");
        TransitionResult result;
        result.success = false;
        result.errorMessage = "JavaScript environment not ready";
        return result;
    }

    LOG_DEBUG("StateMachine: Processing event: '{}' with data: '{}' in session: '{}', originSessionId: '{}'", eventName,
              eventData, sessionId_, originSessionId);

    // Set event processing flag with RAII for exception safety
    struct ProcessingEventGuard {
        bool &flag_;
        bool wasAlreadySet_;

        explicit ProcessingEventGuard(bool &flag) : flag_(flag), wasAlreadySet_(flag) {
            if (!wasAlreadySet_) {
                LOG_DEBUG("ProcessingEventGuard: Setting isProcessingEvent_ = true");
                flag_ = true;
            } else {
                LOG_DEBUG("ProcessingEventGuard: Already processing event (nested call)");
            }
        }

        ~ProcessingEventGuard() {
            if (!wasAlreadySet_) {
                LOG_DEBUG("ProcessingEventGuard: Setting isProcessingEvent_ = false");
                flag_ = false;
            } else {
                LOG_DEBUG("ProcessingEventGuard: Leaving isProcessingEvent_ = true (nested call)");
            }
        }

        // Delete copy constructor and assignment
        ProcessingEventGuard(const ProcessingEventGuard &) = delete;
        ProcessingEventGuard &operator=(const ProcessingEventGuard &) = delete;
    };

    ProcessingEventGuard eventGuard(isProcessingEvent_);

    // Count this event
    stats_.totalEvents++;

    // Store event data for access in guards/actions
    currentEventData_ = eventData;

    // W3C SCXML compliance: Set current event in ActionExecutor for _event context
    if (actionExecutor_) {
        auto actionExecutorImpl = std::dynamic_pointer_cast<ActionExecutorImpl>(actionExecutor_);
        if (actionExecutorImpl) {
            // W3C SCXML 5.10: Set current event with full metadata using EventMetadata structure
            EventMetadata metadata(eventName, eventData, eventType, sendId, invokeId, originType, originSessionId);
            actionExecutorImpl->setCurrentEvent(metadata);

            if (!sendId.empty() || !invokeId.empty() || !originType.empty() || !eventType.empty() ||
                !originSessionId.empty()) {
                LOG_DEBUG("StateMachine: Set current event in ActionExecutor - event: '{}', data: '{}', sendid: '{}', "
                          "invokeid: '{}', origintype: '{}', type: '{}', originSessionId: '{}'",
                          eventName, eventData, sendId, invokeId, originType, eventType, originSessionId);
            } else {
                LOG_DEBUG("StateMachine: Set current event in ActionExecutor - event: '{}', data: '{}'", eventName,
                          eventData);
            }
        }
    }

    // W3C SCXML Test 252: Filter events from cancelled invoke child sessions
    if (invokeExecutor_ && !originSessionId.empty()) {
        if (invokeExecutor_->shouldFilterCancelledInvokeEvent(originSessionId)) {
            LOG_DEBUG("StateMachine: Filtering event '{}' from cancelled invoke child session: {}", eventName,
                      originSessionId);
            return TransitionResult(false, getCurrentState(), getCurrentState(), eventName);
        }
    }

    // W3C SCXML 1.0 Section 6.4: Execute finalize handler before processing events from invoked children
    // According to W3C SCXML: "finalize markup runs BEFORE the event is processed"
    // The finalize handler is executed when an event arrives from an invoked child
    // and has access to _event.data to update parent variables before transition evaluation
    if (invokeExecutor_ && !originSessionId.empty()) {
        // W3C SCXML compliance: Use originSessionId to find the exact child that sent this event
        std::string finalizeScript = invokeExecutor_->getFinalizeScriptForChildSession(originSessionId);

        if (!finalizeScript.empty()) {
            LOG_DEBUG("StateMachine: Executing finalize handler BEFORE processing event '{}', script: '{}'", eventName,
                      finalizeScript);

            // W3C SCXML 6.4: Parse and execute finalize as SCXML executable content
            // Finalize contains elements like <assign>, <script>, <log>, etc.
            if (actionExecutor_) {
                try {
                    // Parse finalize SCXML content to extract assign actions
                    // Simple pattern: <assign location="var1" expr="_event.data.aParam"/>
                    std::regex assign_pattern("<assign location=\"([^\"]+)\" expr=\"([^\"]+)\"/>");
                    std::smatch match;
                    std::string content = finalizeScript;

                    while (std::regex_search(content, match, assign_pattern)) {
                        std::string location = match[1].str();
                        std::string expr = match[2].str();

                        LOG_DEBUG("StateMachine: Finalize assign - location: '{}', expr: '{}'", location, expr);

                        // Execute assignment: evaluate expression and assign to variable
                        auto exprFuture = JSEngine::instance().evaluateExpression(sessionId_, expr);
                        auto exprResult = exprFuture.get();

                        if (exprResult.isSuccess()) {
                            // Get the actual value from JSResult
                            const ScriptValue &value = exprResult.getInternalValue();
                            JSEngine::instance().setVariable(sessionId_, location, value);
                            LOG_DEBUG("StateMachine: Finalize assigned '{}' successfully", location);
                        } else {
                            LOG_WARN("StateMachine: Finalize expr evaluation failed: {}", exprResult.getErrorMessage());
                        }

                        content = match.suffix();
                    }

                    LOG_DEBUG("StateMachine: Finalize handler executed successfully for event '{}'", eventName);
                } catch (const std::exception &e) {
                    LOG_ERROR("StateMachine: Exception during finalize handler execution: {}", e.what());
                }
            } else {
                LOG_WARN("StateMachine: No ActionExecutor available for finalize execution");
            }
        }
    }

    // W3C SCXML 1.0 Section 6.4: Auto-forward external events to child invoke sessions
    if (invokeExecutor_) {
        auto autoForwardSessions = invokeExecutor_->getAutoForwardSessions(sessionId_);
        for (auto *childStateMachine : autoForwardSessions) {
            if (childStateMachine->isRunning()) {
                childStateMachine->processEvent(eventName, eventData);
            }
        }
    }

    // Find applicable transitions from SCXML model
    if (!model_) {
        LOG_ERROR("StateMachine: No SCXML model available");
        TransitionResult result;
        result.success = false;
        result.fromState = getCurrentState();
        result.eventName = eventName;
        result.errorMessage = "No SCXML model available";
        return result;
    }

    // SCXML W3C specification section 3.4: Handle parallel state event broadcasting
    std::string currentState = getCurrentState();
    auto currentStateNode = model_->findStateById(currentState);
    if (!currentStateNode) {
        LOG_DEBUG("Current state not found in model: {}", currentState);
        TransitionResult result;
        result.success = false;
        result.fromState = getCurrentState();
        result.eventName = eventName;
        result.errorMessage = "Current state not found in model";
        return result;
    }

    // SCXML W3C specification compliance: Process parallel state events according to standard priority
    if (currentStateNode->getType() == Type::PARALLEL) {
        auto parallelState = dynamic_cast<ConcurrentStateNode *>(currentStateNode);
        assert(parallelState && "SCXML violation: PARALLEL type state must be ConcurrentStateNode");

        LOG_DEBUG("Processing event '{}' for parallel state: {}", eventName, currentState);

        // SCXML W3C specification 3.13: Check transitions on the parallel state itself
        // Internal transitions (no target) execute actions but DON'T prevent region processing
        // External transitions (with target) exit the parallel state and return immediately
        auto stateTransitionResult = processStateTransitions(currentStateNode, eventName, eventData);
        if (stateTransitionResult.success) {
            // Check if this is an external transition (toState != fromState)
            if (stateTransitionResult.toState != stateTransitionResult.fromState) {
                // External transition: exit parallel state
                LOG_DEBUG("SCXML W3C: External transition from parallel state: {} -> {}",
                          stateTransitionResult.fromState, stateTransitionResult.toState);
                return stateTransitionResult;
            }
            // Internal transition: actions executed, continue to region processing
            LOG_DEBUG("SCXML W3C: Internal transition on parallel state {} (actions executed, continuing to regions)",
                      currentState);
        }

        // SCXML W3C specification 3.13: Removed region root state check (lines 457-471)
        // The old approach checked region root states with processStateTransitions() and returned early.
        // This violated W3C SCXML 3.13 because:
        // 1. It prevented proper event broadcasting to ALL regions
        // 2. It didn't handle transition preemption correctly (child > parent)
        // 3. It didn't respect document order for transition priority
        // 4. It didn't distinguish cross-region vs external transitions
        // Instead, use region->processEvent() below (lines 484-537) which properly implements SCXML 3.13

        // W3C SCXML 3.13: Broadcast event to ALL regions using processEventInAllRegions()
        // This ensures proper transition preemption, blocking, and external transition handling
        LOG_DEBUG("StateMachine: No transitions on parallel state or region children, broadcasting to all regions");

        // W3C SCXML 3.13: Disable immediate mode during parallel state event processing
        // RAII guard ensures restoration even if processEventInAllRegions() throws exception
        // This prevents re-entrancy: raised events must be queued, not processed immediately
        // Otherwise, one region's <raise> action can deactivate other regions before they compute their transitions
        std::vector<ConcurrentOperationResult> results;
        {
            ImmediateModeGuard guard(eventRaiser_, false);
            LOG_DEBUG("W3C SCXML 3.13: Disabled immediate mode for parallel state event processing");

            // Create EventDescriptor for SCXML-compliant event processing
            EventDescriptor event;
            event.eventName = eventName;
            event.data = eventData;

            // Broadcast event to all active regions (SCXML W3C mandated)
            // Exception safety: guard automatically restores immediate mode on scope exit
            results = parallelState->processEventInAllRegions(event);

            LOG_DEBUG("W3C SCXML 3.13: Immediate mode will be restored on scope exit");
        }  // RAII guard restores immediate mode here

        bool anyTransitionExecuted = false;
        std::vector<std::string> successfulTransitions;
        std::string externalTransitionTarget;
        std::string externalTransitionSource;

        // W3C SCXML 3.13: Process results from all regions
        // Check for external transitions and collect successful transitions
        for (const auto &result : results) {
            if (result.isSuccess) {
                anyTransitionExecuted = true;
                successfulTransitions.push_back(result.regionId + ": SUCCESS");
            }

            // W3C SCXML 3.13: Detect external transition (transition outside parallel state)
            // Take the FIRST external transition found (document order for preemption/blocking)
            if (!result.externalTransitionTarget.empty() && externalTransitionTarget.empty()) {
                LOG_DEBUG("External transition from region '{}': {} -> {}", result.regionId,
                          result.externalTransitionSource, result.externalTransitionTarget);
                externalTransitionTarget = result.externalTransitionTarget;
                externalTransitionSource = result.externalTransitionSource;
                anyTransitionExecuted = true;
            }
        }

        // W3C SCXML 3.13: Execute external transition if found
        if (!externalTransitionTarget.empty()) {
            LOG_DEBUG("Executing external transition from parallel state '{}' to '{}'", currentState,
                      externalTransitionTarget);

            // Check if target is a child of the current parallel state (cross-region transition)
            auto targetStateNode = model_->findStateById(externalTransitionTarget);
            bool isCrossRegion = false;

            if (targetStateNode) {
                auto targetParent = targetStateNode->getParent();
                isCrossRegion = (targetParent && targetParent->getId() == currentState);
            }

            // Exit parallel state and all its regions
            exitState(currentState);

            if (isCrossRegion) {
                // Cross-region transition: re-enter the parallel state to activate ALL regions
                LOG_INFO("W3C SCXML 3.13: Cross-region transition {} -> {}, re-entering parallel state {}",
                         externalTransitionSource, externalTransitionTarget, currentState);
                enterState(currentState);

                // W3C SCXML: Set executionContext for all regions after re-entry
                // This is critical for regions to execute transition actions correctly
                auto parallelStateNode = model_->findStateById(currentState);
                if (parallelStateNode && parallelStateNode->getType() == Type::PARALLEL) {
                    auto reenteredParallelState = dynamic_cast<ConcurrentStateNode *>(parallelStateNode);
                    if (reenteredParallelState) {
                        const auto &regions = reenteredParallelState->getRegions();
                        for (const auto &region : regions) {
                            if (region) {
                                // Dynamic cast to concrete ConcurrentRegion for setExecutionContext
                                auto concreteRegion = std::dynamic_pointer_cast<ConcurrentRegion>(region);
                                if (concreteRegion) {
                                    concreteRegion->setExecutionContext(executionContext_);
                                    LOG_DEBUG("StateMachine: Set execution context for region: {} after parallel state "
                                              "re-entry",
                                              region->getId());
                                }
                            }
                        }
                    }
                }
            } else {
                // True external transition: enter the target state
                LOG_INFO("W3C SCXML 3.13: External transition from parallel {}, entering target {}", currentState,
                         externalTransitionTarget);
                enterState(externalTransitionTarget);
            }

            stats_.totalTransitions++;

            TransitionResult externalResult;
            externalResult.success = true;
            externalResult.fromState = currentState;
            externalResult.toState = externalTransitionTarget;
            externalResult.eventName = eventName;
            return externalResult;
        }

        if (anyTransitionExecuted) {
            stats_.totalTransitions++;
            LOG_INFO("SCXML compliant parallel region processing succeeded. Transitions: [{}/{}]",
                     successfulTransitions.size(), results.size());

            // W3C SCXML 3.4: Check if all regions completed (reached final states)
            // This triggers done.state.{id} event generation
            bool allRegionsComplete = parallelState->areAllRegionsComplete();
            if (allRegionsComplete) {
                LOG_DEBUG("SCXML W3C: All parallel regions completed for state: {}", currentState);
            }

            // Invoke execution consolidated to key lifecycle points            // Return success with parallel state as
            // context
            TransitionResult finalResult;
            finalResult.success = true;
            finalResult.fromState = currentState;
            finalResult.toState = currentState;  // Parallel state remains active
            finalResult.eventName = eventName;
            return finalResult;
        } else {
            LOG_DEBUG("No transitions executed in any region for event: {}", eventName);
            stats_.failedTransitions++;
            TransitionResult result;
            result.success = false;
            result.fromState = getCurrentState();
            result.eventName = eventName;
            result.errorMessage = "No valid transitions found";
            return result;
        }
    }

    // Non-parallel state: SCXML W3C compliant hierarchical event processing
    // Process transitions in active state hierarchy (innermost to outermost)
    auto activeStates = hierarchyManager_->getActiveStates();

    LOG_DEBUG("SCXML hierarchical processing: Checking {} active states for event '{}'", activeStates.size(),
              eventName);

    // W3C SCXML: Process states from most specific (innermost) to least specific (outermost)
    // Optimization: Track checked states to avoid duplicate ancestor traversal
    std::unordered_set<std::string> checkedStates;

    for (auto it = activeStates.rbegin(); it != activeStates.rend(); ++it) {
        const std::string &stateId = *it;
        auto stateNode = model_->findStateById(stateId);
        if (!stateNode) {
            LOG_WARN("SCXML hierarchical processing: State node not found: {}", stateId);
            continue;
        }

        // W3C SCXML: Check transitions from innermost state to root
        // Skip already-checked ancestors to avoid duplicate processing
        IStateNode *currentNode = stateNode;
        while (currentNode) {
            const std::string &nodeId = currentNode->getId();

            // Skip if already checked (optimization for duplicate ancestor traversal)
            if (checkedStates.count(nodeId)) {
                break;
            }
            checkedStates.insert(nodeId);

            LOG_DEBUG("SCXML hierarchical processing: Checking state '{}' for transitions", nodeId);
            auto transitionResult = processStateTransitions(currentNode, eventName, eventData);
            if (transitionResult.success) {
                LOG_DEBUG("SCXML hierarchical processing: Transition found in state '{}': {} -> {}", nodeId,
                          transitionResult.fromState, transitionResult.toState);
                return transitionResult;
            }

            // Move to parent state
            currentNode = currentNode->getParent();
        }
    }

    // No transitions found in any active state
    LOG_DEBUG("SCXML hierarchical processing: No transitions found in any active state for event '{}'", eventName);
    stats_.failedTransitions++;

    TransitionResult result;
    result.success = false;
    result.fromState = getCurrentState();
    result.eventName = eventName;
    result.errorMessage = "No valid transitions found in active state hierarchy";
    return result;
}

StateMachine::TransitionResult StateMachine::processStateTransitions(IStateNode *stateNode,
                                                                     const std::string &eventName,
                                                                     const std::string &eventData) {
    // eventData available for future SCXML features (e.g., event.data access in guards/actions)
    (void)eventData;

    if (!stateNode) {
        TransitionResult result;
        result.success = false;
        result.fromState = getCurrentState();
        result.eventName = eventName;
        result.errorMessage = "Invalid state node";
        return result;
    }

    // SCXML W3C specification: Process transitions in document order
    const auto &transitions = stateNode->getTransitions();

    LOG_DEBUG("Checking {} transitions for event '{}' on state: {}", transitions.size(), eventName, stateNode->getId());

    // Execute first valid transition (SCXML W3C specification)
    for (const auto &transitionNode : transitions) {
        // W3C SCXML 3.12: A transition can have multiple event descriptors
        // The transition matches if at least one descriptor matches the event name
        const std::vector<std::string> &eventDescriptors = transitionNode->getEvents();

        // Check if this transition matches the event
        bool eventMatches = false;

        if (eventName.empty()) {
            // For eventless transitions, only consider transitions without event descriptors
            eventMatches = eventDescriptors.empty();
        } else {
            // W3C SCXML 3.12: Check if ANY descriptor matches the event
            constexpr size_t WILDCARD_SUFFIX_LEN = 2;  // Length of ".*"

            for (const auto &descriptor : eventDescriptors) {
                // Skip malformed/empty descriptors from parsing errors
                if (descriptor.empty()) {
                    continue;
                }

                // W3C SCXML 3.12: Wildcard "*" matches any event
                if (descriptor == "*") {
                    eventMatches = true;
                    break;
                }

                // W3C SCXML 3.12: Exact match
                if (descriptor == eventName) {
                    eventMatches = true;
                    break;
                }

                // W3C SCXML 3.12: Wildcard pattern "foo.*" matches "foo", "foo.bar", "foo.bar.baz"
                if (descriptor.ends_with(".*")) {
                    std::string prefix = descriptor.substr(0, descriptor.length() - WILDCARD_SUFFIX_LEN);
                    // Match if event is exactly the prefix OR starts with "prefix."
                    if (eventName == prefix || eventName.starts_with(prefix + ".")) {
                        eventMatches = true;
                        break;
                    }
                } else {
                    // W3C SCXML 3.12: Token-based prefix matching
                    // "foo" matches "foo.bar" but NOT "foobar"
                    if (eventName.starts_with(descriptor + ".")) {
                        eventMatches = true;
                        break;
                    }
                }
            }
        }

        if (!eventMatches) {
            continue;
        }

        const auto &targets = transitionNode->getTargets();

        // W3C SCXML: Internal transitions have no targets but should still execute
        bool isInternal = transitionNode->isInternal();
        if (targets.empty() && !isInternal) {
            LOG_DEBUG("StateMachine: Skipping transition with no targets (not internal)");
            continue;
        }

        std::string targetState = targets.empty() ? "" : targets[0];
        std::string condition = transitionNode->getGuard();

        // Performance optimization: Only build debug string when DEBUG logging is enabled
        if constexpr (SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG) {
            std::string eventDescStr;
            for (size_t i = 0; i < eventDescriptors.size(); ++i) {
                if (i > 0) {
                    eventDescStr += " ";
                }
                eventDescStr += eventDescriptors[i];
            }
            LOG_DEBUG("Checking transition: {} -> {} with condition: '{}' (events: '{}')", stateNode->getId(),
                      targetState, condition, eventDescStr);
        }

        bool conditionResult = condition.empty() || evaluateCondition(condition);
        LOG_DEBUG("Condition result: {}", conditionResult ? "true" : "false");

        if (conditionResult) {
            // W3C SCXML: The source state of the transition is the state that contains it
            // NOT getCurrentState() which may return a parallel state
            std::string fromState = stateNode->getId();

            // W3C SCXML: Internal transitions execute actions without exiting/entering states
            if (isInternal) {
                LOG_DEBUG("StateMachine: Executing internal transition actions (no state change)");
                const auto &actionNodes = transitionNode->getActionNodes();
                if (!actionNodes.empty()) {
                    executeActionNodes(actionNodes, false);
                }

                TransitionResult result;
                result.success = true;
                result.fromState = fromState;
                result.toState = fromState;  // Same state (internal transition)
                result.eventName = eventName;
                return result;
            }

            LOG_DEBUG("Executing SCXML compliant transition from {} to {}", fromState, targetState);

            // W3C SCXML: Compute and exit ALL states in the exit set (not just current state)
            // For a transition from source to target, we must exit all states from source
            // up to (but not including) the Lowest Common Ancestor (LCA) with the target
            std::vector<std::string> exitSet = computeExitSet(fromState, targetState);
            LOG_DEBUG("W3C SCXML: Exiting {} states for transition {} -> {}", exitSet.size(), fromState, targetState);

            // Exit states in the exit set (already in correct order: deepest first)
            for (const std::string &stateToExit : exitSet) {
                if (!exitState(stateToExit)) {
                    LOG_ERROR("Failed to exit state: {}", stateToExit);
                    TransitionResult result;
                    result.success = false;
                    result.fromState = fromState;
                    result.eventName = eventName;
                    result.errorMessage = "Failed to exit state: " + stateToExit;
                    return result;
                }
            }

            // Execute transition actions (SCXML W3C specification)
            // W3C compliance: Events raised in transition actions must be queued, not processed immediately
            const auto &actionNodes = transitionNode->getActionNodes();
            if (!actionNodes.empty()) {
                LOG_DEBUG("StateMachine: Executing transition actions (events will be queued)");
                // processEventsAfter=false: Don't process events yet, they will be handled in macrostep loop
                executeActionNodes(actionNodes, false);
            } else {
                LOG_DEBUG("StateMachine: No transition actions for this transition");
            }

            // Enter new state
            if (!enterState(targetState)) {
                LOG_ERROR("Failed to enter state: {}", targetState);
                TransitionResult result;
                result.success = false;
                result.fromState = fromState;
                result.toState = targetState;
                result.eventName = eventName;
                result.errorMessage = "Failed to enter state: " + targetState;
                return result;
            }

            updateStatistics();
            stats_.totalTransitions++;

            LOG_INFO("Successfully transitioned from {} to {}", fromState, targetState);

            // W3C SCXML compliance: Macrostep loop - check for eventless transitions
            // After a transition completes, we must check for eventless transitions
            // that may have been enabled by the state change. Repeat until no
            // eventless transitions are found. Queued events are processed by
            // processQueuedEvents() in FIFO order to maintain event ordering guarantees.
            if (eventRaiser_) {
                auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
                if (eventRaiserImpl) {
                    LOG_DEBUG("W3C SCXML: Starting macrostep loop after transition");

                    // W3C SCXML: Safety guard against infinite loops in malformed SCXML
                    // Typical SCXML should complete in far fewer iterations
                    const int MAX_MACROSTEP_ITERATIONS = 1000;
                    int iterations = 0;

                    while (true) {
                        if (++iterations > MAX_MACROSTEP_ITERATIONS) {
                            LOG_ERROR(
                                "W3C SCXML: Macrostep limit exceeded ({} iterations) - possible infinite loop in SCXML",
                                MAX_MACROSTEP_ITERATIONS);
                            LOG_ERROR("W3C SCXML: Check for circular eventless transitions in your SCXML document");
                            break;  // Safety exit
                        }

                        // W3C SCXML: Check for eventless transitions on all active states
                        bool eventlessTransitionExecuted = checkEventlessTransitions();

                        if (eventlessTransitionExecuted) {
                            LOG_DEBUG("W3C SCXML: Eventless transition executed, continuing macrostep");
                            continue;  // Loop back to check for more eventless transitions
                        }

                        // W3C SCXML: No eventless transitions found, exit macrostep
                        // Queued events will be processed by processQueuedEvents() in FIFO order
                        LOG_DEBUG("W3C SCXML: No eventless transitions, macrostep complete");
                        break;
                    }

                    LOG_DEBUG("W3C SCXML: Macrostep loop complete");
                }
            }

            // W3C SCXML compliance: Execute deferred invokes after macrostep completes
            executePendingInvokes();

            return TransitionResult(true, fromState, targetState, eventName);
        }
    }

    // No valid transitions found
    LOG_DEBUG("No valid transitions found for event: {} from state: {}", eventName, stateNode->getId());

    // Note: Failed transition counter is managed at processEvent() level to avoid double counting

    TransitionResult result;
    result.success = false;
    result.fromState = getCurrentState();
    result.eventName = eventName;
    result.errorMessage = "No valid transitions found";
    return result;
}

std::string StateMachine::getCurrentState() const {
    // W3C SCXML: Thread safety for JSEngine worker thread access
    std::lock_guard<std::mutex> lock(hierarchyManagerMutex_);

    if (!hierarchyManager_) {
        return "";
    }

    return hierarchyManager_->getCurrentState();
}

std::vector<std::string> StateMachine::getActiveStates() const {
    // W3C SCXML: Thread safety for JSEngine worker thread access
    std::lock_guard<std::mutex> lock(hierarchyManagerMutex_);

    if (!hierarchyManager_) {
        return {};
    }

    return hierarchyManager_->getActiveStates();
}

bool StateMachine::isRunning() const {
    return isRunning_;
}

bool StateMachine::isStateActive(const std::string &stateId) const {
    // W3C SCXML: Thread safety for JSEngine worker thread access
    std::lock_guard<std::mutex> lock(hierarchyManagerMutex_);

    if (!hierarchyManager_) {
        return false;
    }
    return hierarchyManager_->isStateActive(stateId);
}

bool StateMachine::isStateInFinalState(const std::string &stateId) const {
    if (!model_) {
        LOG_DEBUG("StateMachine::isStateInFinalState: No model available");
        return false;
    }

    if (stateId.empty()) {
        LOG_DEBUG("StateMachine::isStateInFinalState: State ID is empty");
        return false;
    }

    auto state = model_->findStateById(stateId);
    bool isFinal = state && state->isFinalState();
    LOG_DEBUG("StateMachine::isStateInFinalState: stateId='{}', state found: {}, isFinalState: {}", stateId,
              (void *)state, isFinal);
    return isFinal;
}

bool StateMachine::isInFinalState() const {
    if (!isRunning_) {
        LOG_DEBUG("StateMachine::isInFinalState: State machine is not running");
        return false;
    }

    return isStateInFinalState(getCurrentState());
}

bool StateMachine::isInitialStateFinal() const {
    return isStateInFinalState(model_ ? model_->getInitialState() : "");
}

std::string StateMachine::getCurrentEventData() const {
    return currentEventData_;
}

const std::string &StateMachine::getSessionId() const {
    return sessionId_;
}

std::shared_ptr<SCXMLModel> StateMachine::getModel() const {
    return model_;
}

StateMachine::Statistics StateMachine::getStatistics() const {
    return stats_;
}

// W3C SCXML 5.3: Collect all data items from document for global scope initialization
std::vector<StateMachine::DataItemInfo> StateMachine::collectAllDataItems() const {
    std::vector<DataItemInfo> allDataItems;

    if (!model_) {
        return allDataItems;
    }

    // Collect top-level datamodel items
    const auto &topLevelItems = model_->getDataModelItems();
    for (const auto &item : topLevelItems) {
        allDataItems.push_back(DataItemInfo{"", item});  // Empty stateId for top-level
    }
    LOG_DEBUG("StateMachine: Collected {} top-level data items", topLevelItems.size());

    // Collect state-level data items from all states
    const auto &allStates = model_->getAllStates();
    for (const auto &state : allStates) {
        if (!state) {
            continue;
        }

        const auto &stateDataItems = state->getDataItems();
        if (!stateDataItems.empty()) {
            for (const auto &item : stateDataItems) {
                allDataItems.push_back(DataItemInfo{state->getId(), item});
            }
            LOG_DEBUG("StateMachine: Collected {} data items from state '{}'", stateDataItems.size(), state->getId());
        }
    }

    LOG_INFO("StateMachine: Total data items collected: {} (for global scope initialization)", allDataItems.size());
    return allDataItems;
}

// W3C SCXML 5.3: Initialize a single data item with binding mode support
void StateMachine::initializeDataItem(const std::shared_ptr<IDataModelItem> &item, bool assignValue) {
    if (!item) {
        return;
    }

    std::string id = item->getId();
    std::string expr = item->getExpr();
    std::string src = item->getSrc();
    std::string content = item->getContent();

    // W3C SCXML 6.4: Check if variable was pre-initialized (e.g., by invoke namelist/param)
    if (RSM::JSEngine::instance().isVariablePreInitialized(sessionId_, id)) {
        LOG_INFO("StateMachine: Skipping initialization for '{}' - pre-initialized by invoke data", id);
        return;
    }

    if (!assignValue) {
        // Late binding: Create variable but don't assign value yet (leave undefined)
        auto setVarFuture = RSM::JSEngine::instance().setVariable(sessionId_, id, ScriptValue{});
        auto setResult = setVarFuture.get();

        if (!RSM::JSEngine::isSuccess(setResult)) {
            LOG_ERROR("StateMachine: Failed to create unbound variable '{}': {}", id, setResult.getErrorMessage());
            if (eventRaiser_) {
                eventRaiser_->raiseEvent("error.execution",
                                         "Failed to create variable '" + id + "': " + setResult.getErrorMessage());
            }
            return;
        }

        LOG_DEBUG("StateMachine: Created unbound variable '{}' for late binding", id);
        return;
    }

    // Early binding or late binding value assignment: Evaluate and assign
    if (!expr.empty()) {
        // W3C SCXML B.2: For function expressions, use direct JavaScript assignment to preserve function type
        // Test 453: ECMAScript function literals must be stored as functions, not converted to C++
        bool isFunctionExpression = (expr.find("function") == 0);

        if (isFunctionExpression) {
            // Use direct JavaScript assignment to avoid function → C++ → function conversion loss
            std::string assignmentScript = id + " = " + expr;
            auto scriptFuture = RSM::JSEngine::instance().executeScript(sessionId_, assignmentScript);
            auto scriptResult = scriptFuture.get();

            if (!RSM::JSEngine::isSuccess(scriptResult)) {
                LOG_ERROR("StateMachine: Failed to assign function expression '{}' to variable '{}': {}", expr, id,
                          scriptResult.getErrorMessage());
                if (eventRaiser_) {
                    eventRaiser_->raiseEvent("error.execution", "Failed to assign function expression for '" + id +
                                                                    "': " + scriptResult.getErrorMessage());
                }
                return;
            }
            LOG_DEBUG("StateMachine: Initialized function variable '{}' from expression '{}'", id, expr);
        } else {
            // Standard evaluation for non-function expressions
            auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, expr);
            auto result = future.get();

            if (RSM::JSEngine::isSuccess(result)) {
                auto setVarFuture = RSM::JSEngine::instance().setVariable(sessionId_, id, result.getInternalValue());
                auto setResult = setVarFuture.get();

                if (!RSM::JSEngine::isSuccess(setResult)) {
                    LOG_ERROR("StateMachine: Failed to set variable '{}' from expression '{}': {}", id, expr,
                              setResult.getErrorMessage());
                    if (eventRaiser_) {
                        eventRaiser_->raiseEvent("error.execution", "Failed to set variable '" + id +
                                                                        "' from expression '" + expr +
                                                                        "': " + setResult.getErrorMessage());
                    }
                    return;
                }

                LOG_DEBUG("StateMachine: Initialized variable '{}' from expression '{}'", id, expr);
            } else {
                LOG_ERROR("StateMachine: Failed to evaluate expression '{}' for variable '{}': {}", expr, id,
                          result.getErrorMessage());
                // W3C SCXML 5.3: On evaluation error, raise error.execution event and create unbound variable
                if (eventRaiser_) {
                    eventRaiser_->raiseEvent("error.execution", "Failed to evaluate data expression for '" + id +
                                                                    "': " + result.getErrorMessage());
                }
                // Leave variable unbound (don't create it) so it can be assigned later
                return;
            }
        }
    } else if (!src.empty()) {
        // W3C SCXML 5.3: Load data from external source (test 446)
        std::string filePath = src;

        // Remove "file:" prefix if present
        if (filePath.find("file:") == 0) {
            filePath = filePath.substr(5);
        }

        // Resolve relative path based on SCXML file location
        if (filePath[0] != '/') {  // Relative path
            std::string scxmlFilePath = RSM::JSEngine::instance().getSessionFilePath(sessionId_);
            if (!scxmlFilePath.empty()) {
                // Extract directory from SCXML file path
                size_t lastSlash = scxmlFilePath.find_last_of("/");
                if (lastSlash != std::string::npos) {
                    std::string directory = scxmlFilePath.substr(0, lastSlash + 1);
                    filePath = directory + filePath;
                }
            }
        }

        // Read file content
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG_ERROR("StateMachine: Failed to open file '{}' for variable '{}'", filePath, id);
            if (eventRaiser_) {
                eventRaiser_->raiseEvent("error.execution",
                                         "Failed to open file '" + filePath + "' for variable '" + id + "'");
            }
            return;
        }

        std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Evaluate file content as JavaScript expression
        auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, fileContent);
        auto result = future.get();

        if (RSM::JSEngine::isSuccess(result)) {
            auto setVarFuture = RSM::JSEngine::instance().setVariable(sessionId_, id, result.getInternalValue());
            auto setResult = setVarFuture.get();

            if (!RSM::JSEngine::isSuccess(setResult)) {
                LOG_ERROR("StateMachine: Failed to set variable '{}' from file '{}': {}", id, filePath,
                          setResult.getErrorMessage());
                if (eventRaiser_) {
                    eventRaiser_->raiseEvent("error.execution", "Failed to set variable '" + id + "' from file '" +
                                                                    filePath + "': " + setResult.getErrorMessage());
                }
                return;
            }

            LOG_DEBUG("StateMachine: Initialized variable '{}' from file '{}'", id, filePath);
        } else {
            LOG_ERROR("StateMachine: Failed to evaluate content from file '{}' for variable '{}': {}", filePath, id,
                      result.getErrorMessage());
            if (eventRaiser_) {
                eventRaiser_->raiseEvent("error.execution", "Failed to evaluate file content for '" + id +
                                                                "': " + result.getErrorMessage());
            }
            return;
        }
    } else if (!content.empty()) {
        auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, content);
        auto result = future.get();

        if (RSM::JSEngine::isSuccess(result)) {
            auto setVarFuture = RSM::JSEngine::instance().setVariable(sessionId_, id, result.getInternalValue());
            auto setResult = setVarFuture.get();

            if (!RSM::JSEngine::isSuccess(setResult)) {
                LOG_ERROR("StateMachine: Failed to set variable '{}' from content: {}", id,
                          setResult.getErrorMessage());
                if (eventRaiser_) {
                    eventRaiser_->raiseEvent("error.execution", "Failed to set variable '" + id +
                                                                    "' from content: " + setResult.getErrorMessage());
                }
                return;
            }

            LOG_DEBUG("StateMachine: Initialized variable '{}' from content", id);
        } else {
            // Try setting content as string literal
            auto setVarFuture = RSM::JSEngine::instance().setVariable(sessionId_, id, ScriptValue{content});
            auto setResult = setVarFuture.get();

            if (!RSM::JSEngine::isSuccess(setResult)) {
                LOG_ERROR("StateMachine: Failed to set variable '{}' as string literal: {}", id,
                          setResult.getErrorMessage());
                if (eventRaiser_) {
                    eventRaiser_->raiseEvent("error.execution",
                                             "Failed to set variable '" + id +
                                                 "' as string literal: " + setResult.getErrorMessage());
                }
                return;
            }

            LOG_DEBUG("StateMachine: Set variable '{}' as string literal from content", id);
        }
    } else {
        // W3C SCXML 5.3: No expression or content - create variable with undefined value (test 445)
        auto setVarFuture = RSM::JSEngine::instance().setVariable(sessionId_, id, ScriptValue{});
        auto setResult = setVarFuture.get();

        if (!RSM::JSEngine::isSuccess(setResult)) {
            LOG_ERROR("StateMachine: Failed to create undefined variable '{}': {}", id, setResult.getErrorMessage());
            if (eventRaiser_) {
                eventRaiser_->raiseEvent("error.execution",
                                         "Failed to create variable '" + id + "': " + setResult.getErrorMessage());
            }
            return;
        }

        LOG_DEBUG("StateMachine: Created variable '{}' with undefined value", id);
    }
}

bool StateMachine::initializeFromModel() {
    LOG_DEBUG("StateMachine: Initializing from SCXML model");

    // Clear existing state
    initialState_.clear();

    // Get initial state
    initialState_ = model_->getInitialState();

    // W3C SCXML 3.2: If no initial attribute, use first state in document order
    if (initialState_.empty()) {
        const auto &allStates = model_->getAllStates();
        if (allStates.empty()) {
            LOG_ERROR("StateMachine: No states found in SCXML model");
            return false;
        }

        // Auto-select first state in document order (W3C SCXML 3.2 compliance)
        initialState_ = allStates[0]->getId();
        LOG_DEBUG("StateMachine: No initial attribute found, auto-selected first state in document order: '{}'",
                  initialState_);
    }

    // Extract all states from the model
    const auto &allStates = model_->getAllStates();
    if (allStates.empty()) {
        LOG_ERROR("StateMachine: No states found in SCXML model");
        return false;
    }

    try {
        // Initialize hierarchy manager for hierarchical state support
        hierarchyManager_ = std::make_unique<StateHierarchyManager>(model_);

        // Set up onentry callback for W3C SCXML compliance
        LOG_DEBUG("StateMachine: Setting up onentry callback for StateHierarchyManager");
        hierarchyManager_->setOnEntryCallback([this](const std::string &stateId) {
            LOG_DEBUG("StateMachine: Onentry callback triggered for state: {}", stateId);
            executeOnEntryActions(stateId);
        });
        LOG_DEBUG("StateMachine: Onentry callback successfully configured");

        // W3C SCXML 6.4: Set up invoke defer callback for proper timing in parallel states
        LOG_DEBUG("StateMachine: Setting up invoke defer callback for StateHierarchyManager");
        hierarchyManager_->setInvokeDeferCallback(
            [this](const std::string &stateId, const std::vector<std::shared_ptr<IInvokeNode>> &invokes) {
                LOG_DEBUG("StateMachine: Invoke defer callback triggered for state: {} with {} invokes", stateId,
                          invokes.size());
                deferInvokeExecution(stateId, invokes);
            });
        LOG_DEBUG("StateMachine: Invoke defer callback successfully configured");

        // W3C SCXML: Set up condition evaluator callback for transition guard evaluation in parallel states
        LOG_DEBUG("StateMachine: Setting up condition evaluator callback for StateHierarchyManager");
        hierarchyManager_->setConditionEvaluator(
            [this](const std::string &condition) -> bool { return evaluateCondition(condition); });
        LOG_DEBUG("StateMachine: Condition evaluator callback successfully configured");

        // Set up completion callbacks for parallel states (SCXML W3C compliance)
        setupParallelStateCallbacks();

        // SCXML W3C Section 3.6: Auto-register history states from parsed model (SOLID architecture)
        initializeHistoryAutoRegistrar();
        if (historyAutoRegistrar_) {
            historyAutoRegistrar_->autoRegisterHistoryStates(model_, historyManager_.get());
        }

        LOG_DEBUG("Model initialized with initial state: {}", initialState_);
        LOG_INFO("Model initialized with {} states", allStates.size());
        return true;
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to extract model: {}", e.what());
        return false;
    }
}

bool StateMachine::evaluateCondition(const std::string &condition) {
    if (condition.empty()) {
        LOG_DEBUG("Empty condition, returning true");
        return true;
    }

    try {
        LOG_DEBUG("Evaluating condition: '{}'", condition);

        auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, condition);
        auto result = future.get();

        if (!RSM::JSEngine::isSuccess(result)) {
            // W3C SCXML 5.9: Condition evaluation error must raise error.execution
            LOG_ERROR("W3C SCXML 5.9: Failed to evaluate condition '{}': {}", condition, result.getErrorMessage());

            if (eventRaiser_) {
                eventRaiser_->raiseEvent("error.execution", "Failed to evaluate condition: " + condition);
            }
            return false;
        }

        // Convert result to boolean using integrated JSEngine method
        bool conditionResult = RSM::JSEngine::resultToBool(result);
        LOG_DEBUG("Condition '{}' evaluated to: {}", condition, conditionResult ? "true" : "false");

        return conditionResult;

    } catch (const std::exception &e) {
        // W3C SCXML 5.9: Exception during condition evaluation must raise error.execution
        LOG_ERROR("W3C SCXML 5.9: Exception evaluating condition '{}': {}", condition, e.what());

        if (eventRaiser_) {
            eventRaiser_->raiseEvent("error.execution", "Exception evaluating condition: " + condition);
        }
        return false;
    }
}

bool StateMachine::enterState(const std::string &stateId) {
    LOG_DEBUG("Entering state: {}", stateId);

    // RAII guard against invalid reentrant calls
    // Automatically handles legitimate reentrant calls during event processing
    EnterStateGuard guard(isEnteringState_, isProcessingEvent_);

    // Early return for invalid reentrant calls (matches original behavior)
    if (guard.isInvalidCall()) {
        LOG_DEBUG("Invalid reentrant enterState call detected, ignoring: {}", stateId);
        return true;  // Return success to avoid breaking transition chain
    }

    // Check if this is a history state and handle restoration (SCXML W3C specification section 3.6)
    if (historyManager_ && historyManager_->isHistoryState(stateId)) {
        LOG_INFO("Entering history state: {}", stateId);

        // W3C SCXML 3.10: Restore history configuration and enter target states with ancestors
        auto restorationResult = historyManager_->restoreHistory(stateId);
        if (restorationResult.success && !restorationResult.targetStateIds.empty()) {
            LOG_INFO("History restoration successful, entering {} target states",
                     restorationResult.targetStateIds.size());

            // Release guard before entering target states (allows recursive enterState calls)
            guard.release();

            // Enter all target states from history restoration
            // Use enterStateWithAncestors to ensure parent states are entered (test 387)
            bool allSucceeded = true;
            for (const auto &targetStateId : restorationResult.targetStateIds) {
                if (hierarchyManager_) {
                    // Enter target state along with all its ancestors
                    if (!hierarchyManager_->enterStateWithAncestors(targetStateId, nullptr)) {
                        LOG_ERROR("Failed to enter restored target state with ancestors: {}", targetStateId);
                        allSucceeded = false;
                    }
                } else {
                    // Fallback: use regular enterState if hierarchyManager not available
                    if (!enterState(targetStateId)) {
                        LOG_ERROR("Failed to enter restored target state: {}", targetStateId);
                        allSucceeded = false;
                    }
                }
            }
            return allSucceeded;
        } else {
            LOG_ERROR("History restoration failed: {}", restorationResult.errorMessage);
            // Guard will auto-clear on scope exit
            return false;
        }
    }

    // SCXML W3C specification: hierarchy manager is required for compliant state entry
    assert(hierarchyManager_ && "SCXML violation: hierarchy manager required for state management");

    // W3C SCXML 5.3: Late binding - assign values to state's data items when state is entered
    if (model_) {
        const std::string &binding = model_->getBinding();
        bool isLateBinding = (binding == "late");

        if (isLateBinding && initializedStates_.find(stateId) == initializedStates_.end()) {
            // Late binding: Assign values to this state's data items now (on first entry)
            auto stateNode = model_->findStateById(stateId);
            if (stateNode) {
                const auto &stateDataItems = stateNode->getDataItems();
                if (!stateDataItems.empty()) {
                    LOG_DEBUG("StateMachine: Late binding - assigning values to {} data items for state '{}'",
                              stateDataItems.size(), stateId);
                    for (const auto &item : stateDataItems) {
                        initializeDataItem(item, true);  // assignValue=true
                    }
                    initializedStates_.insert(stateId);  // Mark state as initialized
                }
            }
        }
    }

    bool hierarchyResult = hierarchyManager_->enterState(stateId);
    assert(hierarchyResult && "SCXML violation: state entry must succeed");
    (void)hierarchyResult;  // Suppress unused variable warning in release builds

    // SCXML W3C 3.4: For parallel states, activate regions AFTER parent onentry executed
    // This ensures correct entry sequence: parallel onentry -> child onentry
    if (model_) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode && stateNode->getType() == Type::PARALLEL) {
            auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
            if (parallelState) {
                // Set ExecutionContext for region action execution
                if (executionContext_) {
                    parallelState->setExecutionContextForRegions(executionContext_);
                    LOG_DEBUG("SCXML compliant: Injected ExecutionContext into parallel state regions: {}", stateId);
                }

                // W3C SCXML 3.4: Activate all regions AFTER parallel state entered
                auto activationResults = parallelState->activateAllRegions();
                for (const auto &result : activationResults) {
                    if (!result.isSuccess) {
                        LOG_ERROR("Failed to activate region '{}': {}", result.regionId, result.errorMessage);
                    } else {
                        LOG_DEBUG("SCXML W3C: Activated region '{}' in parallel state '{}'", result.regionId, stateId);
                    }
                }

                // Check if all regions immediately reached final state (for done.state event)
                const auto &regions = parallelState->getRegions();
                bool allInFinalState =
                    !regions.empty() && std::all_of(regions.begin(), regions.end(), [](const auto &region) {
                        return region && region->isInFinalState();
                    });

                if (allInFinalState) {
                    LOG_DEBUG("SCXML W3C 3.4: All parallel regions in final state, triggering done.state event for {}",
                              stateId);
                    handleParallelStateCompletion(stateId);
                }
            }
        }
    }

    // SCXML W3C macrostep compliance: Check if reentrant transition occurred during state entry
    // This handles cases where onentry actions cause immediate transitions
    std::string actualCurrentState = getCurrentState();
    LOG_DEBUG("StateMachine: After entering '{}', getCurrentState() returns '{}'", stateId, actualCurrentState);
    if (actualCurrentState != stateId) {
        LOG_DEBUG("SCXML macrostep: State transition occurred during entry (expected: {}, actual: {})", stateId,
                  actualCurrentState);
        LOG_DEBUG("This indicates a valid internal transition (e.g., compound state entering initial child) - must "
                  "check eventless");

        // W3C SCXML 3.7: Check if actualCurrentState is a final state and generate done.state event
        // This handles compound states with initial attribute pointing to final child (test 372)
        if (model_) {
            auto currentStateNode = model_->findStateById(actualCurrentState);
            if (currentStateNode && currentStateNode->isFinalState()) {
                LOG_DEBUG("W3C SCXML 3.7: Current state '{}' is final, generating done.state event before early return",
                          actualCurrentState);
                handleCompoundStateFinalChild(actualCurrentState);
            }
        }

        // IMPORTANT: Release guard before checking eventless transitions
        guard.release();

        // W3C SCXML 3.3: Skip eventless transition check during initial configuration entry
        // This prevents premature transitions before all initial states are entered
        if (!isEnteringInitialConfiguration_) {
            // W3C SCXML: Check eventless transitions even on early return (initial child may have eventless
            // transitions)
            checkEventlessTransitions();
        }
        return true;
    }

    // W3C SCXML: onentry actions (including invokes) are executed via callback from StateHierarchyManager
    // This ensures proper execution order per W3C specification

    // NOTE: _state is not a W3C SCXML standard system variable (only _event, _sessionid, _name, _ioprocessors, _x
    // exist) Setting _state here causes issues with invoke lifecycle when child sessions terminate Removed to comply
    // with W3C SCXML 5.10 specification

    LOG_DEBUG("Successfully entered state using hierarchy manager: {} (current: {})", stateId, getCurrentState());

    // W3C SCXML 6.5: Check for top-level final state and invoke completion callback
    // IMPORTANT: Only for invoked child StateMachines, not for parallel regions
    if (model_ && completionCallback_) {
        auto stateNode = model_->findStateById(actualCurrentState);
        if (stateNode && stateNode->isFinalState()) {
            // Check if this is a top-level final state by checking parent chain
            // Top-level states have no parent or parent is the <scxml> root element
            // We need to traverse up to ensure we're not in a parallel region
            auto parent = stateNode->getParent();
            bool isTopLevel = false;

            if (!parent) {
                // No parent means root-level final state
                isTopLevel = true;
            } else {
                // Check if parent is <scxml> root or if we're in a direct child of <scxml>
                // Parallel regions have intermediate parent states, so we need to check the entire chain
                auto grandparent = parent->getParent();
                if (!grandparent || parent->getId() == "scxml") {
                    // Parent is root or state is direct child of root
                    isTopLevel = true;
                } else if (grandparent->getId() == "scxml" && parent->getType() != RSM::Type::PARALLEL) {
                    // Grandparent is root and parent is NOT a parallel state
                    // This means we're a final state in a compound/atomic state at root level
                    isTopLevel = true;
                }
                // If parent is PARALLEL or we're deeper in the hierarchy, this is NOT top-level
            }

            if (isTopLevel) {
                LOG_INFO("StateMachine: Reached top-level final state: {}, executing onexit then completion callback",
                         actualCurrentState);

                // W3C SCXML: Execute onexit actions BEFORE generating done.invoke
                // For top-level final states, onexit runs when state machine completes
                bool exitResult = executeExitActions(actualCurrentState);
                if (!exitResult) {
                    LOG_WARN("StateMachine: Failed to execute onexit for final state: {}", actualCurrentState);
                }

                // IMPORTANT: Callback is invoked AFTER onexit handlers execute
                // This ensures correct event order: child events → done.invoke
                if (completionCallback_) {
                    try {
                        completionCallback_();
                    } catch (const std::exception &e) {
                        LOG_ERROR("StateMachine: Exception in completion callback: {}", e.what());
                    }
                }
            }
        }
    }

    // W3C SCXML 3.7 & 5.5: Generate done.state event for compound state completion
    if (model_) {
        auto stateNode = model_->findStateById(actualCurrentState);
        if (stateNode && stateNode->isFinalState()) {
            handleCompoundStateFinalChild(actualCurrentState);
        }
    }

    // Release guard - state entry complete
    guard.release();

    // W3C SCXML: Check for eventless transitions after state entry
    checkEventlessTransitions();

    return true;
}

bool StateMachine::executeTransitionDirect(IStateNode *sourceState, std::shared_ptr<ITransitionNode> transition) {
    if (!sourceState || !transition) {
        LOG_ERROR("StateMachine: Invalid parameters for executeTransitionDirect");
        return false;
    }

    // Execute the transition directly without re-evaluating its condition
    // This avoids side effects from conditions with mutations (e.g., ++var1 in W3C test 444)
    const auto &targets = transition->getTargets();
    bool isInternal = transition->isInternal();

    if (targets.empty() && !isInternal) {
        LOG_DEBUG("SCXML: Skipping transition with no targets (not internal)");
        return false;
    }

    std::string targetState = targets.empty() ? "" : targets[0];
    std::string fromState = sourceState->getId();

    // W3C SCXML: Internal transitions execute actions without exiting/entering states
    if (isInternal) {
        LOG_DEBUG("SCXML: Executing internal eventless transition actions (no state change)");
        const auto &actionNodes = transition->getActionNodes();
        if (!actionNodes.empty()) {
            if (!executeActionNodes(actionNodes, false)) {
                LOG_ERROR("StateMachine: Failed to execute internal transition actions");
                return false;
            }
        }
        return true;
    }

    // W3C SCXML: Compute and exit ALL states in the exit set
    std::vector<std::string> exitSet = computeExitSet(fromState, targetState);
    LOG_DEBUG("W3C SCXML: Exiting {} states for eventless transition {} -> {}", exitSet.size(), fromState, targetState);

    for (const std::string &stateToExit : exitSet) {
        if (!exitState(stateToExit)) {
            LOG_ERROR("Failed to exit state: {}", stateToExit);
            return false;
        }
    }

    // Execute transition actions
    const auto &actionNodes = transition->getActionNodes();
    if (!actionNodes.empty()) {
        LOG_DEBUG("SCXML: Executing eventless transition actions");
        if (!executeActionNodes(actionNodes, false)) {
            LOG_ERROR("StateMachine: Failed to execute transition actions");
            return false;
        }
    }

    // Enter new state
    if (!enterState(targetState)) {
        LOG_ERROR("Failed to enter state: {}", targetState);
        return false;
    }

    updateStatistics();
    stats_.totalTransitions++;

    LOG_DEBUG("SCXML: Eventless transition executed: {} -> {}", fromState, targetState);
    return true;
}

bool StateMachine::checkEventlessTransitions() {
    // W3C SCXML 3.13: Eventless Transition Selection Algorithm
    //
    // 1. For each active state (reverse document order):
    //    a. Find first enabled eventless transition (document order)
    //    b. Check if state is within a parallel state
    //    c. If parallel: collect transitions from ALL parallel regions (microstep)
    //    d. If not: execute single transition immediately
    // 2. Execute collected transitions atomically (exit all → execute all → enter all)
    //
    // Key Rule: Only the FIRST enabled transition per state is selected
    // Internal transitions count as "first" and prevent further checking

    if (!model_) {
        return false;
    }

    auto activeStates = hierarchyManager_->getActiveStates();
    LOG_DEBUG("SCXML: Checking eventless transitions on {} active state(s)", activeStates.size());

    // Performance: Cache state lookups to avoid repeated O(n) searches
    std::unordered_map<std::string, IStateNode *> stateCache;
    for (const auto &stateId : activeStates) {
        stateCache[stateId] = model_->findStateById(stateId);
    }

    IStateNode *firstEnabledState = nullptr;
    std::shared_ptr<ITransitionNode> firstTransition = nullptr;
    IStateNode *parallelAncestor = nullptr;

    // Find first enabled eventless transition
    for (auto it = activeStates.rbegin(); it != activeStates.rend(); ++it) {
        const std::string &activeStateId = *it;
        auto stateNode = stateCache[activeStateId];

        if (!stateNode) {
            continue;
        }

        const auto &transitions = stateNode->getTransitions();
        for (const auto &transitionNode : transitions) {
            const std::vector<std::string> &eventDescriptors = transitionNode->getEvents();
            if (!eventDescriptors.empty()) {
                continue;  // Not eventless
            }

            std::string condition = transitionNode->getGuard();
            bool conditionResult = condition.empty() || evaluateCondition(condition);

            if (conditionResult) {
                firstEnabledState = stateNode;
                firstTransition = transitionNode;

                // Check if this state is within a parallel state
                IStateNode *current = stateNode->getParent();
                while (current) {
                    if (current->getType() == Type::PARALLEL) {
                        parallelAncestor = current;
                        break;
                    }
                    current = current->getParent();
                }

                break;
            }
        }

        if (firstEnabledState) {
            break;
        }
    }

    if (!firstEnabledState) {
        LOG_DEBUG("SCXML: No eventless transitions found");
        return false;
    }

    // W3C SCXML 3.13: If not in parallel state, execute the already-selected transition
    // IMPORTANT: We already evaluated the condition, so we must not re-evaluate it
    // to avoid side effects (e.g., ++var1 would increment twice - W3C test 444)
    if (!parallelAncestor) {
        LOG_DEBUG("SCXML: Single eventless transition (non-parallel)");
        return executeTransitionDirect(firstEnabledState, firstTransition);
    }

    // W3C SCXML 3.13: Parallel state - collect ALL eventless transitions from all regions
    // Algorithm: For each active state in parallel, select first enabled transition (document order)
    LOG_DEBUG("W3C SCXML 3.13: Parallel state detected - collecting all region transitions");
    std::vector<TransitionInfo> enabledTransitions;
    enabledTransitions.reserve(activeStates.size());  // Optimize: pre-allocate for typical case

    // W3C SCXML 3.13: Track states with selected transitions for preemption check
    // Memory safety: Use state IDs instead of raw pointers
    std::set<std::string> statesWithTransitions;

    for (auto it = activeStates.rbegin(); it != activeStates.rend(); ++it) {
        const std::string &activeStateId = *it;
        auto stateNode = stateCache[activeStateId];

        if (!stateNode) {
            continue;
        }

        // Check if this state is descendant of the same parallel ancestor
        bool isInParallel = false;
        IStateNode *current = stateNode;
        while (current) {
            if (current == parallelAncestor) {
                isInParallel = true;
                break;
            }
            current = current->getParent();
        }

        if (!isInParallel) {
            continue;
        }

        // W3C SCXML 3.13: Preemption check - skip if descendant state already has transition
        bool isPreempted = false;
        for (const auto &transitionStateId : statesWithTransitions) {
            // Check if current state is ancestor of a state that already has a transition
            auto transitionStateNode = stateCache[transitionStateId];
            if (!transitionStateNode) {
                continue;
            }

            IStateNode *descendant = transitionStateNode;
            while (descendant) {
                if (descendant->getParent() == stateNode) {
                    // Current state is ancestor of state with transition - preempted
                    isPreempted = true;
                    LOG_DEBUG("W3C SCXML 3.13: Transition from '{}' preempted by descendant state", activeStateId);
                    break;
                }
                descendant = descendant->getParent();
            }
            if (isPreempted) {
                break;
            }
        }

        if (isPreempted) {
            continue;
        }

        const auto &transitions = stateNode->getTransitions();
        for (const auto &transitionNode : transitions) {
            const std::vector<std::string> &eventDescriptors = transitionNode->getEvents();
            if (!eventDescriptors.empty()) {
                continue;
            }

            std::string condition = transitionNode->getGuard();
            bool conditionResult = condition.empty() || evaluateCondition(condition);

            if (!conditionResult) {
                continue;
            }

            const auto &targets = transitionNode->getTargets();
            if (targets.empty()) {
                // W3C SCXML: Internal transition - execute inline and stop checking this state
                // This is still the "first enabled transition" for this state
                const auto &actionNodes = transitionNode->getActionNodes();
                if (!actionNodes.empty()) {
                    executeActionNodes(actionNodes, false);
                }
                break;  // First enabled transition rule applies to internal transitions too
            }

            std::string targetState = targets[0];
            std::vector<std::string> exitSet = computeExitSet(activeStateId, targetState);

            enabledTransitions.emplace_back(stateNode, transitionNode, targetState, exitSet);
            statesWithTransitions.insert(activeStateId);  // Track for preemption
            LOG_DEBUG("W3C SCXML 3.13: Collected parallel transition: {} -> {}", activeStateId, targetState);

            // W3C SCXML: Only select first enabled transition per state (document order)
            break;
        }
    }

    if (enabledTransitions.empty()) {
        LOG_DEBUG("W3C SCXML 3.13: No transitions collected from parallel regions");
        return false;
    }

    // W3C SCXML 3.13: Sort by document order
    // Performance: Cache document positions to avoid O(n) tree traversal per comparison
    std::unordered_map<std::string, int> positionCache;
    for (const auto &trans : enabledTransitions) {
        const std::string &stateId = trans.sourceState->getId();
        if (positionCache.find(stateId) == positionCache.end()) {
            positionCache[stateId] = getStateDocumentPosition(stateId);
        }
    }

    std::sort(enabledTransitions.begin(), enabledTransitions.end(),
              [&positionCache](const TransitionInfo &a, const TransitionInfo &b) {
                  int posA = positionCache.at(a.sourceState->getId());
                  int posB = positionCache.at(b.sourceState->getId());
                  return posA < posB;
              });

    LOG_DEBUG("W3C SCXML 3.13: Executing {} parallel transitions as microstep", enabledTransitions.size());

    bool success = executeTransitionMicrostep(enabledTransitions);

    if (success) {
        updateStatistics();
        stats_.totalTransitions += static_cast<int>(enabledTransitions.size());
    }

    return success;
}

bool StateMachine::executeTransitionMicrostep(const std::vector<TransitionInfo> &transitions) {
    if (transitions.empty()) {
        return false;
    }

    LOG_DEBUG("W3C SCXML 3.13: Executing microstep with {} transition(s)", transitions.size());

    // Phase 1: Exit all source states (executing onexit actions)
    // W3C SCXML: Compute unique exit set from all transitions, exit in correct order
    std::set<std::string> exitSetUnique;
    for (const auto &transInfo : transitions) {
        for (const auto &stateId : transInfo.exitSet) {
            exitSetUnique.insert(stateId);
        }
    }

    // Convert to vector for ordered exit (deepest first)
    std::vector<std::string> allStatesToExit(exitSetUnique.begin(), exitSetUnique.end());

    // Performance: Cache state lookups and depths to avoid repeated parent chain traversal
    std::unordered_map<std::string, IStateNode *> exitStateCache;
    std::unordered_map<std::string, int> depthCache;

    for (const auto &stateId : allStatesToExit) {
        auto node = model_->findStateById(stateId);
        exitStateCache[stateId] = node;

        // Pre-calculate depth once for O(1) lookup during sort
        int depth = 0;
        if (node) {
            auto parent = node->getParent();
            while (parent) {
                depth++;
                parent = parent->getParent();
            }
        }
        depthCache[stateId] = depth;
    }

    // W3C SCXML 3.13: Sort by depth (deepest first), then by reverse document order
    // Performance: Cache document positions for O(1) lookup during sort
    std::unordered_map<std::string, int> positionCache;
    for (const auto &stateId : allStatesToExit) {
        positionCache[stateId] = getStateDocumentPosition(stateId);
    }

    std::sort(allStatesToExit.begin(), allStatesToExit.end(),
              [&depthCache, &positionCache](const std::string &a, const std::string &b) {
                  // Primary: Sort deepest first (higher depth comes first)
                  int depthA = depthCache.at(a);
                  int depthB = depthCache.at(b);

                  if (depthA != depthB) {
                      return depthA > depthB;
                  }

                  // Secondary: For states at same depth, use reverse document order
                  // W3C SCXML: Exit states in reverse document order (later states exit first)
                  int posA = positionCache.at(a);
                  int posB = positionCache.at(b);
                  return posA > posB;  // Reverse document order
              });

    LOG_DEBUG("W3C SCXML 3.13: Phase 1 - Exiting {} state(s)", allStatesToExit.size());
    for (const auto &stateId : allStatesToExit) {
        if (!exitState(stateId)) {
            LOG_ERROR("W3C SCXML 3.13: Failed to exit state '{}' during microstep", stateId);
            return false;
        }
    }

    // Phase 2: Execute all transition actions in document order
    LOG_DEBUG("W3C SCXML 3.13: Phase 2 - Executing transition actions for {} transition(s)", transitions.size());
    for (const auto &transInfo : transitions) {
        const auto &actionNodes = transInfo.transition->getActionNodes();
        if (!actionNodes.empty()) {
            LOG_DEBUG("W3C SCXML 3.13: Executing {} action(s) from transition", actionNodes.size());
            // processEventsAfter=false: Events raised here will be queued, not processed immediately
            executeActionNodes(actionNodes, false);
        }
    }

    // Phase 3: Enter all target states (executing onentry actions)
    LOG_DEBUG("W3C SCXML 3.13: Phase 3 - Entering {} target state(s)", transitions.size());
    for (const auto &transInfo : transitions) {
        if (!transInfo.targetState.empty()) {
            if (!enterState(transInfo.targetState)) {
                LOG_ERROR("W3C SCXML 3.13: Failed to enter target state '{}' during microstep", transInfo.targetState);
                return false;
            }
        }
    }

    LOG_DEBUG("W3C SCXML 3.13: Microstep execution complete");
    return true;
}

bool StateMachine::exitState(const std::string &stateId) {
    LOG_DEBUG("Exiting state: {}", stateId);

    // SCXML W3C specification section 3.4: Execute exit actions in correct order for parallel states
    auto stateNode = model_->findStateById(stateId);
    if (stateNode && stateNode->getType() == Type::PARALLEL) {
        // For parallel states: Child regions exit FIRST, then parallel state exits
        LOG_DEBUG("StateMachine: SCXML W3C compliant - executing parallel state exit actions in correct order");

        // Exit actions for child regions are already handled by executeExitActions for parallel
        // Execute parallel state's own onexit actions LAST
        bool exitResult = executeExitActions(stateId);
        if (!exitResult && isRunning_) {
            // Only log error if machine is still running - during shutdown, raise failures are expected
            LOG_ERROR("StateMachine: Failed to execute exit actions for parallel state: {}", stateId);
        }
    } else {
        // Execute IActionNode-based exit actions for non-parallel states
        bool exitResult = executeExitActions(stateId);
        if (!exitResult && isRunning_) {
            // Only log error if machine is still running - during shutdown, raise failures are expected
            LOG_ERROR("StateMachine: Failed to execute exit actions for state: {}", stateId);
        }
        (void)exitResult;  // Suppress unused variable warning in release builds
    }

    // Get state node for invoke cancellation and history recording
    auto stateNodeForCleanup = model_->findStateById(stateId);

    // W3C SCXML specification section 3.13: Cancel invokes BEFORE removing from active states
    // "Then it MUST cancel any ongoing invocations that were triggered by that state"
    // This must happen AFTER onexit handlers but BEFORE state removal
    if (stateNodeForCleanup && invokeExecutor_) {
        const auto &invokes = stateNodeForCleanup->getInvoke();
        LOG_DEBUG("StateMachine::exitState - State '{}' has {} invoke(s) to check", stateId, invokes.size());

        for (const auto &invoke : invokes) {
            const std::string &invokeid = invoke->getId();
            if (!invokeid.empty()) {
                bool isActive = invokeExecutor_->isInvokeActive(invokeid);
                LOG_DEBUG("StateMachine::exitState - Invoke '{}' isActive: {}", invokeid, isActive);

                if (isActive) {
                    LOG_DEBUG("StateMachine: Cancelling active invoke '{}' due to state exit: {}", invokeid, stateId);
                    bool cancelled = invokeExecutor_->cancelInvoke(invokeid);
                    LOG_DEBUG("StateMachine: Cancel result for invoke '{}': {}", invokeid, cancelled);
                } else {
                    LOG_DEBUG("StateMachine: NOT cancelling inactive invoke '{}' (may be completing naturally)",
                              invokeid);
                }
            } else {
                LOG_WARN("StateMachine::exitState - Found invoke with empty ID in state '{}'", stateId);
            }
        }
    } else {
        if (!stateNodeForCleanup) {
            LOG_DEBUG("StateMachine::exitState - stateNodeForCleanup is null for state '{}'", stateId);
        }
        if (!invokeExecutor_) {
            LOG_DEBUG("StateMachine::exitState - invokeExecutor_ is null");
        }
    }

    // Record history before removing from active states (SCXML W3C specification section 3.6)
    // History recording needs current active states, so must happen before hierarchyManager_->exitState
    if (historyManager_ && hierarchyManager_ && stateNodeForCleanup) {
        if (stateNodeForCleanup->getType() == Type::COMPOUND || stateNodeForCleanup->getType() == Type::PARALLEL) {
            // Get current active states before exiting
            auto activeStates = hierarchyManager_->getActiveStates();

            // Record history for this compound state
            bool recorded = historyManager_->recordHistory(stateId, activeStates);
            if (recorded) {
                LOG_DEBUG("Recorded history for compound state: {}", stateId);
            }
        }
    }

    // W3C SCXML section 3.13: Finally remove the state from active states list
    // Use hierarchy manager for SCXML-compliant state exit
    assert(hierarchyManager_ && "SCXML violation: hierarchy manager required for state management");
    LOG_DEBUG("StateMachine::exitState - executionContext_ is {}", executionContext_ ? "valid" : "NULL");
    hierarchyManager_->exitState(stateId, executionContext_);

    // State management fully delegated to StateHierarchyManager

    LOG_DEBUG("Successfully exited state: {}", stateId);
    return true;
}

bool StateMachine::ensureJSEnvironment() {
    if (jsEnvironmentReady_) {
        return true;
    }

    return setupJSEnvironment();
}

bool StateMachine::setupJSEnvironment() {
    // JSEngine은 생성자에서 자동 초기화됨 (RAII)
    auto &jsEngine = RSM::JSEngine::instance();  // RAII 보장
    LOG_DEBUG("StateMachine: JSEngine automatically initialized via RAII at address: {}",
              static_cast<void *>(&jsEngine));

    // Create JavaScript session only if it doesn't exist (for invoke scenarios)
    // Check if session already exists (created by InvokeExecutor for child sessions)
    bool sessionExists = RSM::JSEngine::instance().hasSession(sessionId_);

    if (!sessionExists) {
        // Create new session for standalone StateMachine
        if (!RSM::JSEngine::instance().createSession(sessionId_)) {
            LOG_ERROR("StateMachine: Failed to create JavaScript session");
            return false;
        }
        LOG_DEBUG("StateMachine: Created new JavaScript session: {}", sessionId_);
    } else {
        LOG_DEBUG("StateMachine: Using existing JavaScript session (injected): {}", sessionId_);
    }

    // W3C SCXML 5.10: Set up read-only system variables (_sessionid, _name, _ioprocessors)
    std::string sessionName = model_ && !model_->getName().empty() ? model_->getName() : "StateMachine";
    std::vector<std::string> ioProcessors = {"scxml"};  // W3C SCXML I/O Processors
    auto setupResult = RSM::JSEngine::instance().setupSystemVariables(sessionId_, sessionName, ioProcessors).get();
    if (!setupResult.isSuccess()) {
        LOG_ERROR("StateMachine: Failed to setup system variables: {}", setupResult.getErrorMessage());
        return false;
    }

    // Register this StateMachine instance with JSEngine for In() function support
    RSM::JSEngine::instance().setStateMachine(this, sessionId_);
    LOG_DEBUG("StateMachine: Registered with JSEngine for In() function support");

    // W3C SCXML 5.3: Initialize data model with binding mode support (early/late binding)
    if (model_) {
        // Collect all data items (top-level + state-level) for global scope
        const auto allDataItems = collectAllDataItems();
        LOG_INFO("StateMachine: Initializing {} total data items (global scope with {} binding)", allDataItems.size(),
                 model_->getBinding());

        // Get binding mode: "early" (default) or "late"
        const std::string &binding = model_->getBinding();
        bool isEarlyBinding = (binding.empty() || binding == "early");

        if (isEarlyBinding) {
            // Early binding (default): Initialize all variables with values at document load
            LOG_DEBUG("StateMachine: Using early binding - all variables initialized with values at init");
            for (const auto &dataInfo : allDataItems) {
                initializeDataItem(dataInfo.dataItem, true);  // assignValue=true
            }
        } else {
            // Late binding: Create all variables but don't assign values yet
            LOG_DEBUG("StateMachine: Using late binding - creating variables without values (assigned on state entry)");
            for (const auto &dataInfo : allDataItems) {
                initializeDataItem(dataInfo.dataItem, false);  // assignValue=false (defer assignment)
            }
            // Note: Value assignment will happen in enterState() when each state is entered
        }
    } else {
        LOG_DEBUG("StateMachine: No model available for data model initialization");
    }

    // Initialize ActionExecutor and ExecutionContext (needed for script execution)
    if (!initializeActionExecutor()) {
        LOG_ERROR("StateMachine: Failed to initialize action executor");
        return false;
    }

    // W3C SCXML 403c: Set execution context for concurrent region action execution
    // This must happen AFTER executionContext_ is created in initializeActionExecutor()
    if (hierarchyManager_ && executionContext_) {
        hierarchyManager_->setExecutionContext(executionContext_);
        LOG_DEBUG("StateMachine: ExecutionContext successfully configured for StateHierarchyManager (403c compliance)");

        // W3C SCXML 3.13: Set initial transition callback for proper event queuing
        hierarchyManager_->setInitialTransitionCallback(
            [this](const std::vector<std::shared_ptr<IActionNode>> &actions) {
                // Execute actions with immediate mode control to ensure proper event queuing
                executeActionNodes(actions, false);
            });
        LOG_DEBUG(
            "StateMachine: Initial transition callback configured for StateHierarchyManager (test 412 compliance)");
    }

    // W3C SCXML 5.8: Execute top-level scripts AFTER datamodel init, BEFORE start()
    if (model_) {
        const auto &topLevelScripts = model_->getTopLevelScripts();
        if (!topLevelScripts.empty()) {
            LOG_INFO("StateMachine: Executing {} top-level script(s) at document load time (W3C SCXML 5.8)",
                     topLevelScripts.size());

            for (size_t i = 0; i < topLevelScripts.size(); ++i) {
                const auto &script = topLevelScripts[i];

                if (!script) {
                    LOG_WARN("StateMachine: Null script at index {} - skipping (W3C SCXML 5.8)", i);
                    continue;
                }

                if (!executionContext_) {
                    LOG_ERROR("StateMachine: ExecutionContext is null - cannot execute scripts (W3C SCXML 5.8)");
                    return false;
                }

                LOG_DEBUG("StateMachine: Executing top-level script #{} (W3C SCXML 5.8)", i + 1);
                bool success = script->execute(*executionContext_);
                if (!success) {
                    LOG_ERROR("StateMachine: Top-level script #{} execution failed (W3C SCXML 5.8) - document rejected",
                              i + 1);
                    return false;  // W3C SCXML 5.8: Script failure rejects document
                }
            }
            LOG_DEBUG("StateMachine: All {} top-level script(s) executed successfully (W3C SCXML 5.8)",
                      topLevelScripts.size());
        }
    }

    // Pass EventDispatcher to ActionExecutor if it was set before initialization
    if (eventDispatcher_ && actionExecutor_) {
        auto actionExecutorImpl = std::dynamic_pointer_cast<ActionExecutorImpl>(actionExecutor_);
        if (actionExecutorImpl) {
            actionExecutorImpl->setEventDispatcher(eventDispatcher_);
            LOG_DEBUG(
                "StateMachine: EventDispatcher passed to ActionExecutor during JS environment setup for session: {}",
                sessionId_);
        }
    }

    // Pass EventRaiser to ActionExecutor if available
    if (eventRaiser_ && actionExecutor_) {
        actionExecutor_->setEventRaiser(eventRaiser_);
        LOG_DEBUG("StateMachine: EventRaiser passed to ActionExecutor for session: {}", sessionId_);
    }

    // Register EventRaiser with JSEngine after session creation
    // This handles both cases: EventRaiser set before session creation (deferred) and after
    if (eventRaiser_) {
        // Use EventRaiserService for centralized registration
        if (EventRaiserService::getInstance().registerEventRaiser(sessionId_, eventRaiser_)) {
            LOG_DEBUG("StateMachine: EventRaiser registered via Service after session creation for session: {}",
                      sessionId_);
        } else {
            LOG_DEBUG("StateMachine: EventRaiser already registered for session: {}", sessionId_);
        }
    }

    jsEnvironmentReady_ = true;
    LOG_DEBUG("StateMachine: JavaScript environment setup completed");
    return true;
}

void StateMachine::updateStatistics() {
    stats_.currentState = getCurrentState();
    stats_.isRunning = isRunning_;
}

bool StateMachine::initializeActionExecutor() {
    try {
        // Create ActionExecutor using the same session as StateMachine
        actionExecutor_ = std::make_shared<ActionExecutorImpl>(sessionId_);

        // Inject EventRaiser if already set via builder pattern
        if (eventRaiser_) {
            actionExecutor_->setEventRaiser(eventRaiser_);
            LOG_DEBUG("StateMachine: EventRaiser injected to ActionExecutor during initialization for session: {}",
                      sessionId_);
        }

        // Create ExecutionContext with shared_ptr and sessionId
        executionContext_ = std::make_shared<ExecutionContextImpl>(actionExecutor_, sessionId_);

        LOG_DEBUG("ActionExecutor and ExecutionContext initialized for session: {}", sessionId_);
        return true;
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to initialize ActionExecutor: {}", e.what());
        return false;
    }
}

bool StateMachine::executeActionNodes(const std::vector<std::shared_ptr<RSM::IActionNode>> &actions,
                                      bool processEventsAfter) {
    if (!executionContext_) {
        LOG_WARN("StateMachine: ExecutionContext not initialized, skipping action node execution");
        return true;  // Not a failure, just no actions to execute
    }

    bool allSucceeded = true;

    // W3C SCXML compliance: Set immediate mode to false during executable content execution
    // This ensures events raised during execution are queued and processed after completion
    if (eventRaiser_) {
        auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
        if (eventRaiserImpl) {
            eventRaiserImpl->setImmediateMode(false);
            LOG_DEBUG("SCXML compliance: Set immediate mode to false for executable content execution");
        }
    }

    for (const auto &action : actions) {
        if (!action) {
            LOG_WARN("StateMachine: Null action node encountered, skipping");
            continue;
        }

        try {
            LOG_DEBUG("Executing action: {}", action->getActionType());
            if (action->execute(*executionContext_)) {
                LOG_DEBUG("Successfully executed action: {}", action->getActionType());
            } else {
                LOG_WARN("Failed to execute action: {} - W3C compliance: stopping remaining actions",
                         action->getActionType());
                allSucceeded = false;
                // W3C SCXML specification: If error occurs in executable content,
                // processor MUST NOT process remaining elements in the block
                break;
            }
        } catch (const std::exception &e) {
            LOG_WARN("Exception executing action {}: {} - W3C compliance: stopping remaining actions",
                     action->getActionType(), e.what());
            allSucceeded = false;
            // W3C SCXML specification: If error occurs in executable content,
            // processor MUST NOT process remaining elements in the block
            break;
        }
    }

    // W3C SCXML compliance: Restore immediate mode and optionally process queued events
    if (eventRaiser_) {
        auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
        if (eventRaiserImpl) {
            eventRaiserImpl->setImmediateMode(true);
            // Process events only if requested (e.g., for entry actions, not exit/transition actions)
            if (processEventsAfter) {
                eventRaiserImpl->processQueuedEvents();
                LOG_DEBUG("SCXML compliance: Restored immediate mode and processed queued events");
            } else {
                LOG_DEBUG("SCXML compliance: Restored immediate mode (events will be processed later)");
            }
        }
    }

    // W3C SCXML compliance: Return true only if all actions succeeded or no actions to execute
    // If any action failed, we stopped execution per W3C spec, so return false to indicate failure
    return actions.empty() || allSucceeded;
}

bool StateMachine::executeEntryActions(const std::string &stateId) {
    if (!model_) {
        assert(false && "SCXML violation: StateMachine must have a model for entry action execution");
        return false;
    }

    // Find the StateNode in the SCXML model
    auto stateNode = model_->findStateById(stateId);
    if (!stateNode) {
        // SCXML W3C compliance: All states in active configuration must exist in model
        assert(false && "SCXML violation: Active state not found in model");
        return false;
    }

    LOG_DEBUG("Executing entry actions for state: {}", stateId);

    // SCXML W3C specification section 3.4: Parallel states require special handling
    if (stateNode->getType() == Type::PARALLEL) {
        auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
        assert(parallelState && "SCXML violation: PARALLEL type state must be ConcurrentStateNode");

        // W3C SCXML 3.8: Execute parallel state's own onentry action blocks FIRST
        const auto &parallelEntryBlocks = parallelState->getEntryActionBlocks();
        if (!parallelEntryBlocks.empty()) {
            LOG_DEBUG("W3C SCXML 3.8: executing {} entry action blocks for parallel state itself: {}",
                      parallelEntryBlocks.size(), stateId);
            for (size_t i = 0; i < parallelEntryBlocks.size(); ++i) {
                if (!executeActionNodes(parallelEntryBlocks[i])) {
                    LOG_WARN("W3C SCXML 3.8: Parallel entry block {}/{} failed, continuing", i + 1,
                             parallelEntryBlocks.size());
                }
            }
        }

        // provide ExecutionContext to all regions for action execution
        if (executionContext_) {
            parallelState->setExecutionContextForRegions(executionContext_);
            LOG_DEBUG("Injected ExecutionContext into all regions of parallel state: {}", stateId);
        }

        // SCXML W3C specification: ALL child regions MUST have their entry actions executed AFTER parallel state
        const auto &regions = parallelState->getRegions();
        assert(!regions.empty() && "SCXML violation: parallel state must have at least one region");

        LOG_DEBUG("SCXML W3C compliant - executing entry actions for {} child regions in parallel state: {}",
                  regions.size(), stateId);

        // Execute entry actions for each region's root state
        for (const auto &region : regions) {
            assert(region && "SCXML violation: parallel state cannot have null regions");

            auto rootState = region->getRootState();
            assert(rootState && "SCXML violation: region must have root state");

            // W3C SCXML 3.8: Execute entry action blocks for the region's root state
            const auto &regionEntryBlocks = rootState->getEntryActionBlocks();
            if (!regionEntryBlocks.empty()) {
                LOG_DEBUG("W3C SCXML 3.8: executing {} entry action blocks for region: {}", regionEntryBlocks.size(),
                          region->getId());
                for (size_t i = 0; i < regionEntryBlocks.size(); ++i) {
                    if (!executeActionNodes(regionEntryBlocks[i])) {
                        LOG_WARN("W3C SCXML 3.8: Region entry block {}/{} failed, continuing", i + 1,
                                 regionEntryBlocks.size());
                    }
                }
            }

            // SCXML W3C specification: Enter initial child states of each region ONLY if not already active
            const auto &children = rootState->getChildren();
            if (!children.empty()) {
                // SCXML W3C 사양 준수: 병렬 영역이 이미 활성화되어 있으면 초기 상태로 재진입하지 않음
                if (!region->isActive()) {
                    std::string initialChild = rootState->getInitialState();
                    if (initialChild.empty()) {
                        // SCXML W3C: Use first child as default initial state
                        initialChild = children[0]->getId();
                    }

                    LOG_DEBUG("Entering initial child state for INACTIVE region {}: {}", region->getId(), initialChild);

                    // W3C SCXML 3.8: Execute entry action blocks for the initial child state
                    auto childState = model_->findStateById(initialChild);
                    if (childState) {
                        const auto &childEntryBlocks = childState->getEntryActionBlocks();
                        if (!childEntryBlocks.empty()) {
                            LOG_DEBUG("W3C SCXML 3.8: executing {} entry action blocks for initial child state: {}",
                                      childEntryBlocks.size(), initialChild);
                            for (size_t i = 0; i < childEntryBlocks.size(); ++i) {
                                if (!executeActionNodes(childEntryBlocks[i])) {
                                    LOG_WARN("W3C SCXML 3.8: Child entry block {}/{} failed, continuing", i + 1,
                                             childEntryBlocks.size());
                                }
                            }
                        }
                    }
                } else {
                    // SCXML W3C 사양 준수: 이미 활성화된 영역은 초기 상태로 재진입하지 않음
                    auto concreteRegion = std::dynamic_pointer_cast<ConcurrentRegion>(region);
                    std::string currentState = concreteRegion ? concreteRegion->getCurrentState() : "unknown";

                    LOG_DEBUG("SCXML W3C compliance - skipping initial state entry for already ACTIVE region: {} "
                              "(current state: {})",
                              region->getId(), currentState);

                    // SCXML W3C 사양 위반 방지: 이미 활성화된 영역의 현재 상태 유지
                    assert(concreteRegion && !concreteRegion->getCurrentState().empty() &&
                           "SCXML violation: active region must have current state");

                    // SCXML W3C 사양 준수 검증: 활성 영역이 초기 상태로 재설정되지 않음을 보장
                    assert(region->isActive() &&
                           "SCXML violation: region marked as active but isActive() returns false");

                    // SCXML W3C 사양 위반 감지: 병렬 상태 재진입 시 상태 일관성 검증
                    const auto &currentActiveStates = region->getActiveStates();
                    assert(!currentActiveStates.empty() && "SCXML violation: active region must have active states");
                }
            }
        }

        return true;
    }

    // W3C SCXML 3.8: Execute block-based entry actions for non-parallel states
    const auto &entryBlocks = stateNode->getEntryActionBlocks();
    if (!entryBlocks.empty()) {
        LOG_DEBUG("W3C SCXML 3.8: Executing {} entry action blocks for state: {}", entryBlocks.size(), stateId);

        for (size_t i = 0; i < entryBlocks.size(); ++i) {
            LOG_DEBUG("W3C SCXML 3.8: Executing entry action block {}/{} for state: {}", i + 1, entryBlocks.size(),
                      stateId);

            // W3C SCXML 3.8: Each onentry handler is a separate block
            // If one block fails, continue with remaining blocks
            if (!executeActionNodes(entryBlocks[i])) {
                LOG_WARN("W3C SCXML 3.8: Entry action block {}/{} failed, continuing with remaining blocks", i + 1,
                         entryBlocks.size());
                // Don't break - continue with next block per W3C spec
            }
        }

        // W3C SCXML: State entry succeeds even if some action blocks fail
        return true;
    }

    return true;
}

bool StateMachine::executeExitActions(const std::string &stateId) {
    if (!model_) {
        return true;  // No model, no actions to execute
    }

    // Find the StateNode in the SCXML model
    auto stateNode = model_->findStateById(stateId);
    if (!stateNode) {
        LOG_DEBUG("State {} not found in SCXML model, skipping exit actions", stateId);
        return true;  // Not an error if state not found in model
    }

    // SCXML W3C specification section 3.4: Parallel states require special exit sequence
    if (stateNode->getType() == Type::PARALLEL) {
        auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
        assert(parallelState && "SCXML violation: PARALLEL type state must be ConcurrentStateNode");

        const auto &regions = parallelState->getRegions();
        assert(!regions.empty() && "SCXML violation: parallel state must have at least one region");

        LOG_DEBUG("SCXML W3C compliant - executing exit sequence for parallel state: {}", stateId);

        // SCXML W3C specification: Execute child region exit actions FIRST in REVERSE document order
        for (auto it = regions.rbegin(); it != regions.rend(); ++it) {
            const auto &region = *it;
            assert(region && "SCXML violation: parallel state cannot have null regions");

            if (region->isActive()) {
                auto rootState = region->getRootState();
                assert(rootState && "SCXML violation: region must have root state");

                // W3C SCXML 3.9: Execute exit action blocks for currently active child states in this region
                const auto activeStates = region->getActiveStates();
                const auto &children = rootState->getChildren();
                for (const auto &child : children) {
                    // Check if this child is currently active
                    bool isChildActive =
                        std::find(activeStates.begin(), activeStates.end(), child->getId()) != activeStates.end();
                    if (child && isChildActive) {
                        const auto &childExitBlocks = child->getExitActionBlocks();
                        if (!childExitBlocks.empty()) {
                            LOG_DEBUG("W3C SCXML 3.9: executing {} exit action blocks for active child state: {}",
                                      childExitBlocks.size(), child->getId());
                            for (size_t i = 0; i < childExitBlocks.size(); ++i) {
                                if (!executeActionNodes(childExitBlocks[i], false)) {
                                    LOG_WARN("W3C SCXML 3.9: Child exit block {}/{} failed, continuing", i + 1,
                                             childExitBlocks.size());
                                }
                            }
                        }
                        break;
                    }
                }

                // W3C SCXML 3.9: Execute exit action blocks for the region's root state
                const auto &regionExitBlocks = rootState->getExitActionBlocks();
                if (!regionExitBlocks.empty()) {
                    LOG_DEBUG("W3C SCXML 3.9: executing {} exit action blocks for region: {}", regionExitBlocks.size(),
                              region->getId());
                    for (size_t i = 0; i < regionExitBlocks.size(); ++i) {
                        if (!executeActionNodes(regionExitBlocks[i], false)) {
                            LOG_WARN("W3C SCXML 3.9: Region exit block {}/{} failed, continuing", i + 1,
                                     regionExitBlocks.size());
                        }
                    }
                }
            }
        }

        // W3C SCXML 3.9: Execute parallel state's own onexit action blocks LAST
        const auto &parallelExitBlocks = parallelState->getExitActionBlocks();
        if (!parallelExitBlocks.empty()) {
            LOG_DEBUG("W3C SCXML 3.9: executing {} exit action blocks for parallel state itself: {}",
                      parallelExitBlocks.size(), stateId);
            for (size_t i = 0; i < parallelExitBlocks.size(); ++i) {
                if (!executeActionNodes(parallelExitBlocks[i], false)) {
                    LOG_WARN("W3C SCXML 3.9: Parallel exit block {}/{} failed, continuing", i + 1,
                             parallelExitBlocks.size());
                }
            }
        }

        return true;
    }

    // W3C SCXML 3.9: Execute block-based exit actions for non-parallel states
    const auto &exitBlocks = stateNode->getExitActionBlocks();
    if (!exitBlocks.empty()) {
        LOG_DEBUG("W3C SCXML 3.9: Executing {} exit action blocks for state: {}", exitBlocks.size(), stateId);

        for (size_t i = 0; i < exitBlocks.size(); ++i) {
            LOG_DEBUG("W3C SCXML 3.9: Executing exit action block {}/{} for state: {}", i + 1, exitBlocks.size(),
                      stateId);

            // W3C SCXML 3.9: Each onexit handler is a separate block
            // If one block fails, continue with remaining blocks
            if (!executeActionNodes(exitBlocks[i], false)) {
                LOG_WARN("W3C SCXML 3.9: Exit action block {}/{} failed, continuing with remaining blocks", i + 1,
                         exitBlocks.size());
                // Don't break - continue with next block per W3C spec
            }
        }

        // W3C SCXML: State exit succeeds even if some action blocks fail
        return true;
    }

    return true;
}

void StateMachine::handleParallelStateCompletion(const std::string &stateId) {
    LOG_DEBUG("Handling parallel state completion for: {}", stateId);

    // Generate done.state.{stateId} event according to SCXML W3C specification section 3.4
    std::string doneEventName = "done.state." + stateId;

    LOG_INFO("Generating done.state event: {} for completed parallel state: {}", doneEventName, stateId);

    // Process the done.state event to trigger any transitions waiting for it
    if (isRunning_) {
        auto result = processEvent(doneEventName, "");
        if (result.success) {
            LOG_DEBUG("Successfully processed done.state event: {}", doneEventName);
        } else {
            LOG_DEBUG("No transitions found for done.state event: {} (this is normal if no transitions are waiting for "
                      "this event)",
                      doneEventName);
        }
    } else {
        LOG_WARN("Cannot process done.state event {} - state machine is not running", doneEventName);
    }
}

void StateMachine::setupParallelStateCallbacks() {
    if (!model_) {
        LOG_WARN("StateMachine: Cannot setup parallel state callbacks - no model available");
        return;
    }

    LOG_DEBUG("StateMachine: Setting up completion callbacks for parallel states");

    const auto &allStates = model_->getAllStates();
    int parallelStateCount = 0;

    for (const auto &state : allStates) {
        if (state && state->getType() == Type::PARALLEL) {
            // Cast to ConcurrentStateNode to access the callback method
            auto parallelState = std::dynamic_pointer_cast<ConcurrentStateNode>(state);
            if (parallelState) {
                // Set up the completion callback using a lambda that captures this StateMachine
                parallelState->setCompletionCallback([this](const std::string &completedStateId) {
                    this->handleParallelStateCompletion(completedStateId);
                });

                parallelStateCount++;
                LOG_DEBUG("Set up completion callback for parallel state: {}", state->getId());
            } else {
                LOG_WARN("Found parallel state that is not a ConcurrentStateNode: {}", state->getId());
            }
        }
    }

    LOG_INFO("Set up completion callbacks for {} parallel states", parallelStateCount);
}

void StateMachine::initializeHistoryManager() {
    LOG_DEBUG("StateMachine: Initializing History Manager with SOLID architecture");

    // Create state provider function for dependency injection
    auto stateProvider = [this](const std::string &stateId) -> std::shared_ptr<IStateNode> {
        if (!model_) {
            return nullptr;
        }
        // Find state by ID in the shared_ptr vector
        auto allStates = model_->getAllStates();
        for (const auto &state : allStates) {
            if (state && state->getId() == stateId) {
                return state;
            }
        }
        return nullptr;
    };

    // Create filter components using Strategy pattern
    auto shallowFilter = std::make_unique<ShallowHistoryFilter>(stateProvider);
    auto deepFilter = std::make_unique<DeepHistoryFilter>(stateProvider);
    auto validator = std::make_unique<HistoryValidator>(stateProvider);

    // Create HistoryManager with dependency injection
    historyManager_ = std::make_unique<HistoryManager>(stateProvider, std::move(shallowFilter), std::move(deepFilter),
                                                       std::move(validator));

    LOG_INFO("StateMachine: History Manager initialized with SOLID dependencies");
}

void StateMachine::initializeHistoryAutoRegistrar() {
    LOG_DEBUG("StateMachine: Initializing History Auto-Registrar with SOLID architecture");

    // Create state provider function for dependency injection (same as history manager)
    auto stateProvider = [this](const std::string &stateId) -> std::shared_ptr<IStateNode> {
        if (!model_) {
            return nullptr;
        }
        // Find state by ID in the model
        auto allStates = model_->getAllStates();
        for (const auto &state : allStates) {
            if (state && state->getId() == stateId) {
                return state;
            }
        }
        return nullptr;
    };

    // Create HistoryStateAutoRegistrar with dependency injection
    historyAutoRegistrar_ = std::make_unique<HistoryStateAutoRegistrar>(stateProvider);

    LOG_INFO("StateMachine: History Auto-Registrar initialized with SOLID dependencies");
}

bool StateMachine::registerHistoryState(const std::string &historyStateId, const std::string &parentStateId,
                                        HistoryType type, const std::string &defaultStateId) {
    if (!historyManager_) {
        LOG_ERROR("StateMachine: History Manager not initialized");
        return false;
    }

    return historyManager_->registerHistoryState(historyStateId, parentStateId, type, defaultStateId);
}

bool StateMachine::isHistoryState(const std::string &stateId) const {
    if (!historyManager_) {
        return false;
    }

    return historyManager_->isHistoryState(stateId);
}

void StateMachine::clearAllHistory() {
    if (historyManager_) {
        historyManager_->clearAllHistory();
    }
}

std::vector<HistoryEntry> StateMachine::getHistoryEntries() const {
    if (!historyManager_) {
        return {};
    }

    return historyManager_->getHistoryEntries();
}

void StateMachine::executeOnEntryActions(const std::string &stateId) {
    if (!model_) {
        LOG_ERROR("Cannot execute onentry actions: SCXML model is null");
        return;
    }

    // Find the state node
    auto stateNode = model_->findStateById(stateId);
    if (!stateNode) {
        LOG_ERROR("Cannot find state node for onentry execution: {}", stateId);
        return;
    }

    // W3C SCXML 3.8: Get entry action blocks from the state
    const auto &entryBlocks = stateNode->getEntryActionBlocks();
    if (entryBlocks.empty()) {
        LOG_DEBUG("No onentry actions to execute for state: {}", stateId);
        return;
    }

    LOG_DEBUG("W3C SCXML 3.8: Executing {} onentry action blocks for state: {}", entryBlocks.size(), stateId);

    // W3C SCXML compliance: Set immediate mode to false during executable content execution
    // This ensures events raised during execution are queued and processed after completion
    if (eventRaiser_) {
        auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
        if (eventRaiserImpl) {
            eventRaiserImpl->setImmediateMode(false);
            LOG_DEBUG("SCXML compliance: Set immediate mode to false for onentry actions execution");
        }
    }

    // W3C SCXML 3.8: Execute each onentry handler as a separate block
    for (size_t blockIndex = 0; blockIndex < entryBlocks.size(); ++blockIndex) {
        const auto &actionBlock = entryBlocks[blockIndex];

        LOG_DEBUG("W3C SCXML 3.8: Executing onentry block {}/{} with {} actions for state: {}", blockIndex + 1,
                  entryBlocks.size(), actionBlock.size(), stateId);

        // Execute all actions in this block
        for (const auto &action : actionBlock) {
            if (!action) {
                LOG_WARN("Null onentry action found in state: {}", stateId);
                continue;
            }

            LOG_DEBUG("StateMachine: Executing onentry action: {} in state: {}", action->getActionType(), stateId);

            // Create execution context for the action
            if (actionExecutor_) {
                auto sharedActionExecutor =
                    std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
                ExecutionContextImpl context(sharedActionExecutor, sessionId_);

                // Execute the action
                if (!action->execute(context)) {
                    LOG_WARN("StateMachine: Failed to execute onentry action: {} in block {}/{} - W3C SCXML 3.8: "
                             "stopping remaining actions in THIS block only",
                             action->getActionType(), blockIndex + 1, entryBlocks.size());
                    // W3C SCXML 3.8: If error occurs, stop processing remaining actions IN THIS BLOCK
                    // but CONTINUE with next onentry handler block
                    break;
                } else {
                    LOG_DEBUG("StateMachine: Successfully executed onentry action: {} in state: {}",
                              action->getActionType(), stateId);
                }
            } else {
                LOG_ERROR("Cannot execute onentry action: ActionExecutor is null");
            }
        }

        // Continue with next block even if this block had failures
        // W3C SCXML 3.8: Each onentry handler is independent
    }

    // W3C SCXML compliance: Restore immediate mode (but DON'T process queued events yet)
    // Events must be processed AFTER the entire state tree entry completes, not during onentry
    // This ensures parent and child states are both active before processing raised events
    if (eventRaiser_) {
        auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
        if (eventRaiserImpl) {
            eventRaiserImpl->setImmediateMode(true);
            LOG_DEBUG(
                "SCXML compliance: Restored immediate mode (events will be processed after state entry completes)");
        }
    }

    // W3C SCXML: Defer invoke execution until after state entry completes
    // This ensures proper timing with transition actions and pre-registration pattern
    const auto &invokes = stateNode->getInvoke();
    if (!invokes.empty()) {
        LOG_DEBUG("StateMachine: Deferring {} invokes for state: {}", invokes.size(), stateId);
        deferInvokeExecution(stateId, invokes);
    } else {
        LOG_DEBUG("StateMachine: No invokes to defer for state: {}", stateId);
    }
}

// EventDispatcher management
void StateMachine::setEventDispatcher(std::shared_ptr<IEventDispatcher> eventDispatcher) {
    eventDispatcher_ = eventDispatcher;

    // Pass EventDispatcher to ActionExecutor for send actions
    if (actionExecutor_) {
        auto actionExecutorImpl = std::dynamic_pointer_cast<ActionExecutorImpl>(actionExecutor_);
        if (actionExecutorImpl) {
            actionExecutorImpl->setEventDispatcher(eventDispatcher);
            LOG_DEBUG("StateMachine: EventDispatcher passed to ActionExecutor for session: {}", sessionId_);
        }
    }

    // Pass EventDispatcher to InvokeExecutor for child session management
    if (invokeExecutor_) {
        invokeExecutor_->setEventDispatcher(eventDispatcher);
        LOG_DEBUG("StateMachine: EventDispatcher passed to InvokeExecutor for session: {}", sessionId_);

        // W3C SCXML Test 192: Set parent StateMachine for completion callback state checking
        // Only set if this StateMachine is managed by shared_ptr (not during construction)
        // This will be set later in executeInvoke() when actually needed
    }
}

// W3C SCXML 6.5: Completion callback management
void StateMachine::setCompletionCallback(CompletionCallback callback) {
    completionCallback_ = callback;
    LOG_DEBUG("StateMachine: Completion callback {} for session: {}", callback ? "set" : "cleared", sessionId_);
}

// EventRaiser management
void StateMachine::setEventRaiser(std::shared_ptr<IEventRaiser> eventRaiser) {
    LOG_DEBUG("StateMachine: setEventRaiser called for session: {}", sessionId_);
    eventRaiser_ = eventRaiser;

    // SCXML W3C compliance: Set EventRaiser callback to StateMachine's processEvent
    // This allows events generated by raise actions to actually trigger state transitions
    if (eventRaiser_) {
        auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
        if (eventRaiserImpl) {
            LOG_DEBUG("StateMachine: EventRaiser callback setup - EventRaiser instance: {}, StateMachine instance: {}",
                      (void *)eventRaiserImpl.get(), (void *)this);
            // Set StateMachine's processEvent method as EventRaiser callback
            eventRaiserImpl->setEventCallback(
                [this](const std::string &eventName, const std::string &eventData) -> bool {
                    if (isRunning_) {
                        LOG_DEBUG("EventRaiser callback: StateMachine::processEvent called - event: '{}', data: '{}', "
                                  "StateMachine instance: {}",
                                  eventName, eventData, (void *)this);
                        // Use 2-parameter version (no originSessionId from old callback)
                        auto result = processEvent(eventName, eventData);
                        LOG_DEBUG("EventRaiser callback: processEvent result - success: {}, state transition: {} -> {}",
                                  result.success, result.fromState, result.toState);
                        return result.success;
                    } else {
                        LOG_WARN("EventRaiser callback: StateMachine not running - ignoring event '{}'", eventName);
                        return false;
                    }
                });
            LOG_DEBUG("StateMachine: EventRaiser callback set to processEvent - session: {}, EventRaiser instance: {}",
                      sessionId_, (void *)eventRaiserImpl.get());
        }
    }

    // Register EventRaiser with JSEngine for #_invokeid target support
    // Use EventRaiserService for centralized registration
    if (eventRaiser_) {
        if (EventRaiserService::getInstance().registerEventRaiser(sessionId_, eventRaiser_)) {
            LOG_DEBUG("StateMachine: EventRaiser registered via Service for session: {}", sessionId_);
        } else {
            LOG_DEBUG("StateMachine: EventRaiser registration deferred or already exists for session: {}", sessionId_);
        }
    }

    // Pass EventRaiser to ActionExecutor if it exists (during build phase)
    if (actionExecutor_) {
        actionExecutor_->setEventRaiser(eventRaiser);
        LOG_DEBUG("StateMachine: EventRaiser passed to ActionExecutor for session: {}", sessionId_);
    }
    // Note: If ActionExecutor doesn't exist yet, it will be set during loadSCXMLFromString
}

std::shared_ptr<IEventDispatcher> StateMachine::getEventDispatcher() const {
    return eventDispatcher_;
}

void StateMachine::deferInvokeExecution(const std::string &stateId,
                                        const std::vector<std::shared_ptr<IInvokeNode>> &invokes) {
    LOG_DEBUG("StateMachine: Deferring {} invokes for state: {} in session: {}", invokes.size(), stateId, sessionId_);

    // Log each invoke being deferred
    for (size_t i = 0; i < invokes.size(); ++i) {
        const auto &invoke = invokes[i];
        std::string invokeId = invoke ? invoke->getId() : "null";
        std::string invokeType = invoke ? invoke->getType() : "null";
        LOG_DEBUG("StateMachine: DETAILED DEBUG - Deferring invoke[{}]: id='{}', type='{}'", i, invokeId, invokeType);
    }

    DeferredInvoke deferred;
    deferred.stateId = stateId;
    deferred.invokes = invokes;

    // Thread-safe access to pendingInvokes_
    std::lock_guard<std::mutex> lock(pendingInvokesMutex_);
    size_t beforeSize = pendingInvokes_.size();
    pendingInvokes_.push_back(std::move(deferred));

    LOG_DEBUG("StateMachine: DETAILED DEBUG - Pending invokes count: {} -> {}", beforeSize, pendingInvokes_.size());
}

void StateMachine::executePendingInvokes() {
    // W3C SCXML Test 192: Set parent StateMachine before executing invokes (requires shared_ptr context)
    // This is safe here because executePendingInvokes() is only called when StateMachine is already in shared_ptr
    // context
    if (invokeExecutor_) {
        try {
            invokeExecutor_->setParentStateMachine(shared_from_this());
            LOG_DEBUG(
                "StateMachine: Parent StateMachine set in InvokeExecutor before executing invokes for session: {}",
                sessionId_);
        } catch (const std::bad_weak_ptr &e) {
            LOG_WARN("StateMachine: Cannot set parent StateMachine - not managed by shared_ptr yet for session: {}",
                     sessionId_);
        }
    }

    // Thread-safe copy of pending invokes
    std::vector<DeferredInvoke> invokesToExecute;
    {
        std::lock_guard<std::mutex> lock(pendingInvokesMutex_);
        if (pendingInvokes_.empty()) {
            LOG_DEBUG("StateMachine: No pending invokes to execute for session: {}", sessionId_);
            return;
        }

        LOG_DEBUG("StateMachine: Found {} pending invokes to execute for session: {}", pendingInvokes_.size(),
                  sessionId_);

        LOG_DEBUG("StateMachine: DETAILED DEBUG - Executing {} pending invokes for session {}", pendingInvokes_.size(),
                  sessionId_);

        // Log all pending invokes for debugging
        for (size_t i = 0; i < pendingInvokes_.size(); ++i) {
            const auto &deferred = pendingInvokes_[i];
            LOG_DEBUG("StateMachine: DETAILED DEBUG - Pending invoke[{}]: stateId='{}', invokeCount={}", i,
                      deferred.stateId, deferred.invokes.size());
            for (size_t j = 0; j < deferred.invokes.size(); ++j) {
                const auto &invoke = deferred.invokes[j];
                std::string invokeId = invoke ? invoke->getId() : "null";
                LOG_DEBUG("StateMachine: DETAILED DEBUG - Pending invoke[{}][{}]: id='{}', type='{}'", i, j, invokeId,
                          invoke ? invoke->getType() : "null");
            }
        }

        // Copy to execute outside of lock
        invokesToExecute = std::move(pendingInvokes_);
        pendingInvokes_.clear();
        LOG_DEBUG("StateMachine: Cleared all pending invokes");
    }

    // Execute invokes outside of lock to avoid deadlock
    for (const auto &deferred : invokesToExecute) {
        // W3C SCXML Test 252: Only execute invokes if their state is still active
        if (!isStateActive(deferred.stateId)) {
            LOG_DEBUG("StateMachine: Skipping {} deferred invokes for inactive state: {}", deferred.invokes.size(),
                      deferred.stateId);
            continue;
        }

        LOG_DEBUG("StateMachine: Executing {} deferred invokes for state: {}", deferred.invokes.size(),
                  deferred.stateId);

        if (invokeExecutor_) {
            bool invokeSuccess = invokeExecutor_->executeInvokes(deferred.invokes, sessionId_);
            if (invokeSuccess) {
                LOG_DEBUG("StateMachine: Successfully executed all deferred invokes for state: {}", deferred.stateId);
            } else {
                LOG_ERROR("StateMachine: Failed to execute some deferred invokes for state: {}", deferred.stateId);
                // W3C SCXML: Continue execution even if invokes fail
            }
        } else {
            LOG_ERROR("StateMachine: Cannot execute deferred invokes - InvokeExecutor is null");
        }
    }
}

// W3C SCXML 3.7 & 5.5: Handle compound state completion when final child is entered
void StateMachine::handleCompoundStateFinalChild(const std::string &finalStateId) {
    if (!model_) {
        return;
    }

    auto finalState = model_->findStateById(finalStateId);
    if (!finalState || !finalState->isFinalState()) {
        return;
    }

    // Get parent state
    auto parent = finalState->getParent();
    if (!parent) {
        return;  // Top-level final state, no done.state event for compound
    }

    // Only generate done.state for compound (non-parallel) parent states
    if (parent->getType() == Type::PARALLEL) {
        return;  // Parallel states handled separately
    }

    // W3C SCXML 3.7: Generate done.state.{parentId} event
    std::string parentId = parent->getId();
    std::string doneEventName = "done.state." + parentId;

    LOG_INFO("W3C SCXML 3.7: Compound state '{}' completed, generating done.state event: {}", parentId, doneEventName);

    // W3C SCXML 5.5 & 5.7: Evaluate donedata and construct event data
    // If evaluation fails (error.execution raised), do not generate done.state event
    std::string eventData;
    if (!evaluateDoneData(finalStateId, eventData)) {
        LOG_DEBUG("W3C SCXML 5.7: Donedata evaluation failed, skipping done.state event generation");
        return;
    }

    // W3C SCXML: Queue the done.state event (not immediate processing)
    // This allows error.execution events from donedata evaluation to be processed first
    if (isRunning_ && eventRaiser_) {
        eventRaiser_->raiseEvent(doneEventName, eventData);
        LOG_DEBUG("W3C SCXML: Queued done.state event: {}", doneEventName);
    }
}

// Helper: Escape special characters in JSON strings
std::string StateMachine::escapeJsonString(const std::string &str) {
    std::ostringstream escaped;
    for (char c : str) {
        switch (c) {
        case '"':
            escaped << "\\\"";
            break;
        case '\\':
            escaped << "\\\\";
            break;
        case '\n':
            escaped << "\\n";
            break;
        case '\r':
            escaped << "\\r";
            break;
        case '\t':
            escaped << "\\t";
            break;
        case '\b':
            escaped << "\\b";
            break;
        case '\f':
            escaped << "\\f";
            break;
        default:
            escaped << c;
            break;
        }
    }
    return escaped.str();
}

// Helper: Convert ScriptValue to JSON representation
std::string StateMachine::convertScriptValueToJson(const ScriptValue &value, bool quoteStrings) {
    if (std::holds_alternative<std::string>(value)) {
        const std::string &str = std::get<std::string>(value);
        if (quoteStrings) {
            return "\"" + escapeJsonString(str) + "\"";
        }
        return str;
    } else if (std::holds_alternative<double>(value)) {
        return std::to_string(std::get<double>(value));
    } else if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    } else if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    }
    return "null";
}

/**
 * W3C SCXML 5.5 & 5.7: Evaluate donedata and return JSON event data
 *
 * Handles two types of param errors with different behaviors:
 *
 * 1. Structural Error (empty location=""):
 *    - Indicates malformed SCXML document
 *    - Raises error.execution event
 *    - Returns false to prevent done.state event generation
 *    - Used when param has no location/expr attribute
 *
 * 2. Runtime Error (invalid expression like "foo"):
 *    - Indicates runtime evaluation failure
 *    - Raises error.execution event
 *    - Ignores the failed param and continues with others
 *    - Returns true to generate done.state event with partial/empty data
 *    - Used when param expression evaluation fails
 *
 * This distinction ensures:
 * - Structural errors fail fast (no done.state)
 * - Runtime errors are recoverable (done.state with available data)
 *
 * @param finalStateId The ID of the final state
 * @param outEventData Output parameter for JSON event data
 * @return false if structural error (prevents done.state), true otherwise
 */
bool StateMachine::evaluateDoneData(const std::string &finalStateId, std::string &outEventData) {
    outEventData = "";

    if (!model_) {
        return true;  // No donedata to evaluate
    }

    auto finalState = model_->findStateById(finalStateId);
    if (!finalState) {
        return true;  // No donedata to evaluate
    }

    const auto &doneData = finalState->getDoneData();

    // Check if donedata has content
    if (!doneData.getContent().empty()) {
        // W3C SCXML 5.5: <content> sets the entire _event.data value
        std::string content = doneData.getContent();
        LOG_DEBUG("W3C SCXML 5.5: Evaluating donedata content: '{}'", content);

        // Evaluate content as expression
        auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, content);
        auto result = future.get();

        if (RSM::JSEngine::isSuccess(result)) {
            // Convert result to JSON string using helper
            const auto &value = result.getInternalValue();
            outEventData = convertScriptValueToJson(value, false);

            // For objects/arrays (null case), use original content as fallback
            if (outEventData == "null" && !std::holds_alternative<ScriptNull>(value)) {
                outEventData = content;
            }
            return true;
        } else {
            LOG_WARN("W3C SCXML 5.5: Failed to evaluate donedata content: {}", result.getErrorMessage());
            outEventData = content;  // Use literal content as fallback
            return true;
        }
    }

    // Check if donedata has params
    const auto &params = doneData.getParams();
    if (!params.empty()) {
        // W3C SCXML 5.5: <param> elements create an object with name:value pairs
        LOG_DEBUG("W3C SCXML 5.5: Evaluating {} donedata params", params.size());

        std::ostringstream jsonBuilder;
        jsonBuilder << "{";

        bool first = true;
        for (const auto &param : params) {
            const std::string &paramName = param.first;
            const std::string &paramExpr = param.second;

            // W3C SCXML 5.7: Empty location is invalid (structural error)
            // Must raise error.execution and prevent done.state event generation
            if (paramExpr.empty()) {
                LOG_ERROR("W3C SCXML 5.7: Empty param location/expression for param '{}'", paramName);

                if (eventRaiser_) {
                    eventRaiser_->raiseEvent("error.execution", "Empty param location or expression: " + paramName);
                }

                // W3C SCXML 5.7: Return false to skip done.state event generation
                return false;
            }

            // Evaluate param expression
            auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, paramExpr);
            auto result = future.get();

            if (RSM::JSEngine::isSuccess(result)) {
                // W3C SCXML 5.7: Successfully evaluated param
                if (!first) {
                    jsonBuilder << ",";
                }
                first = false;

                const auto &value = result.getInternalValue();
                // Use helper to convert value to JSON with proper escaping
                jsonBuilder << "\"" << escapeJsonString(paramName) << "\":" << convertScriptValueToJson(value, true);
            } else {
                // W3C SCXML 5.7: Invalid location or expression (runtime error)
                // Must raise error.execution and ignore this param, but continue with others
                LOG_ERROR("W3C SCXML 5.7: Failed to evaluate param '{}' expr/location '{}': {}", paramName, paramExpr,
                          result.getErrorMessage());

                if (eventRaiser_) {
                    eventRaiser_->raiseEvent("error.execution",
                                             "Invalid param location or expression: " + paramName + " = " + paramExpr);
                }
                // Continue to next param without adding this one
            }
        }

        jsonBuilder << "}";
        outEventData = jsonBuilder.str();
        return true;
    }

    // No donedata
    return true;
}

// W3C SCXML: Get proper ancestors of a state (all ancestors excluding the state itself)
std::vector<std::string> StateMachine::getProperAncestors(const std::string &stateId) const {
    std::vector<std::string> ancestors;

    if (!model_) {
        return ancestors;
    }

    auto stateNode = model_->findStateById(stateId);
    if (!stateNode) {
        return ancestors;
    }

    IStateNode *current = stateNode->getParent();
    while (current != nullptr) {
        ancestors.push_back(current->getId());
        current = current->getParent();
    }

    return ancestors;
}

// W3C SCXML: Check if stateId is a descendant of ancestorId
bool StateMachine::isDescendant(const std::string &stateId, const std::string &ancestorId) const {
    if (!model_ || stateId.empty() || ancestorId.empty()) {
        return false;
    }

    if (stateId == ancestorId) {
        return false;  // A state is not its own descendant
    }

    auto stateNode = model_->findStateById(stateId);
    if (!stateNode) {
        return false;
    }

    IStateNode *current = stateNode->getParent();
    while (current != nullptr) {
        if (current->getId() == ancestorId) {
            return true;
        }
        current = current->getParent();
    }

    return false;
}

// W3C SCXML: Find Lowest Common Ancestor of source and target states
int StateMachine::getStateDocumentPosition(const std::string &stateId) const {
    // W3C SCXML 3.13: Get document order position for state
    // Uses depth-first pre-order traversal to assign positions
    if (!model_) {
        return -1;
    }

    // Helper to recursively assign positions
    int position = 0;
    std::function<int(IStateNode *, const std::string &)> findPosition = [&](IStateNode *node,
                                                                             const std::string &targetId) -> int {
        if (!node) {
            return -1;
        }

        if (node->getId() == targetId) {
            return position;
        }

        position++;

        // Depth-first pre-order: visit children
        const auto &children = node->getChildren();
        for (const auto &child : children) {
            int result = findPosition(child.get(), targetId);
            if (result >= 0) {
                return result;
            }
        }

        return -1;
    };

    // Start from root state
    auto rootState = model_->getRootState();
    if (!rootState) {
        return -1;
    }

    return findPosition(rootState.get(), stateId);
}

std::string StateMachine::findLCA(const std::string &sourceStateId, const std::string &targetStateId) const {
    if (!model_) {
        return "";
    }

    // Get all ancestors of source (including source itself for comparison)
    std::vector<std::string> sourceAncestors;
    sourceAncestors.reserve(16);  // Performance: Reserve typical depth to avoid reallocation

    auto sourceNode = model_->findStateById(sourceStateId);
    if (sourceNode) {
        IStateNode *current = sourceNode;
        while (current != nullptr) {
            sourceAncestors.push_back(current->getId());
            current = current->getParent();
        }
    }

    // Walk up from target until we find a common ancestor
    auto targetNode = model_->findStateById(targetStateId);
    if (targetNode) {
        // W3C SCXML: Start from target itself (target can be the LCA)
        IStateNode *current = targetNode;
        while (current != nullptr) {
            std::string currentId = current->getId();
            // Check if this ancestor is in source's ancestor chain
            if (std::find(sourceAncestors.begin(), sourceAncestors.end(), currentId) != sourceAncestors.end()) {
                return currentId;  // Found LCA
            }
            current = current->getParent();
        }
    }

    // No common ancestor found (shouldn't happen in valid SCXML)
    return "";
}

// W3C SCXML: Compute exit set for transition from source to target
std::vector<std::string> StateMachine::computeExitSet(const std::string &sourceStateId,
                                                      const std::string &targetStateId) const {
    std::vector<std::string> exitSet;
    exitSet.reserve(8);  // Performance: Reserve typical exit set size to avoid reallocation

    if (!model_ || sourceStateId.empty()) {
        return exitSet;
    }

    // If target is empty (targetless transition), exit source only
    if (targetStateId.empty()) {
        exitSet.push_back(sourceStateId);
        return exitSet;
    }

    // Find LCA (Lowest Common Ancestor)
    std::string lca = findLCA(sourceStateId, targetStateId);

    // Collect all states from source up to (but not including) LCA
    // These are the states we need to exit
    auto sourceNode = model_->findStateById(sourceStateId);
    if (!sourceNode) {
        return exitSet;
    }

    IStateNode *current = sourceNode;
    while (current != nullptr) {
        std::string currentId = current->getId();

        // Stop when we reach LCA (don't include LCA in exit set)
        if (currentId == lca) {
            break;
        }

        exitSet.push_back(currentId);
        current = current->getParent();
    }

    LOG_DEBUG("W3C SCXML: computeExitSet({} -> {}) = {} states, LCA = '{}'", sourceStateId, targetStateId,
              exitSet.size(), lca);

    return exitSet;
}

}  // namespace RSM
