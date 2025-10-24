#pragma once

#include "common/EventMetadataHelper.h"
#include "common/EventTypeHelper.h"
#include "common/HierarchicalStateHelper.h"
#include "common/HistoryHelper.h"
#include "common/Logger.h"
#include "core/EventMetadata.h"
#include "core/EventProcessingAlgorithms.h"
#include "core/EventQueueAdapters.h"
#include "core/EventQueueManager.h"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace RSM::Static {

/**
 * @brief Template-based SCXML execution engine for static code generation
 *
 * This engine implements the core SCXML execution semantics (event queue management,
 * entry/exit actions, transitions) while delegating state-specific logic to the
 * StatePolicy template parameter.
 *
 * Key SCXML standards implemented:
 * - Internal event queue with FIFO ordering (W3C SCXML 3.12.1)
 * - Entry/exit action execution (W3C SCXML 3.7, 3.8)
 * - Event processing loop (W3C SCXML D.1)
 *
 * @tparam StatePolicy Policy class providing state-specific implementations
 *         Must provide: State, Event enums, transition logic, action execution
 */
template <typename StatePolicy> class StaticExecutionEngine {
    friend StatePolicy;

public:
    using State = typename StatePolicy::State;
    using Event = typename StatePolicy::Event;

    /**
     * @brief Event with metadata for W3C SCXML 5.10 compliance
     *
     * Wraps Event enum with metadata (origin, sendid, data, type) to support
     * _event.origin, _event.sendid, _event.data, _event.type fields.
     *
     * @example Constructor Pattern (required for nested template types)
     * @code
     * // Create event with data and sendId
     * engine.raise(EventWithMetadata(Event::Error_execution, errorMsg, "", sendId));
     *
     * // Create external event with all metadata
     * externalQueue_.raise(EventWithMetadata(
     *     Event::Foo,         // event
     *     "data",             // data
     *     "#_internal",       // origin
     *     sendId,             // sendId
     *     "external"          // type
     * ));
     * @endcode
     */
    struct EventWithMetadata {
        Event event;
        std::string data;
        std::string origin;      // W3C SCXML 5.10.1: _event.origin
        std::string sendId;      // W3C SCXML 5.10.1: _event.sendid
        std::string type;        // W3C SCXML 5.10.1: _event.type
        std::string originType;  // W3C SCXML 5.10.1: _event.origintype
        std::string invokeId;    // W3C SCXML 5.10.1: _event.invokeid

        // Default constructor for aggregate initialization
        EventWithMetadata() = default;

        // Constructor with positional parameters (event, data, origin, sendId, type, originType, invokeId)
        EventWithMetadata(Event e, const std::string &d = "", const std::string &o = "", const std::string &s = "",
                          const std::string &t = "", const std::string &ot = "", const std::string &i = "")
            : event(e), data(d), origin(o), sendId(s), type(t), originType(ot), invokeId(i) {}
    };

private:
    /**
     * @brief Handle hierarchical exit and entry for state transition
     *
     * @details
     * ARCHITECTURE.md: Extract duplicate code from processEventQueues
     * W3C SCXML 3.12: Compute LCA and execute hierarchical exit/entry
     *
     * @param oldState State before transition
     * @param newState State after transition
     * @param preTransitionStates Active states before transition (for history recording)
     */
    template <typename TransitionActionFn = decltype([] {})>
    void handleHierarchicalTransition(State oldState, State newState, const std::vector<State> &preTransitionStates,
                                      TransitionActionFn &&transitionAction = {}) {
        LOG_DEBUG("AOT handleHierarchicalTransition: Transition {} -> {}", static_cast<int>(oldState),
                  static_cast<int>(newState));

        // W3C SCXML 3.12: Find LCA and build exit/entry chains
        auto lca = RSM::Common::HierarchicalStateHelper<StatePolicy>::findLCA(oldState, newState);

        if (lca.has_value()) {
            // W3C SCXML 3.13: Exit states from oldState up to (but not including) LCA
            auto exitChain = RSM::Common::HierarchicalStateHelper<StatePolicy>::buildExitChain(oldState, lca.value());
            for (const auto &state : exitChain) {
                LOG_DEBUG("AOT handleHierarchicalTransition: Hierarchical exit state {}", static_cast<int>(state));
                executeOnExit(state, preTransitionStates);
            }

            // W3C SCXML 3.13: Execute transition actions AFTER exit, BEFORE entry
            LOG_DEBUG("AOT handleHierarchicalTransition: Executing transition actions");
            transitionAction();

            // W3C SCXML 3.13: Enter states from LCA down to newState (including initial children)
            auto entryChain =
                RSM::Common::HierarchicalStateHelper<StatePolicy>::buildEntryChainFromParent(newState, lca.value());
            for (const auto &state : entryChain) {
                LOG_DEBUG("AOT handleHierarchicalTransition: Hierarchical entry state {}", static_cast<int>(state));
                executeOnEntry(state);
            }
        } else {
            // No LCA (top-level transition) - exit all ancestors of oldState
            LOG_DEBUG("AOT handleHierarchicalTransition: No LCA (top-level transition)");

            State current = oldState;
            while (true) {
                LOG_DEBUG("AOT handleHierarchicalTransition: Exit state {} (to root)", static_cast<int>(current));
                executeOnExit(current, preTransitionStates);

                auto parent = StatePolicy::getParent(current);
                if (!parent.has_value()) {
                    break;  // Reached root
                }
                current = parent.value();
            }

            // W3C SCXML 3.13: Execute transition actions AFTER exit, BEFORE entry
            LOG_DEBUG("AOT handleHierarchicalTransition: Executing transition actions (no LCA)");
            transitionAction();

            // Enter full hierarchy from root to newState
            auto entryChain = RSM::Common::HierarchicalStateHelper<StatePolicy>::buildEntryChain(newState);
            for (const auto &state : entryChain) {
                LOG_DEBUG("AOT handleHierarchicalTransition: Entry state {} (from root)", static_cast<int>(state));
                executeOnEntry(state);
            }
        }
    }

    State currentState_;
    RSM::Core::EventQueueManager<EventWithMetadata>
        internalQueue_;  // W3C SCXML C.1: Internal event queue (high priority)
    RSM::Core::EventQueueManager<EventWithMetadata>
        externalQueue_;  // W3C SCXML C.1: External event queue (low priority)
    bool isRunning_ = false;
    std::function<void()> completionCallback_;  // W3C SCXML 6.4: Callback for done.invoke

protected:
    StatePolicy policy_;  // Policy instance for stateful policies

protected:
    /**
     * @brief Raise an internal event with metadata (W3C SCXML C.1)
     *
     * Preferred API for raising events with complete metadata.
     * Uses EventWithMetadata struct for better readability and extensibility.
     *
     * @param metadata Complete event metadata including all W3C SCXML 5.10.1 fields
     *
     * @example Constructor Pattern (required for nested template types)
     * @code
     * // Raise error.execution with data and sendId
     * engine.raise(typename Engine::EventWithMetadata(
     *     Event::Error_execution,  // event
     *     errorMsg,                // data
     *     "#_internal",            // origin
     *     sendId,                  // sendId
     *     "platform"               // type
     * ));
     *
     * // Raise simple event
     * engine.raise(typename Engine::EventWithMetadata(Event::Done_invoke));
     * @endcode
     */
    void raise(EventWithMetadata metadata) {
        // W3C SCXML C.1: Enqueue event with metadata
        internalQueue_.raise(std::move(metadata));
    }

public:
    /**
     * @brief Raise an external event (W3C SCXML C.1, 6.2)
     *
     * External events are placed at the back of the external event queue.
     * They are processed after all internal events have been consumed.
     *
     * Used by:
     * - <send> without target (W3C SCXML 6.2)
     * - <send> with external targets (not #_internal)
     * - <send target="#_parent"> from child state machines (W3C SCXML 6.2)
     *
     * W3C SCXML C.1 (test189): External queue has lower priority than internal queue.
     *
     * @param event Event to raise externally
     * @param eventData Optional event data as JSON string (W3C SCXML 5.10)
     */
    void raiseExternal(Event event, const std::string &eventData = "", const std::string &origin = "") {
        // W3C SCXML C.1: Enqueue event with metadata (origin, data, sendid, type)
        externalQueue_.raise(EventWithMetadata(event, eventData, origin, "", "external"));

        // W3C SCXML 5.10.1: Mark next event as external for _event.type (test331)
        if constexpr (requires { policy_.nextEventIsExternal_; }) {
            policy_.nextEventIsExternal_ = true;
        }
    }

    /**
     * @brief Raise external event with full metadata (W3C SCXML 6.4.1)
     *
     * Used for child-to-parent communication where invokeid must be preserved.
     * W3C SCXML 6.4.1 (test338): Events from child to parent must include invokeid.
     *
     * @param eventWithMetadata Event with metadata (including invokeid)
     */
    void raiseExternal(const EventWithMetadata &eventWithMetadata) {
        // W3C SCXML C.1: Enqueue event with full metadata
        LOG_DEBUG("AOT raiseExternal: Enqueuing external event with metadata (event={}, invokeId='{}')",
                  static_cast<int>(eventWithMetadata.event), eventWithMetadata.invokeId);
        externalQueue_.raise(eventWithMetadata);

        // W3C SCXML 5.10.1: Mark next event as external for _event.type (test331)
        if constexpr (requires { policy_.nextEventIsExternal_; }) {
            policy_.nextEventIsExternal_ = true;
        }
    }

protected:
    /**
     * @brief Execute entry actions for a state (W3C SCXML 3.7)
     *
     * Entry actions are executable content that runs when entering a state.
     * This includes <onentry> blocks which may contain <raise>, <assign>, etc.
     *
     * Supports both static (stateless) and non-static (stateful) policies.
     * Static methods can also be called through an instance in C++.
     *
     * @param state State being entered
     */
    void executeOnEntry(State state) {
        // Call through policy instance (works for both static and non-static)
        policy_.executeEntryActions(state, *this);
    }

    /**
     * @brief Execute exit actions for a state (W3C SCXML 3.8)
     *
     * Exit actions are executable content that runs when exiting a state.
     * This includes <onexit> blocks.
     *
     * Supports both static (stateless) and non-static (stateful) policies.
     * Static methods can also be called through an instance in C++.
     *
     * @param state State being exited
     */
    void executeOnExit(State state, const std::vector<State> &activeStatesBeforeTransition) {
        // Call through policy instance with pre-transition active states
        policy_.executeExitActions(state, *this, activeStatesBeforeTransition);
    }

    /**
     * @brief Process both internal and external event queues (W3C SCXML D.1 Algorithm)
     *
     * Processes all queued internal and external events in priority order.
     * Internal events are processed first (high priority), then external events.
     *
     * W3C SCXML C.1 (test189): Internal queue (#_internal target) has higher
     * priority than external queue (no target or external targets).
     *
     * Uses shared EventProcessingAlgorithms for W3C-compliant processing.
     * This ensures Interpreter and AOT engines use identical logic.
     *
     * Supports both static (stateless) and non-static (stateful) policies.
     * Static methods can also be called through an instance in C++.
     */
    void processEventQueues() {
        LOG_DEBUG("AOT processEventQueues: Starting internal queue processing");
        // W3C SCXML C.1: Process internal queue first (high priority)
        RSM::Core::AOTEventQueue<EventWithMetadata> internalAdapter(internalQueue_);
        RSM::Core::EventProcessingAlgorithms::processInternalEventQueue(
            internalAdapter, [this](const EventWithMetadata &eventWithMeta) {
                // W3C SCXML 5.10: Set pending event fields from metadata using EventMetadataHelper
                // ARCHITECTURE.md: Zero Duplication - shared logic with EventMetadataHelper
                Event event = eventWithMeta.event;
                RSM::Common::EventMetadataHelper::populatePolicyFromMetadata<StatePolicy, Event>(policy_,
                                                                                                 eventWithMeta);

                LOG_DEBUG("AOT processEventQueues: Processing internal event, currentState={}",
                          static_cast<int>(currentState_));
                // Process event through transition logic
                State oldState = currentState_;
                std::vector<State> preTransitionStates =
                    getActiveStates();  // W3C SCXML 3.11: Capture before transition
                // Call through policy instance (works for both static and non-static)
                bool transitionTaken = policy_.processTransition(currentState_, event, *this);
                LOG_DEBUG(
                    "AOT processEventQueues (internal): processTransition returned {}, oldState={}, currentState={}",
                    transitionTaken, static_cast<int>(oldState), static_cast<int>(currentState_));
                if (transitionTaken) {
                    // Transition occurred: execute hierarchical exit/entry actions
                    if (oldState != currentState_) {
                        LOG_DEBUG("AOT processEventQueues: State transition {} -> {}", static_cast<int>(oldState),
                                  static_cast<int>(currentState_));

                        // ARCHITECTURE.md: Zero Duplication - use shared helper
                        // W3C SCXML 3.13: Pass transition action callback for correct execution order
                        handleHierarchicalTransition(oldState, currentState_, preTransitionStates,
                                                     [this] { policy_.executeTransitionActions(*this); });

                        LOG_DEBUG("AOT processEventQueues: Calling checkEventlessTransitions after state entry");
                        // W3C SCXML 3.13: Check eventless transitions immediately after state entry
                        // This ensures guards evaluate BEFORE queued error.execution events are processed
                        checkEventlessTransitions();
                        LOG_DEBUG("AOT processEventQueues: Returned from checkEventlessTransitions");
                    } else {
                        // W3C SCXML 3.4: Internal transition - no state change, but execute actions
                        LOG_DEBUG("AOT processEventQueues: Internal transition in state {}",
                                  static_cast<int>(currentState_));
                        policy_.executeTransitionActions(*this);
                    }
                }
                return true;  // Continue processing
            });

        // W3C SCXML C.1: Process external queue second (low priority)
        RSM::Core::AOTEventQueue<EventWithMetadata> externalAdapter(externalQueue_);
        RSM::Core::EventProcessingAlgorithms::processInternalEventQueue(
            externalAdapter, [this](const EventWithMetadata &eventWithMeta) {
                // W3C SCXML 5.10: Set pending event fields from metadata using EventMetadataHelper
                // ARCHITECTURE.md: Zero Duplication - shared logic with EventMetadataHelper
                Event event = eventWithMeta.event;
                RSM::Common::EventMetadataHelper::populatePolicyFromMetadata<StatePolicy, Event>(policy_,
                                                                                                 eventWithMeta);

                // Process event through transition logic
                State oldState = currentState_;
                std::vector<State> preTransitionStates =
                    getActiveStates();  // W3C SCXML 3.11: Capture before transition
                // Call through policy instance (works for both static and non-static)
                bool transitionTaken = policy_.processTransition(currentState_, event, *this);
                LOG_DEBUG(
                    "AOT processEventQueues (external): processTransition returned {}, oldState={}, currentState={}",
                    transitionTaken, static_cast<int>(oldState), static_cast<int>(currentState_));
                if (transitionTaken) {
                    // Transition occurred: execute hierarchical exit/entry actions
                    if (oldState != currentState_) {
                        // ARCHITECTURE.md: Zero Duplication - use shared helper
                        // W3C SCXML 3.13: Pass transition action callback for correct execution order
                        handleHierarchicalTransition(oldState, currentState_, preTransitionStates,
                                                     [this] { policy_.executeTransitionActions(*this); });

                        // W3C SCXML 3.13: Check eventless transitions immediately after state entry
                        checkEventlessTransitions();
                    } else {
                        // W3C SCXML 3.4: Internal transition - no state change, but execute actions
                        LOG_DEBUG("AOT processEventQueues: Internal transition in state {}",
                                  static_cast<int>(currentState_));
                        policy_.executeTransitionActions(*this);
                    }
                }
                return true;  // Continue processing
            });
    }

    /**
     * @brief Check for eventless transitions (W3C SCXML 3.13)
     *
     * Eventless transitions have no event attribute and are evaluated
     * immediately after entering a state. They are checked after all
     * internal events have been processed.
     *
     * Uses shared EventProcessingAlgorithms for W3C-compliant processing.
     * This ensures Interpreter and AOT engines use identical logic.
     *
     * Uses iteration instead of recursion to prevent stack overflow
     * and includes loop detection to prevent infinite cycles.
     */
    void checkEventlessTransitions() {
        LOG_DEBUG("AOT checkEventlessTransitions: Starting");
        static const int MAX_ITERATIONS = 100;  // Safety limit
        int iterations = 0;

        // W3C SCXML 3.13: Use shared algorithm (Single Source of Truth)
        // Note: Eventless transitions can raise new internal events, use internal queue
        RSM::Core::AOTEventQueue<EventWithMetadata> adapter(internalQueue_);

        while (iterations++ < MAX_ITERATIONS) {
            State oldState = currentState_;
            std::vector<State> preTransitionStates = getActiveStates();  // W3C SCXML 3.11: Capture before transition
            LOG_DEBUG("AOT checkEventlessTransitions: Iteration {}, currentState={}", iterations,
                      static_cast<int>(currentState_));

            // Call processTransition with default event for eventless transitions
            if (policy_.processTransition(currentState_, Event(), *this)) {
                // W3C SCXML 3.4: For parallel states, use actual transition source state
                State actualSourceState = policy_.lastTransitionSourceState_;
                LOG_DEBUG("AOT checkEventlessTransitions: Transition taken from {} to {} (actual source: {})",
                          static_cast<int>(oldState), static_cast<int>(currentState_),
                          static_cast<int>(actualSourceState));
                if (oldState != currentState_) {
                    // W3C SCXML Appendix D: For parallel states, executeMicrostep already handled exit/transition/entry
                    // Only call handleHierarchicalTransition for non-parallel state machines
                    if constexpr (!StatePolicy::HAS_PARALLEL_STATES) {
                        // ARCHITECTURE.MD: Zero Duplication - use shared helper
                        // W3C SCXML 3.13: Pass transition action callback for correct execution order
                        // W3C SCXML 3.4: Use actualSourceState for correct hierarchical exit/entry
                        handleHierarchicalTransition(actualSourceState, currentState_, preTransitionStates,
                                                     [this] { policy_.executeTransitionActions(*this); });
                    } else {
                        LOG_DEBUG("AOT checkEventlessTransitions: Parallel state machine - executeMicrostep handled "
                                  "all transitions");
                    }

                    // W3C SCXML C.1: Internal events are processed AFTER stable configuration is reached
                    // Continue loop to check for more eventless transitions first
                } else {
                    // Transition taken but state didn't change - stop
                    break;
                }
            } else {
                // W3C SCXML C.1: No eventless transition available - stable configuration reached
                // Internal events will be processed by caller (processEventQueues or step)
                break;
            }
        }

        if (iterations >= MAX_ITERATIONS) {
            // Eventless transition loop detected
            LOG_ERROR("StaticExecutionEngine: Eventless transition loop detected after {} iterations - stopping state "
                      "machine",
                      MAX_ITERATIONS);
            stop();
        }
    }

public:
    StaticExecutionEngine() : currentState_(StatePolicy::initialState()) {}

    /**
     * @brief Initialize state machine (W3C SCXML 3.2)
     *
     * Performs the initial configuration:
     * 1. Enter initial state (with hierarchical entry from root to leaf)
     * 2. Execute entry actions (may raise internal events)
     * 3. Process internal event queue
     * 4. Check for eventless transitions
     */
    void initialize() {
        isRunning_ = true;

        // W3C SCXML 5.3: Initialize datamodel before any state entry
        // This ensures error.execution events are raised immediately if initialization fails
        if constexpr (requires { policy_.initializeDataModel(*this); }) {
            policy_.initializeDataModel(*this);
        }

        // W3C SCXML 3.3: Use HierarchicalStateHelper for correct entry order
        auto entryChain = RSM::Common::HierarchicalStateHelper<StatePolicy>::buildEntryChain(currentState_);

        // Execute entry actions from root to leaf (ancestor first)
        for (const auto &state : entryChain) {
            executeOnEntry(state);
        }

        // W3C SCXML C.1: Macrostep completion loop
        // Process eventless transitions and internal events until stable configuration
        LOG_DEBUG("AOT initialize: After entry actions, starting macrostep completion loop");
        while (true) {
            // Process eventless transitions until stable
            checkEventlessTransitions();

            // Check if there are internal events to process
            if (!internalQueue_.hasEvents() && !externalQueue_.hasEvents()) {
                // Truly stable - no eventless transitions and no events
                break;
            }

            // Process internal/external events (may raise more events or cause transitions)
            processEventQueues();
        }
        LOG_DEBUG("AOT initialize: Macrostep completion loop finished - stable configuration reached");

        // W3C SCXML 6.4: Execute pending invokes after macrostep completes (ARCHITECTURE.md Zero Duplication)
        // Only invokes in entered-and-not-exited states execute (cancellation handled during state exits)
        if constexpr (requires { policy_.executePendingInvokes(*this); }) {
            policy_.executePendingInvokes(*this);

            // W3C SCXML 6.4: Process done.invoke events raised by immediately-completed children
            // Child state machines may reach final state during initialization and raise done.invoke
            // These events must be processed to allow parent transitions (e.g., s1 -> pass)
            LOG_DEBUG("AOT initialize: Processing events raised by completed invokes");
            processEventQueues();
            checkEventlessTransitions();
        }
    }

    /**
     * @brief Step the state machine (process pending events)
     *
     * W3C SCXML 6.4: For parent-child communication, parents must explicitly
     * step child state machines after sending events to ensure synchronous processing.
     *
     * This method processes all pending events in both internal and external queues.
     */
    void step() {
        processEventQueues();
        checkEventlessTransitions();

        // W3C SCXML 6.4: Invoke completion callback if in final state
        if (isInFinalState() && completionCallback_) {
            LOG_DEBUG("AOT step: Invoking completion callback for done.invoke");
            completionCallback_();
        }
    }

    /**
     * @brief Process an external event (W3C SCXML 3.12)
     *
     * External events are processed after all internal events have been
     * consumed. Each external event triggers a macrostep.
     *
     * Supports both static (stateless) and non-static (stateful) policies.
     * Static methods can also be called through an instance in C++.
     *
     * @param event External event to process
     */
    void processEvent(Event event) {
        if (!isRunning_) {
            return;
        }

        // Note: currentEventMetadata_ is only present in policies with invokes
        // If needed, it's managed by processTransition() and processEvent(Event, EventMetadata)

        State oldState = currentState_;
        std::vector<State> preTransitionStates = getActiveStates();  // W3C SCXML 3.11: Capture before transition
        // Call through policy instance (works for both static and non-static)
        if (policy_.processTransition(currentState_, event, *this)) {
            if (oldState != currentState_) {
                executeOnExit(oldState, preTransitionStates);
                executeOnEntry(currentState_);
                processEventQueues();
                checkEventlessTransitions();

                // W3C SCXML 6.4: Notify parent if reached final state
                if (isInFinalState() && completionCallback_) {
                    completionCallback_();
                }
            }
        }
    }

    /**
     * @brief Process an external event with metadata (W3C SCXML 5.10)
     *
     * External events with metadata support originSessionId for invoke finalize.
     * Used when events come from child sessions via invoke.
     *
     * @param event External event to process
     * @param metadata Event metadata (originSessionId, etc.)
     */
    void processEvent(Event event, const RSM::Core::EventMetadata &metadata) {
        if (!isRunning_) {
            return;
        }

        // Set event metadata for invoke processing
        policy_.currentEventMetadata_ = metadata;

        State oldState = currentState_;
        std::vector<State> preTransitionStates = getActiveStates();  // W3C SCXML 3.11: Capture before transition
        // Call through policy instance (works for both static and non-static)
        if (policy_.processTransition(currentState_, event, *this)) {
            if (oldState != currentState_) {
                executeOnExit(oldState, preTransitionStates);
                executeOnEntry(currentState_);
                processEventQueues();
                checkEventlessTransitions();

                // W3C SCXML 6.4: Notify parent if reached final state
                if (isInFinalState() && completionCallback_) {
                    completionCallback_();
                }
            }
        }
    }

    /**
     * @brief Get current state
     * @return Current active state
     */
    State getCurrentState() const {
        return currentState_;
    }

    /**
     * @brief Get all active states (W3C SCXML 3.11)
     *
     * For simple state machines (no parallel), returns vector with single current state.
     * For parallel state machines, would return all active states in parallel regions.
     *
     * Used by history recording logic to match Interpreter HistoryManager behavior.
     *
     * @return Vector of currently active states
     */
    std::vector<State> getActiveStates() const {
        // W3C SCXML 3.11: Use shared HistoryHelper for full active hierarchy (Zero Duplication Principle)
        // Returns [currentState, parent, grandparent, ...] for proper history recording
        return ::RSM::HistoryHelper::getActiveHierarchy(currentState_,
                                                        [](State s) { return StatePolicy::getParent(s); });
    }

    /**
     * @brief Check if in a final state (W3C SCXML 3.3)
     * @return true if current state is final
     */
    bool isInFinalState() const {
        return StatePolicy::isFinalState(currentState_);
    }

    /**
     * @brief Check if state machine is running
     * @return true if running (not stopped or completed)
     */
    bool isRunning() const {
        return isRunning_;
    }

    /**
     * @brief Stop state machine execution
     */
    void stop() {
        isRunning_ = false;
    }

    /**
     * @brief Tick scheduler and process ready internal events (W3C SCXML 6.2)
     *
     * For single-threaded AOT engines with delayed send support.
     * This method polls the event scheduler and processes any ready scheduled events
     * without injecting an external event. Should be called periodically in a polling
     * loop to allow delayed sends to fire at the correct time.
     *
     * Implementation: Calls processTransition to trigger scheduler check, which
     * raises ready scheduled events to the internal queue, then processes them.
     * The dummy event parameter won't match transitions in final states.
     */
    void tick() {
        if (!isRunning_ || isInFinalState()) {
            return;
        }

        // Trigger scheduler check by calling processTransition
        // Use Event() which is typically the first enum value (e.g., Wildcard)
        // This triggers the scheduler check in processTransition, which raises
        // any ready scheduled events to the internal queue.
        State oldState = currentState_;
        std::vector<State> preTransitionStates = getActiveStates();  // W3C SCXML 3.11: Capture before transition
        if (policy_.processTransition(currentState_, Event(), *this)) {
            // Check if state actually changed (external transition) or just actions (internal transition)
            if (oldState != currentState_) {
                // W3C SCXML 3.13: External transition - exit old, execute actions, enter new
                executeOnExit(oldState, preTransitionStates);
                executeOnEntry(currentState_);
                processEventQueues();
                checkEventlessTransitions();

                // W3C SCXML 6.4: Notify parent if reached final state
                if (isInFinalState() && completionCallback_) {
                    completionCallback_();
                }
            } else {
                // W3C SCXML 3.4: Internal transition - no state change, but execute actions
                LOG_DEBUG("AOT tick: Internal transition in state {}", static_cast<int>(currentState_));
                policy_.executeTransitionActions(*this);
            }
        }

        // Even if no transition taken, process internal queue in case
        // scheduler raised events that should be processed
        processEventQueues();
        checkEventlessTransitions();
    }

    /**
     * @brief Set completion callback for done.invoke event generation (W3C SCXML 6.4)
     *
     * This callback is invoked when the state machine reaches a final state.
     * Used by parent to generate done.invoke.{id} events.
     *
     * @param callback Function to call on completion (nullptr to clear)
     */
    void setCompletionCallback(std::function<void()> callback) {
        completionCallback_ = callback;
    }

    /**
     * @brief Get access to policy for parameter passing (W3C SCXML 6.4)
     *
     * Used by parent state machines to pass invoke parameters to child state machines.
     * Allows setting datamodel variables before calling initialize().
     *
     * @return Reference to policy instance
     */
    StatePolicy &getPolicy() {
        return policy_;
    }
};

}  // namespace RSM::Static
