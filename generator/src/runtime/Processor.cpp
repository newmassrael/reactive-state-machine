#include "runtime/Processor.h"
#include "common/ErrorHandler.h"
#include "common/GracefulJoin.h"
#include "common/IdGenerator.h"
#include "common/SCXMLCommon.h"
#include "core/DataNode.h"
#include "core/NodeFactory.h"
#include "model/DocumentModel.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "parsing/DocumentParser.h"
#include "runtime/ActionProcessor.h"
#include "runtime/DataModelEngine.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/GuardEvaluator.h"
#include "runtime/InitialStateResolver.h"
#include "runtime/Processor.h"
#include "runtime/RuntimeContext.h"
#include "runtime/TransitionExecutor.h"
// Event system headers - fully integrated
#include "events/EventQueue.h"
#include "runtime/CoreEventProcessor.h"
#include "runtime/EventScheduler.h"
#include "runtime/IOProcessor.h"
// HTTP and SCXML I/O Processors not included - require external dependencies
#include <fstream>

using namespace SCXML;
#include <chrono>
#include <future>
#include <iostream>
#include <sstream>
#include <thread>

// Explicit using declarations to avoid namespace lookup issues
using Events::EventDispatcher;
using Events::EventQueue;

Processor::Processor(const std::string &name)
    : name_(name.empty() ? "scxml_processor" : name), sessionId_(generateSessionId()), state_(State::STOPPED),
      model_(nullptr), context_(nullptr), eventQueue_(nullptr), dispatcher_(nullptr), ioManager_(nullptr),
      stateResolver_(nullptr), transitionExecutor_(nullptr), actionProcessor_(nullptr), expressionEvaluator_(nullptr),
      guardEvaluator_(nullptr), stopRequested_(false), maxEventRate_(0), eventTracing_(false), processedEvents_(0),
      failedEvents_(0), startTime_(std::chrono::steady_clock::now()) {}

Processor::~Processor() {
    // Ensure graceful shutdown
    if (state_ != State::STOPPED) {
        stop(true);  // Wait for completion
    }

    // Single-threaded mode: No event thread to clean up

    cleanup();
}

std::string Processor::generateSessionId() {
    return IdGenerator::generateSessionId("scxml");
}

// ========== Lifecycle Management ==========

