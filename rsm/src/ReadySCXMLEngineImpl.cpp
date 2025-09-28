#include "ReadySCXMLEngine.h"
#include "SCXMLEngine.h"
#include "common/Logger.h"
#include <filesystem>
#include <fstream>

namespace RSM {

/**
 * @brief Internal implementation of ReadySCXMLEngine
 *
 * Now uses SCXMLEngine's high-level API for a unified architecture:
 * - Automatic session management via SCXMLEngine
 * - Simplified initialization
 * - Direct delegation to SCXMLEngine high-level methods
 * - Consistent error handling across the stack
 */
class ReadySCXMLEngineImpl : public ReadySCXMLEngine {
private:
    std::unique_ptr<SCXMLEngine> scxmlEngine_;
    std::string sessionId_;
    std::string lastError_;
    bool initialized_ = false;

public:
    ReadySCXMLEngineImpl() {
        // Create SCXMLEngine instance
        scxmlEngine_ = createSCXMLEngine();
        sessionId_ = "ready_default";
    }

    ~ReadySCXMLEngineImpl() {
        if (initialized_ && scxmlEngine_) {
            scxmlEngine_->stopStateMachine(sessionId_);
        }
    }

    bool initialize(const std::string &scxmlContent) {
        try {
            if (!scxmlEngine_) {
                lastError_ = "SCXMLEngine is null";
                LOG_ERROR("ReadySCXMLEngine: {}", lastError_);
                return false;
            }

            // Initialize the engine
            if (!scxmlEngine_->initialize()) {
                lastError_ = "Failed to initialize SCXMLEngine";
                LOG_ERROR("ReadySCXMLEngine: {}", lastError_);
                return false;
            }

            // Load SCXML content using the high-level API
            if (!scxmlEngine_->loadSCXMLFromString(scxmlContent, sessionId_)) {
                lastError_ = "Failed to load SCXML content: " + scxmlEngine_->getLastStateMachineError(sessionId_);
                LOG_ERROR("ReadySCXMLEngine: {}", lastError_);
                return false;
            }

            initialized_ = true;
            LOG_INFO("ReadySCXMLEngine: Initialized successfully with session: {}", sessionId_);
            return true;

        } catch (const std::exception &e) {
            lastError_ = std::string("Initialization failed: ") + e.what();
            LOG_ERROR("ReadySCXMLEngine: {}", lastError_);
            return false;
        }
    }

    // === ReadySCXMLEngine Interface Implementation ===

    bool start() override {
        if (!initialized_) {
            lastError_ = "Engine not initialized";
            return false;
        }

        try {
            bool result = scxmlEngine_->startStateMachine(sessionId_);
            if (!result) {
                lastError_ = scxmlEngine_->getLastStateMachineError(sessionId_);
                LOG_ERROR("ReadySCXMLEngine: {}", lastError_);
            }
            return result;
        } catch (const std::exception &e) {
            lastError_ = std::string("Start failed: ") + e.what();
            LOG_ERROR("ReadySCXMLEngine: {}", lastError_);
            return false;
        }
    }

    void stop() override {
        if (initialized_ && scxmlEngine_) {
            try {
                scxmlEngine_->stopStateMachine(sessionId_);
            } catch (const std::exception &e) {
                LOG_WARN("ReadySCXMLEngine: Exception during stop: {}", e.what());
            }
        }
    }

    bool sendEvent(const std::string &eventName, const std::string &eventData) override {
        if (!initialized_) {
            lastError_ = "Engine not initialized";
            return false;
        }

        if (!scxmlEngine_->isStateMachineRunning(sessionId_)) {
            lastError_ = "State machine is not running";
            return false;
        }

        try {
            bool result = scxmlEngine_->sendEventSync(eventName, sessionId_, eventData);
            if (!result) {
                lastError_ = scxmlEngine_->getLastStateMachineError(sessionId_);
                LOG_WARN("ReadySCXMLEngine: Event '{}' failed: {}", eventName, lastError_);
            }
            return result;
        } catch (const std::exception &e) {
            lastError_ = std::string("Event processing exception: ") + e.what();
            LOG_ERROR("ReadySCXMLEngine: Event '{}' exception: {}", eventName, e.what());
            return false;
        }
    }

