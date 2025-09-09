#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

// Forward declarations
namespace SCXML {

namespace Model {
class DocumentModel;
class ITransitionNode;
class IStateNode;
}  // namespace Model

// Forward declarations
namespace Core {
class InitialStateResolver;
class DocumentModel;
}  // namespace Core

namespace Core {
// Forward declaration moved to Model namespace
// Forward declaration moved to Model namespace
// Forward declaration moved to Model namespace
}  // namespace Core

namespace Runtime {
class RuntimeContext;
class ActionProcessor;
class ExpressionEvaluator;
}  // namespace Runtime

namespace Events {
class Event;
class EventQueue;
class EventDispatcher;
class IOProcessorManager;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

class GuardEvaluator;
class DataModelEngine;
class TransitionExecutor;

/**
 * @brief SCXML state machine processor
 *
 * Main execution engine for SCXML state machines, following W3C SCXML standard.
 * This class provides the core interpreter implementation for state machine execution.
 */
class Processor {
public:
    /**
     * @brief Processor execution states
     */
    enum class State {
        STOPPED,   // Processor is stopped
        STARTING,  // Processor is starting up
        RUNNING,   // Processor is executing
        PAUSED,    // Processor is paused
        STOPPING,  // Processor is shutting down
        ERROR      // Processor encountered an error
    };

    /**
     * @brief Constructor
     */
    explicit Processor(const std::string &name = "");

    /**
     * @brief Destructor - ensures clean shutdown
     */
    virtual ~Processor();

    // ====== State Management ======

    /**
     * @brief Initialize the processor with SCXML document
     */
    bool initialize(const std::string &scxmlContent, bool isFilePath = false);

    /**
     * @brief Initialize the processor with pre-parsed model
     */
    bool initialize(std::shared_ptr<::SCXML::Model::DocumentModel> model);

    /**
     * @brief Initialize processor with pre-configured RuntimeContext
     */
    bool initializeWithContext(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                               std::shared_ptr<SCXML::Runtime::RuntimeContext> context);

    /**
     * @brief Start the state machine execution
     */
    bool start();

    /**
     * @brief Stop the state machine execution
     */
    bool stop(bool waitForCompletion = true);

    /**
     * @brief Pause execution (can be resumed)
     */
    bool pause();

    /**
     * @brief Resume from paused state
     */
    bool resume();

    /**
     * @brief Reset to initial state
     */
    bool reset();

    // ====== Event Processing ======

    /**
     * @brief Send an event to the state machine
     */
    bool sendEvent(const std::string &eventName, const std::string &data = "");

    /**
     * @brief Send an event object
     */
    bool sendEvent(std::shared_ptr<SCXML::Events::Event> event);

    /**
     * @brief Check if event queue is empty
     */
    bool isEventQueueEmpty() const;

    /**
     * @brief Wait for processor to reach stable state (no pending events)
     * @param timeoutMs Maximum time to wait in milliseconds
     * @return true if stable state reached, false if timeout
     */
    bool waitForStable(int timeoutMs = 5000) const;

    // ====== State Queries ======

    /**
     * @brief Check if a state is currently active
     */
    bool isStateActive(const std::string &stateId) const;

    /**
     * @brief Get all currently active states
     */
    std::vector<std::string> getActiveStates() const;

    /**
     * @brief Get the current configuration
     */
    std::set<std::string> getConfiguration() const;

    // ====== Data Model Access ======

    /**
     * @brief Get data value by location expression
     */
    std::string getDataValue(const std::string &location) const;

    /**
     * @brief Set data value by location expression
     */
    bool setDataValue(const std::string &location, const std::string &value);

    /**
     * @brief Get runtime context for advanced integration
     * @return Shared pointer to runtime context, or nullptr if not initialized
     */
    std::shared_ptr<Runtime::RuntimeContext> getContext() const;

    // ====== Status and Monitoring ======

    /**
     * @brief Get current processor state
     */
    State getCurrentState() const;

    /**
     * @brief Get session ID
     */
    std::string getSessionId() const;

    /**
     * @brief Get processor name
     */
    std::string getName() const;

    /**
     * @brief Check if processor is running
     */
    bool isRunning() const;

    /**
     * @brief Check if in final state
     */
    bool isInFinalState() const;

    /**
     * @brief Get execution statistics
     */
    std::map<std::string, uint64_t> getStatistics() const;

private:
    // Runtime identification
    std::string name_;
    std::string sessionId_;

    // State management
    std::atomic<State> state_;
    // Single-threaded mode: No synchronization primitives needed

    // Core components
    std::shared_ptr<::SCXML::Model::DocumentModel> model_;
    std::shared_ptr<SCXML::Runtime::RuntimeContext> context_;
    std::shared_ptr<SCXML::Events::EventQueue> eventQueue_;
    std::shared_ptr<SCXML::Events::EventDispatcher> dispatcher_;
    std::shared_ptr<SCXML::Events::IOProcessorManager> ioManager_;

    // Runtime execution components
    std::unique_ptr<SCXML::Core::InitialStateResolver> stateResolver_;
    std::unique_ptr<TransitionExecutor> transitionExecutor_;
    std::unique_ptr<SCXML::Runtime::ActionProcessor> actionProcessor_;
    std::shared_ptr<SCXML::Runtime::ExpressionEvaluator> expressionEvaluator_;
    std::unique_ptr<SCXML::GuardEvaluator> guardEvaluator_;
    std::unique_ptr<SCXML::DataModelEngine> dataModelEngine_;

    // Single-threaded mode: No event thread needed
    std::atomic<bool> stopRequested_;

    // Configuration
    uint32_t maxEventRate_;
    bool eventTracing_;

    // Statistics
    mutable std::mutex statsMutex_;
    uint64_t processedEvents_;
    uint64_t failedEvents_;
    std::chrono::steady_clock::time_point startTime_;

    // Internal methods
    bool initializeWithModel(std::shared_ptr<::SCXML::Model::DocumentModel> model);  // Internal without mutex
    bool initializeInternal();
    bool initializeRuntimeComponents();
    bool initializeStateConfiguration();
    void cleanup();
    void setState(State newState);
    // Single-threaded mode: runEventLoop removed
    bool processNextEvent();  // Moved to private - only called internally

public:
    /**
     * @brief Process all pending events synchronously
     * @note This method is public to allow test frameworks to process scheduled events
     */
    void processAllEvents();  // Process all pending events synchronously

private:
    void processEvent(std::shared_ptr<SCXML::Events::Event> event);
    void executeTransitions(const std::vector<SCXML::Model::ITransitionNode *> &enabledTransitions);
    void enterStates(const std::set<SCXML::Model::IStateNode *> &statesToEnter);
    void exitStates(const std::set<SCXML::Model::IStateNode *> &statesToExit);
    bool validateModel(std::shared_ptr<::SCXML::Model::DocumentModel> model);
    void resetStatistics();
    void updateStatistics(bool eventSuccess);
    std::string stateToString(State state) const;
    std::string generateSessionId();
};

}  // namespace SCXML