bool Processor::initialize(const std::string &scxmlContent, bool isFilePath) {
    // Single-threaded mode: No mutex needed
    if (state_ != State::STOPPED) {
        return false;
    }

    try {
        std::string xmlContent;

        if (isFilePath) {
            std::ifstream file(scxmlContent);
            if (!file.is_open()) {
                return false;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            xmlContent = buffer.str();
        } else {
            xmlContent = scxmlContent;
        }

        auto nodeFactory = std::make_shared<Core::NodeFactory>();

        auto parser = std::make_unique<Parsing::DocumentParser>(nodeFactory, nullptr);
        if (!parser) {
            return false;
        }

        model_ = parser->parseContent(xmlContent);
        if (!model_) {
            return false;
        }

        if (!validateModel(model_)) {
            model_.reset();
            return false;
        }

        return initializeWithModel(model_);
    } catch (const std::exception &e) {
        return false;
    }
}

bool Processor::initialize(std::shared_ptr<Model::DocumentModel> model) {
    // Single-threaded mode: No mutex needed
    if (state_ != State::STOPPED || !model) {
        return false;
    }

    return initializeWithModel(model);
}

bool Processor::initializeWithContext(std::shared_ptr<Model::DocumentModel> model,
                                      std::shared_ptr<SCXML::Runtime::RuntimeContext> context) {
    // Single-threaded mode: No mutex needed
    if (state_ != State::STOPPED || !model || !context) {
        return false;
    }

    try {
        model_ = model;
        context_ = context;
        context_->setSessionName(name_);

        // Initialize DataModel items with the pre-configured context
        for (const auto &dataItem : model_->getDataModelItems()) {
            if (dataItem) {
                auto dataNode = std::dynamic_pointer_cast<Core::DataNode>(dataItem);
                if (dataNode && !dataNode->initialize(*context_)) {
                    std::cerr << "Failed to initialize data model item: " << dataItem->getId() << std::endl;
                    cleanup();
                    return false;
                }
            }
        }

        if (!initializeInternal()) {
            cleanup();
            return false;
        }

        if (!initializeRuntimeComponents()) {
            cleanup();
            return false;
        }

        // LONG-TERM FIX: Configure GuardEvaluator to use ECMAScript-enabled evaluator
        // This ensures guard conditions like "counter == 6" are evaluated using the same
        // JavaScript engine as assign actions, providing consistent behavior
        if (guardEvaluator_ && context_->getDataModelEngine()) {
            // Create an ECMAScript-enabled ExpressionEvaluator that uses the DataModelEngine
            auto ecmaEvaluator = std::make_shared<::SCXML::Runtime::ExpressionEvaluator>();
            guardEvaluator_->setDataModelEngine(context_->getDataModelEngine());
            guardEvaluator_->setExpressionEvaluator(ecmaEvaluator);
        }

        // Configure TransitionExecutor's GuardEvaluator with the same DataModelEngine
        if (transitionExecutor_ && guardEvaluator_) {
            transitionExecutor_->setGuardEvaluator(*guardEvaluator_);
        }

        context_->setEventQueue(eventQueue_);
        context_->setEventDispatcher(dispatcher_);
        context_->setIOProcessorManager(ioManager_);

        resetStatistics();

        return true;
    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("Processor::initializeWithContext - Exception: " + std::string(e.what()));
        cleanup();
        return false;
    }
}

bool Processor::initializeWithModel(std::shared_ptr<Model::DocumentModel> model) {
    try {
        model_ = model;

        context_ = std::make_shared<::SCXML::Runtime::RuntimeContext>();
        context_->setModel(model_);
        context_->setSessionName(name_);

        // Initialize DataModel items
        for (const auto &dataItem : model_->getDataModelItems()) {
            if (dataItem) {
                // Cast to DataNode and initialize
                auto dataNode = std::dynamic_pointer_cast<Core::DataNode>(dataItem);
                if (dataNode && !dataNode->initialize(*context_)) {
                    std::cerr << "Failed to initialize data model item: " << dataItem->getId() << std::endl;
                    cleanup();
                    return false;
                }
            }
        }

        if (!initializeInternal()) {
            cleanup();
            return false;
        }

        if (!initializeRuntimeComponents()) {
            cleanup();
            return false;
        }

        context_->setEventQueue(eventQueue_);
        context_->setEventDispatcher(dispatcher_);
        context_->setIOProcessorManager(ioManager_);

        resetStatistics();

        return true;
    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("Processor::initializeWithModel - Exception: " + std::string(e.what()));
        cleanup();
        return false;
    }
}

bool Processor::start() {
    // Single-threaded mode: No mutex needed
    if (state_ != State::STOPPED || !model_ || !context_) {
        return false;
    }

    try {
        setState(State::STARTING);
        stopRequested_ = false;

        if (!initializeStateConfiguration()) {
            cleanup();
            return false;
        }

        // Single-threaded mode: No event thread needed

        setState(State::RUNNING);
        startTime_ = std::chrono::steady_clock::now();

        // Single-threaded mode: No condition variable needed

        return true;
    } catch (const std::exception &e) {
        setState(State::STOPPED);
        return false;
    }
}

bool Processor::stop(bool waitForCompletion) {
    (void)waitForCompletion;  // Suppress unused parameter warning
    {
        // Single-threaded mode: No mutex needed
        if (state_ == State::STOPPED) {
            return true;
        }

        setState(State::STOPPING);
        stopRequested_ = true;
    }

    // Gracefully shutdown all components

    // 1. Stop IOManager first to prevent new I/O operations
    if (ioManager_) {
        try {
            // IOManager shutdown - prevent new HTTP requests, etc.
            ioManager_.reset();
        } catch (const std::exception &e) {
            // Log error but continue shutdown
        }
    }

    // 2. Shutdown event queue to unblock dequeue()
    if (eventQueue_) {
        eventQueue_->shutdown();
    }

    // 3. Stop event dispatcher
    if (dispatcher_) {
        try {
            dispatcher_.reset();
        } catch (const std::exception &e) {
            // Log error but continue shutdown
        }
    }

    // 4. Notify all waiting threads
    // Single-threaded mode: No condition variable needed

    // Single-threaded mode: No event thread to join

    {
        // Single-threaded mode: No mutex needed
        setState(State::STOPPED);
        // Single-threaded mode: No event thread to reset
    }

    // Single-threaded mode: No condition variable needed
    return true;
}

bool Processor::pause() {
    // Single-threaded mode: No mutex needed
    if (state_ != State::RUNNING) {
        return false;
    }

    setState(State::PAUSED);
    return true;
}

bool Processor::resume() {
    // Single-threaded mode: No mutex needed
    if (state_ != State::PAUSED) {
        return false;
    }

    setState(State::RUNNING);
    // Single-threaded mode: No condition variable needed
    return true;
}

bool Processor::reset() {
    if (!stop()) {
        return false;
    }

    cleanup();
    resetStatistics();

    return true;
}

// ========== Event Processing ==========

bool Processor::sendEvent(const std::string &eventName, const std::string &data) {
    if (!context_) {
        return false;
    }

    auto event = context_->createEvent(eventName, data, "external");

    if (!event) {
        return false;
    }

    bool result = sendEvent(event);

    return result;
}

bool Processor::sendEvent(Events::EventPtr event) {
    if (!event || !eventQueue_) {
        return false;
    }

    try {
        if (eventQueue_) {
            auto eventWrapper = std::make_shared<Events::Event>(event->getName(), event->getData(), event->getType());

            eventQueue_->enqueue(eventWrapper);

            if (eventTracing_) {
                context_->log("debug", "Enqueued event: " + event->getName());
            }

            // Single-threaded mode: Process events immediately after enqueueing
            if (state_ == State::RUNNING) {
                processAllEvents();
            }

            return true;
        }

        return false;
    } catch (const std::exception &e) {
        if (eventTracing_) {
            context_->log("error", "Failed to enqueue event: " + std::string(e.what()));
        }

        return false;
    }
}

bool Processor::processNextEvent() {
    if (!eventQueue_) {
        return false;
    }

    try {
        auto event = eventQueue_->dequeue();
        if (!event) {
            return false;
        }

        processEvent(event);
        bool success = true;  // Assume success for now
        updateStatistics(success);

        return success;
    } catch (const std::exception &e) {
        if (eventTracing_) {
            context_->log("error", "Event processing failed: " + std::string(e.what()));
        }
        updateStatistics(false);
        return false;
    }
}

void Processor::processAllEvents() {
    // Process all pending events synchronously until queue is empty
    // Allow processing during initialization (STARTING state) and normal running
    while (!stopRequested_ && (state_ == State::RUNNING || state_ == State::STARTING) &&
           (!isEventQueueEmpty() || context_->getEventScheduler().hasReadyEvents())) {
        // Process scheduled events that are ready for delivery
        if (context_) {
            context_->processScheduledEvents();
        }

        // Process regular events
        if (!isEventQueueEmpty()) {
            processNextEvent();
        }
    }

    // SCXML-compliant: After processing all events, process eventless transitions
    // Keep processing eventless transitions until no more are enabled
    if (!stopRequested_ && (state_ == State::RUNNING || state_ == State::STARTING) && transitionExecutor_ && model_ &&
        context_) {
        bool eventlessTransitionTaken = true;
        int maxIterations = 100;  // Prevent infinite loops
        int iterations = 0;

        while (eventlessTransitionTaken && iterations < maxIterations && !stopRequested_) {
            auto result = transitionExecutor_->executeEventlessTransitions(model_, *context_);
            eventlessTransitionTaken = result.transitionTaken;
            iterations++;

            // Check if we've reached a final state and should stop
            if (isInFinalState() || context_->isFinalStateReached()) {
                stopRequested_ = true;
                break;
            }
        }

        if (iterations >= maxIterations) {
            SCXML::Common::Logger::warning(
                "Processor::processAllEvents - Maximum eventless transition iterations reached");
        }
    }
}

bool Processor::isEventQueueEmpty() const {
    return !eventQueue_ || eventQueue_->size() == 0;
}

bool Processor::waitForStable(int timeoutMs) const {
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() - startTime < timeout) {
        // Check for scheduled events that might be ready or pending
        bool hasScheduledEvents = false;
        if (context_) {
            hasScheduledEvents = context_->getEventScheduler().getScheduledEventCount() > 0;
        }

        // Wait for events to be processed by the background thread
        if (isEventQueueEmpty() && !hasScheduledEvents && !isRunning()) {
            // Processor stopped and no events pending - stable
            return true;
        }

        // Check if we've reached a final state - this is also a stable condition
        if (isEventQueueEmpty() && !hasScheduledEvents && isInFinalState()) {
            return true;  // Final state reached and no events pending - stable
        }

        if (isEventQueueEmpty() && !hasScheduledEvents) {
            // Additional wait to ensure stability
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            bool stillHasScheduledEvents = false;
            if (context_) {
                stillHasScheduledEvents = context_->getEventScheduler().getScheduledEventCount() > 0;
            }
            if (isEventQueueEmpty() && !stillHasScheduledEvents) {
                return true;  // Stable state reached
            }
        }

        // Process any ready scheduled events
        if (context_) {
            context_->processScheduledEvents();
        }

        // Let the background thread process events
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return false;  // Timeout reached
}

// ========== State Inspection ==========

bool Processor::isStateActive(const std::string &stateId) const {
    if (!context_) {
        return false;
    }

    return context_->isStateActive(stateId);
}

std::vector<std::string> Processor::getActiveStates() const {
    if (!context_) {
        return {};
    }

    return context_->getActiveStates();
}

std::set<std::string> Processor::getConfiguration() const {
    if (!context_) {
        return {};
    }

    std::set<std::string> config;
    auto activeStates = context_->getActiveStates();
    config.insert(activeStates.begin(), activeStates.end());
    return config;
}

// ========== Data Model Access ==========

std::string Processor::getDataValue(const std::string &name) const {
    if (!context_) {
        return "";
    }

    return context_->getDataValue(name);
}

bool Processor::setDataValue(const std::string &name, const std::string &value) {
    if (context_) {
        context_->setDataValue(name, value);
        return true;
    }
    return false;
}

std::shared_ptr<Runtime::RuntimeContext> Processor::getContext() const {
    return context_;
}

Processor::State Processor::getCurrentState() const {
    return state_.load();
}

// ========== Information ==========

std::string Processor::getSessionId() const {
    return sessionId_;
}

std::string Processor::getName() const {
    return name_;
}

bool Processor::isRunning() const {
    // Single-threaded mode: No mutex needed
    return state_ == State::RUNNING && !stopRequested_;
}

bool Processor::isInFinalState() const {
    if (!context_ || !model_) {
        return false;
    }

    auto activeStates = context_->getActiveStates();
    for (const auto &stateId : activeStates) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode && stateNode->isFinalState()) {
            if (eventTracing_) {
                context_->log("debug", "isInFinalState: found final state " + stateId);
            }
            return true;
        }
    }

    return false;
}