    bool isRunning() const override {
        return initialized_ && scxmlEngine_ && scxmlEngine_->isStateMachineRunning(sessionId_);
    }

    std::string getCurrentState() const override {
        if (!initialized_ || !scxmlEngine_) {
            return "";
        }
        return scxmlEngine_->getCurrentStateSync(sessionId_);
    }

    bool isInState(const std::string &stateId) const override {
        if (!initialized_ || !scxmlEngine_) {
            return false;
        }
        return scxmlEngine_->isInStateSync(stateId, sessionId_);
    }

    std::vector<std::string> getActiveStates() const override {
        if (!initialized_ || !scxmlEngine_) {
            return {};
        }
        return scxmlEngine_->getActiveStatesSync(sessionId_);
    }

    bool setVariable(const std::string &name, const std::string &value) override {
        if (!initialized_ || !scxmlEngine_) {
            lastError_ = "Engine not initialized";
            return false;
        }

        try {
            bool result = scxmlEngine_->setVariableSync(name, value, sessionId_);
            if (!result) {
                lastError_ = scxmlEngine_->getLastStateMachineError(sessionId_);
                LOG_WARN("ReadySCXMLEngine: Failed to set variable '{}': {}", name, lastError_);
            }
            return result;
        } catch (const std::exception &e) {
            lastError_ = std::string("Variable setting exception: ") + e.what();
            LOG_ERROR("ReadySCXMLEngine: Variable '{}' exception: {}", name, e.what());
            return false;
        }
    }

    std::string getVariable(const std::string &name) const override {
        if (!initialized_ || !scxmlEngine_) {
            return "";
        }

        try {
            return scxmlEngine_->getVariableSync(name, sessionId_);
        } catch (const std::exception &e) {
            LOG_WARN("ReadySCXMLEngine: Failed to get variable '{}': {}", name, e.what());
            return "";
        }
    }

    std::string getLastError() const override {
        return lastError_;
    }

    Statistics getStatistics() const override {
        Statistics stats;

        if (initialized_ && scxmlEngine_) {
            // Note: SCXMLEngine doesn't expose statistics directly
            // We can only get basic state information
            stats.currentState = scxmlEngine_->getCurrentStateSync(sessionId_);
            stats.isRunning = scxmlEngine_->isStateMachineRunning(sessionId_);
            // totalEvents and totalTransitions are not available from high-level API
            stats.totalEvents = 0;
            stats.totalTransitions = 0;
        }

        return stats;
    }
};

// === Factory Method Implementations ===

std::unique_ptr<ReadySCXMLEngine> ReadySCXMLEngine::fromFile(const std::string &scxmlFile) {
    // Check if file exists
    if (!std::filesystem::exists(scxmlFile)) {
        LOG_ERROR("ReadySCXMLEngine: SCXML file not found: {}", scxmlFile);
        return nullptr;
    }

    // Read file content
    std::ifstream file(scxmlFile);
    if (!file.is_open()) {
        LOG_ERROR("ReadySCXMLEngine: Cannot open SCXML file: {}", scxmlFile);
        return nullptr;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    return fromString(content);
}

std::unique_ptr<ReadySCXMLEngine> ReadySCXMLEngine::fromString(const std::string &scxmlContent) {
    if (scxmlContent.empty()) {
        LOG_ERROR("ReadySCXMLEngine: Empty SCXML content");
        return nullptr;
    }

    auto engine = std::make_unique<ReadySCXMLEngineImpl>();

    if (!engine->initialize(scxmlContent)) {
        LOG_ERROR("ReadySCXMLEngine: Failed to initialize with SCXML content");
        return nullptr;
    }

    return engine;
}

}  // namespace RSM