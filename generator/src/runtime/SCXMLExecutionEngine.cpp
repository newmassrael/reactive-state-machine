#include "runtime/SCXMLExecutionEngine.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "model/DocumentModel.h"
#include "runtime/MicrostepProcessor.h"
#include "runtime/RuntimeContext.h"
#include "runtime/StateConfiguration.h"
#include "runtime/TransitionSelector.h"
#include <chrono>

namespace SCXML {
namespace Runtime {

SCXMLExecutionEngine::SCXMLExecutionEngine()
    : model_(nullptr), context_(nullptr), stateConfig_(nullptr), transitionSelector_(nullptr),
      microstepProcessor_(nullptr), state_(ExecutionState::IDLE) {
    SCXML::Common::Logger::debug("SCXMLExecutionEngine created");
    resetStatistics();
}

SCXMLExecutionEngine::~SCXMLExecutionEngine() {
    if (state_ != ExecutionState::IDLE) {
        SCXML::Common::Logger::warning("SCXMLExecutionEngine destroyed while not idle");
    }
}

// ====== Initialization ======

bool SCXMLExecutionEngine::initialize(std::shared_ptr<Model::DocumentModel> model,
                                      std::shared_ptr<Runtime::RuntimeContext> context) {
    if (!model) {
        SCXML::Common::Logger::error("SCXMLExecutionEngine::initialize - null model provided");
        return false;
    }

    if (!context) {
        SCXML::Common::Logger::error("SCXMLExecutionEngine::initialize - null context provided");
        return false;
    }

    model_ = model;
    context_ = context;

    if (!initializeComponents()) {
        SCXML::Common::Logger::error("SCXMLExecutionEngine::initialize - component initialization failed");
        return false;
    }

    setState(ExecutionState::IDLE);
    SCXML::Common::Logger::info("SCXMLExecutionEngine initialized successfully");
    return true;
}

bool SCXMLExecutionEngine::start() {
    if (state_ != ExecutionState::IDLE) {
        SCXML::Common::Logger::error("SCXMLExecutionEngine::start - engine not in idle state");
        return false;
    }

    setState(ExecutionState::RUNNING);

    if (!enterInitialStates()) {
        SCXML::Common::Logger::error("SCXMLExecutionEngine::start - failed to enter initial states");
        setState(ExecutionState::ERROR);
        return false;
    }

    SCXML::Common::Logger::info("SCXMLExecutionEngine started successfully");
    return true;
}

bool SCXMLExecutionEngine::reset() {
    setState(ExecutionState::IDLE);

    if (stateConfig_) {
        stateConfig_->clear();
    }

    resetStatistics();
    SCXML::Common::Logger::info("SCXMLExecutionEngine reset");
    return true;
}

// ====== Event Processing (SCXML Algorithm) ======

SCXMLExecutionEngine::MacrostepResult SCXMLExecutionEngine::processExternalEvent(Events::EventPtr event) {
    MacrostepResult result;

    if (state_ != ExecutionState::RUNNING) {
        result.errorMessage = "Engine not in running state";
        return result;
    }

    if (!event) {
        result.errorMessage = "Null event provided";
        return result;
    }

    if (!microstepProcessor_ || !stateConfig_ || !context_) {
        result.errorMessage = "Engine components not properly initialized";
        return result;
    }

    try {
        // Execute macrostep with the external event
        auto macrostepResult = microstepProcessor_->executeMacrostep(event, *stateConfig_, *context_);

        // Convert internal result to public result
        result.completed = macrostepResult.success;
        result.microstepsExecuted = macrostepResult.microstepsExecuted;
        result.finalConfiguration = macrostepResult.finalConfiguration;
        result.errorMessage = macrostepResult.errorMessage;

        // Update engine statistics
        stats_.microstepsExecuted += static_cast<uint64_t>(macrostepResult.microstepsExecuted);
        stats_.macrostepsExecuted++;
        stats_.eventsProcessed++;

        // Count state changes
        for (const auto &microstep : macrostepResult.microsteps) {
            stats_.stateEntriesExecued += microstep.enteredStates.size();
            stats_.stateExitsExecuted += microstep.exitedStates.size();
            // Count transitions (simplified - each microstep with transitions counts as 1)
            if (microstep.transitionsTaken) {
                stats_.transitionsTaken++;
            }
        }

        // Check for final state
        if (stateConfig_->isInFinalConfiguration()) {
            setState(ExecutionState::FINAL);
            SCXML::Common::Logger::info("SCXMLExecutionEngine reached final state");
        } else if (!result.completed) {
            setState(ExecutionState::ERROR);
            SCXML::Common::Logger::error("SCXMLExecutionEngine macrostep failed: " + result.errorMessage);
        }

    } catch (const std::exception &e) {
        result.completed = false;
        result.errorMessage = "Exception during event processing: " + std::string(e.what());
        setState(ExecutionState::ERROR);
        SCXML::Common::Logger::error(result.errorMessage);
    }

    return result;
}

SCXMLExecutionEngine::MicrostepResult SCXMLExecutionEngine::executeMicrostep(Events::EventPtr event) {
    MicrostepResult result;

    if (state_ != ExecutionState::RUNNING) {
        result.errorMessage = "Engine not in running state";
        return result;
    }

    if (!microstepProcessor_ || !stateConfig_ || !context_) {
        result.errorMessage = "Engine components not properly initialized";
        return result;
    }

    try {
        // Execute single microstep
        auto microstepResult = microstepProcessor_->executeMicrostep(event, *stateConfig_, *context_);

        // Convert internal result to public result
        result.eventProcessed = (event != nullptr);
        result.transitionsTaken = microstepResult.transitionsTaken;
        result.exitedStates = microstepResult.exitedStates;
        result.enteredStates = microstepResult.enteredStates;
        result.errorMessage = microstepResult.errorMessage;

        // Update statistics
        updateStatistics(result);

        if (!microstepResult.success) {
            setState(ExecutionState::ERROR);
        }

    } catch (const std::exception &e) {
        result.errorMessage = "Exception during microstep execution: " + std::string(e.what());
        setState(ExecutionState::ERROR);
        SCXML::Common::Logger::error(result.errorMessage);
    }

    return result;
}

SCXMLExecutionEngine::MacrostepResult SCXMLExecutionEngine::executeMacrostep(Events::EventPtr initialEvent) {
    MacrostepResult result;

    if (state_ != ExecutionState::RUNNING) {
        result.errorMessage = "Engine not in running state";
        return result;
    }

    if (!microstepProcessor_ || !stateConfig_ || !context_) {
        result.errorMessage = "Engine components not properly initialized";
        return result;
    }

    try {
        // Execute complete macrostep
        auto macrostepResult = microstepProcessor_->executeMacrostep(initialEvent, *stateConfig_, *context_);

        // Convert internal result to public result
        result.completed = macrostepResult.success;
        result.microstepsExecuted = macrostepResult.microstepsExecuted;
        result.finalConfiguration = macrostepResult.finalConfiguration;
        result.errorMessage = macrostepResult.errorMessage;

        // Update engine statistics
        stats_.microstepsExecuted += static_cast<uint64_t>(macrostepResult.microstepsExecuted);
        stats_.macrostepsExecuted++;
        if (initialEvent) {
            stats_.eventsProcessed++;
        }

        // Count state changes
        for (const auto &microstep : macrostepResult.microsteps) {
            stats_.stateEntriesExecued += microstep.enteredStates.size();
            stats_.stateExitsExecuted += microstep.exitedStates.size();
            if (microstep.transitionsTaken) {
                stats_.transitionsTaken++;
            }
        }

        // Check for final state
        if (stateConfig_->isInFinalConfiguration()) {
            setState(ExecutionState::FINAL);
        } else if (!result.completed) {
            setState(ExecutionState::ERROR);
        }

    } catch (const std::exception &e) {
        result.completed = false;
        result.errorMessage = "Exception during macrostep execution: " + std::string(e.what());
        setState(ExecutionState::ERROR);
        SCXML::Common::Logger::error(result.errorMessage);
    }

    return result;
}

// ====== State Queries ======

std::set<std::string> SCXMLExecutionEngine::getCurrentConfiguration() const {
    if (!stateConfig_) {
        return {};
    }

    return stateConfig_->getActiveStates();
}

bool SCXMLExecutionEngine::isStateActive(const std::string &stateId) const {
    if (!stateConfig_) {
        return false;
    }

    return stateConfig_->isActive(stateId);
}

bool SCXMLExecutionEngine::isInFinalState() const {
    if (!stateConfig_) {
        return false;
    }

    return stateConfig_->isInFinalConfiguration();
}

SCXMLExecutionEngine::ExecutionState SCXMLExecutionEngine::getExecutionState() const {
    return state_;
}

// ====== Statistics and Debugging ======

SCXMLExecutionEngine::ExecutionStats SCXMLExecutionEngine::getStatistics() const {
    return stats_;
}

void SCXMLExecutionEngine::resetStatistics() {
    stats_ = ExecutionStats();
    SCXML::Common::Logger::debug("SCXMLExecutionEngine statistics reset");
}

// ====== Private Methods ======

bool SCXMLExecutionEngine::initializeComponents() {
    // Initialize state configuration
    stateConfig_ = std::make_unique<Runtime::StateConfiguration>();
    if (!stateConfig_->initialize(model_)) {
        SCXML::Common::Logger::error("Failed to initialize StateConfiguration");
        return false;
    }

    // Initialize transition selector
    transitionSelector_ = std::make_shared<Runtime::TransitionSelector>();
    if (!transitionSelector_->initialize(model_)) {
        SCXML::Common::Logger::error("Failed to initialize TransitionSelector");
        return false;
    }

    // Initialize microstep processor
    microstepProcessor_ = std::make_unique<Runtime::MicrostepProcessor>();
    if (!microstepProcessor_->initialize(model_, transitionSelector_)) {
        SCXML::Common::Logger::error("Failed to initialize MicrostepProcessor");
        return false;
    }

    SCXML::Common::Logger::debug("SCXMLExecutionEngine components initialized");
    return true;
}

bool SCXMLExecutionEngine::enterInitialStates() {
    if (!model_ || !stateConfig_ || !microstepProcessor_ || !context_) {
        return false;
    }

    // Get initial state from model
    std::string initialState = model_->getInitialState();
    if (initialState.empty()) {
        SCXML::Common::Logger::error("No initial state defined in model");
        return false;
    }

    // Add initial state and its ancestors to configuration
    std::vector<std::string> initialStates = {initialState};

    // Add ancestors if needed (simplified - in full implementation would compute proper entry set)
    stateConfig_->addStates(initialStates);

    // Execute onentry actions for initial states
    for (const auto &stateId : initialStates) {
        if (!microstepProcessor_->executeOnEntryActions(stateId, *context_)) {
            SCXML::Common::Logger::warning("Failed to execute onentry actions for initial state: " + stateId);
        }
        stats_.stateEntriesExecued++;
    }

    // Process any eventless transitions from initial configuration
    auto macrostepResult = microstepProcessor_->executeEventlessMacrostep(*stateConfig_, *context_);
    if (macrostepResult.success) {
        stats_.microstepsExecuted += static_cast<uint64_t>(macrostepResult.microstepsExecuted);
        stats_.macrostepsExecuted++;

        // Count state changes
        for (const auto &microstep : macrostepResult.microsteps) {
            stats_.stateEntriesExecued += microstep.enteredStates.size();
            stats_.stateExitsExecuted += microstep.exitedStates.size();
            if (microstep.transitionsTaken) {
                stats_.transitionsTaken++;
            }
        }
    }

    SCXML::Common::Logger::debug("Entered initial states: " + stateConfig_->toString());
    return true;
}

void SCXMLExecutionEngine::updateStatistics(const MicrostepResult &result) {
    stats_.microstepsExecuted++;
    stats_.stateEntriesExecued += result.enteredStates.size();
    stats_.stateExitsExecuted += result.exitedStates.size();

    if (result.transitionsTaken) {
        stats_.transitionsTaken++;
    }

    if (result.eventProcessed) {
        stats_.eventsProcessed++;
    }
}

void SCXMLExecutionEngine::setState(ExecutionState newState) {
    if (state_ != newState) {
        ExecutionState oldState = state_;
        state_ = newState;

        SCXML::Common::Logger::debug("SCXMLExecutionEngine state changed: " + stateToString(oldState) + " -> " +
                                     stateToString(newState));
    }
}

std::string SCXMLExecutionEngine::stateToString(ExecutionState state) const {
    switch (state) {
    case ExecutionState::IDLE:
        return "IDLE";
    case ExecutionState::RUNNING:
        return "RUNNING";
    case ExecutionState::FINAL:
        return "FINAL";
    case ExecutionState::ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

}  // namespace Runtime
}  // namespace SCXML