std::map<std::string, uint64_t> Processor::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_);

    std::map<std::string, uint64_t> stats;
    stats["uptime_ms"] = static_cast<uint64_t>(duration.count());
    stats["processed_events"] = processedEvents_;
    stats["failed_events"] = failedEvents_;
    stats["pending_events"] = eventQueue_ ? eventQueue_->size() : 0;

    return stats;
}

// ========== Private Methods ==========

bool Processor::initializeInternal() {
    try {
        // Use the same event queue as the RuntimeContext's EventManager
        // This ensures that raised events from actions are processed by the processor
        if (context_) {
            eventQueue_ = context_->getEventManager().getInternalQueue();
        }

        // Fallback to creating new queue if context is not available
        if (!eventQueue_) {
            eventQueue_ = std::shared_ptr<Events::EventQueue>(new Events::EventQueue());
        }

        if (!eventQueue_) {
            throw std::runtime_error("Failed to create event queue");
        }

        dispatcher_ = std::shared_ptr<Events::EventDispatcher>(new Events::EventDispatcher());
        if (!dispatcher_) {
            throw std::runtime_error("Failed to create event dispatcher");
        }

        // I/O processor manager initialization deferred - not required for basic operation

        return true;
    } catch (const std::exception &e) {
        return false;
    }
}

void Processor::cleanup() {
    // Clean up thread first
    // Single-threaded mode: No event thread to clean up

    eventQueue_.reset();
    dispatcher_.reset();
    ioManager_.reset();
    context_.reset();

    stateResolver_.reset();
    transitionExecutor_.reset();
    actionProcessor_.reset();
    expressionEvaluator_.reset();
    guardEvaluator_.reset();
}

void Processor::setState(State newState) {
    state_ = newState;
}

// Single-threaded mode: runEventLoop removed - using processAllEvents() instead

void Processor::processEvent(std::shared_ptr<Events::Event> event) {
    if (!event || !context_ || !transitionExecutor_) {
        updateStatistics(false);
        return;
    }

    try {
        std::string eventName = event->getName().empty() ? "unnamed_event" : event->getName();

        if (eventTracing_) {
            context_->log("debug", "Processing event: " + eventName);
        }

        auto result = transitionExecutor_->executeTransitions(model_, event, *context_);

        if (result.transitionTaken) {
            if (eventTracing_) {
                context_->log("info", "Event '" + eventName + "' caused transition: " +
                                          std::to_string(result.exitedStates.size()) + " states exited, " +
                                          std::to_string(result.enteredStates.size()) + " states entered");
            }
        } else {
            if (eventTracing_) {
                context_->log("debug", "Event '" + eventName + "' did not cause any transitions");
            }
        }

        updateStatistics(true);

        // Check if we've reached a final state and should stop
        if (isInFinalState() || context_->isFinalStateReached()) {
            if (eventTracing_) {
                context_->log("info", "Final state reached, stopping processor");
            }
            stopRequested_ = true;
            // Don't change state here - let the event loop exit naturally
            // Single-threaded mode: No condition variable needed
        }

    } catch (const std::exception &e) {
        if (eventTracing_) {
            context_->log("error", "Event processing failed: " + std::string(e.what()));
        }
        updateStatistics(false);
    }
}

void Processor::executeTransitions(const std::vector<Model::ITransitionNode *> &enabledTransitions) {
    if (enabledTransitions.empty() || !model_ || !context_ || !transitionExecutor_) {
        return;
    }

    try {
        // Execute enabled transitions
        for (auto *transition : enabledTransitions) {
            if (transition && transitionExecutor_) {
                // transitionExecutor_->executeTransition(transition, *context_);
            }
        }

        // State transition handling requires integration with TransitionExecutor

    } catch (const std::exception &e) {
        if (eventTracing_) {
            context_->log("error", "Transition execution failed: " + std::string(e.what()));
        }
    }
}

void Processor::enterStates(const std::set<Model::IStateNode *> &statesToEnter) {
    for (const auto *state : statesToEnter) {
        if (state && model_ && context_ && actionProcessor_) {
            try {
                if (eventTracing_) {
                    context_->log("info", "Entering state: " + state->getId());
                }

                auto result = actionProcessor_->executeEntryActions(model_, state->getId(), nullptr, *context_);
                if (!result.success && eventTracing_) {
                    context_->log("error",
                                  "Entry actions failed for state " + state->getId() + ": " + result.errorMessage);
                }

                context_->activateState(state->getId());

            } catch (const std::exception &e) {
                if (eventTracing_) {
                    context_->log("error", "State entry execution failed for " + state->getId() + ": " + e.what());
                }
            }
        }
    }

    // Check if we've reached a final state and should stop
    if (isInFinalState()) {
        if (eventTracing_) {
            context_->log("info", "Final state reached in enterStates, stopping processor");
        }
        stopRequested_ = true;
        // Also update the processor state properly with mutex protection
        {
            // Single-threaded mode: No mutex needed
            state_ = State::STOPPED;
        }
        // Single-threaded mode: No condition variable needed
    }
}

void Processor::exitStates(const std::set<Model::IStateNode *> &statesToExit) {
    for (const auto *state : statesToExit) {
        if (state && model_ && context_ && actionProcessor_) {
            try {
                if (eventTracing_) {
                    context_->log("info", "Exiting state: " + state->getId());
                }

                auto result = actionProcessor_->executeExitActions(model_, state->getId(), nullptr, *context_);
                if (!result.success && eventTracing_) {
                    context_->log("error",
                                  "Exit actions failed for state " + state->getId() + ": " + result.errorMessage);
                }

                context_->deactivateState(state->getId());

            } catch (const std::exception &e) {
                if (eventTracing_) {
                    context_->log("error", "State exit execution failed for " + state->getId() + ": " + e.what());
                }
            }
        }
    }
}

bool Processor::validateModel(std::shared_ptr<Model::DocumentModel> model) {
    if (!model) {
        if (eventTracing_) {
            Common::ErrorHandler::getInstance().logLegacyError("ERROR", "SCXML model is null",
                                                               "Processor::validateModel");
        }
        return false;
    }

    try {
        if (model->getName().empty() && eventTracing_) {
            SCXML::Common::Logger::warning("SCXML model has no name");
        }

        auto allStates = model->getAllStates();
        if (allStates.empty()) {
            if (eventTracing_) {
                SCXML::Common::Logger::error("SCXML model has no states");
            }
            return false;
        }

        auto initialState = model->getInitialState();
        if (initialState.empty()) {
            if (eventTracing_) {
                SCXML::Common::Logger::error("SCXML model has no initial state");
            }
            return false;
        }

        bool initialStateFound = false;
        for (const auto &state : allStates) {
            if (state && state->getId() == initialState) {
                initialStateFound = true;
                break;
            }
        }

        if (!initialStateFound) {
            if (eventTracing_) {
                SCXML::Common::Logger::error("Initial state '" + initialState + "' not found in state list");
            }
            return false;
        }

        if (eventTracing_) {
            SCXML::Common::Logger::info("SCXML model validation successful: " + std::to_string(allStates.size()) +
                                        " states, " + "initial state: " + initialState);
        }

        return true;

    } catch (const std::exception &e) {
        if (eventTracing_) {
            SCXML::Common::Logger::error("Model validation failed with exception: " + std::string(e.what()));
        }
        return false;
    }
}

void Processor::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    processedEvents_ = 0;
    failedEvents_ = 0;
    startTime_ = std::chrono::steady_clock::now();
}

void Processor::updateStatistics(bool eventSuccess) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    if (eventSuccess) {
        ++processedEvents_;
    } else {
        ++failedEvents_;
    }
}

std::string Processor::stateToString(State state) const {
    switch (state) {
    case State::STOPPED:
        return "STOPPED";
    case State::STARTING:
        return "STARTING";
    case State::RUNNING:
        return "RUNNING";
    case State::PAUSED:
        return "PAUSED";
    case State::STOPPING:
        return "STOPPING";
    case State::ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

bool Processor::initializeRuntimeComponents() {
    try {
        stateResolver_ = std::make_unique<Core::InitialStateResolver>();
        if (!stateResolver_) {
            throw std::runtime_error("Failed to create initial state resolver");
        }

        transitionExecutor_ = std::make_unique<TransitionExecutor>();
        if (!transitionExecutor_) {
            throw std::runtime_error("Failed to create transition executor");
        }

        actionProcessor_ = std::make_unique<::SCXML::Runtime::ActionProcessor>();
        if (!actionProcessor_) {
            throw std::runtime_error("Failed to create state action processor");
        }

        expressionEvaluator_ = std::make_shared<::SCXML::Runtime::ExpressionEvaluator>();
        if (!expressionEvaluator_) {
            throw std::runtime_error("Failed to create expression evaluator");
        }

        guardEvaluator_ = std::make_unique<GuardEvaluator>();
        if (!guardEvaluator_) {
            throw std::runtime_error("Failed to create guard evaluator");
        }

        return true;

    } catch (const std::exception &e) {
        if (eventTracing_) {
            Common::ErrorHandler::getInstance().logLegacyError(
                "ERROR", "Runtime component initialization failed: " + std::string(e.what()),
                "Processor::initializeRuntimeComponents");
        }
        return false;
    }
}

bool Processor::initializeStateConfiguration() {
    if (!model_ || !context_ || !stateResolver_) {
        return false;
    }

    // JavaScript 상태 체크 1: 함수 시작 직후
    if (dataModelEngine_ && dataModelEngine_->getECMAScriptEngine()) {
        auto check1 = dataModelEngine_->evaluateExpression("10+10", *context_);
    }

    try {
        auto initialConfig = stateResolver_->resolveInitialStates(model_, *context_);

        // JavaScript 상태 체크 2: resolveInitialStates 후
        if (dataModelEngine_ && dataModelEngine_->getECMAScriptEngine()) {
            auto check2 = dataModelEngine_->evaluateExpression("20+20", *context_);
        }

        if (!initialConfig.success || initialConfig.activeStates.empty()) {
            if (eventTracing_) {
                context_->log("error", "No initial states found in SCXML model: " + initialConfig.errorMessage);
            }
            return false;
        }

        // Note: Only use entryOrder loop below - activeStates contains the same states
        // and would cause duplicate entry if processed here as well

        // JavaScript 상태 체크 3: activeStates 활성화 후
        if (dataModelEngine_ && dataModelEngine_->getECMAScriptEngine()) {
            auto check3 = dataModelEngine_->evaluateExpression("30+30", *context_);
        }

        if (eventTracing_) {
            context_->log("info", "entryOrder size: " + std::to_string(initialConfig.entryOrder.size()));
            for (const auto &stateId : initialConfig.entryOrder) {
                context_->log("info", "entryOrder state: " + stateId);
            }
        }

        for (const auto &stateId : initialConfig.entryOrder) {
            // Find the state node and call proper enterStates method
            if (model_ && context_) {
                auto stateNode = model_->findStateById(stateId);
                if (stateNode) {
                    if (eventTracing_) {
                        context_->log("info", "About to call enterStates for: " + stateId);
                    }
                    std::set<Model::IStateNode *> statesToEnter;
                    statesToEnter.insert(stateNode);
                    enterStates(statesToEnter);
                } else {
                    // Fallback to just activating the state if we can't find the node
                    context_->activateState(stateId);
                }
            }

            // JavaScript 상태 체크 4: 각 entryOrder state 활성화 후
            if (dataModelEngine_ && dataModelEngine_->getECMAScriptEngine()) {
                auto check4 = dataModelEngine_->evaluateExpression("40+40", *context_);

                if (!check4.success) {
                    break;  // 손상 발생 지점에서 중단
                }
            }
        }

        // JavaScript 상태 체크 5: 함수 완료 직전 최종 체크
        if (dataModelEngine_ && dataModelEngine_->getECMAScriptEngine()) {
            auto check5 = dataModelEngine_->evaluateExpression("50+50", *context_);
        }

        if (eventTracing_) {
            std::string stateList;
            for (size_t i = 0; i < initialConfig.activeStates.size(); ++i) {
                if (i > 0) {
                    stateList += ", ";
                }
                stateList += initialConfig.activeStates[i];
            }
            context_->log("info", "Initialized with states: " + stateList);
        }

        // SCXML-compliant event processing: After state initialization,
        // process all events that were raised during entry actions
        context_->log("info", "Processing events raised during initialization");
        processAllEvents();

        return true;

    } catch (const std::exception &e) {
        if (eventTracing_) {
            context_->log("error", "State configuration initialization failed: " + std::string(e.what()));
        }
        return false;
    }
}
