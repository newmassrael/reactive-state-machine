#pragma once

#include "StateMachine.h"
#include "events/IEventDispatcher.h"
#include "runtime/IEventRaiser.h"
#include <memory>
#include <string>

namespace RSM {

/**
 * @brief Builder pattern for StateMachine construction with dependency injection
 *
 * Builder creates StateMachine with injected dependencies.
 * Caller is responsible for wrapping in StateMachineContext and managing
 * EventRaiser/EventDispatcher lifecycle separately.
 */
class StateMachineBuilder {
private:
    std::shared_ptr<IEventDispatcher> eventDispatcher_;
    std::shared_ptr<IEventRaiser> eventRaiser_;
    std::string sessionId_;

public:
    StateMachineBuilder() = default;

    /**
     * @brief Set EventDispatcher for send actions and delayed events
     * @param eventDispatcher Shared pointer to event dispatcher
     * @return Reference to builder for method chaining
     */
    StateMachineBuilder &withEventDispatcher(std::shared_ptr<IEventDispatcher> eventDispatcher) {
        eventDispatcher_ = eventDispatcher;
        return *this;
    }

    /**
     * @brief Set EventRaiser for raise actions and internal events
     * @param eventRaiser Shared pointer to event raiser
     * @return Reference to builder for method chaining
     */
    StateMachineBuilder &withEventRaiser(std::shared_ptr<IEventRaiser> eventRaiser) {
        eventRaiser_ = eventRaiser;
        return *this;
    }

    /**
     * @brief Set session ID for StateMachine (required for invoke scenarios)
     * @param sessionId Pre-existing session ID to use
     * @return Reference to builder for method chaining
     */
    StateMachineBuilder &withSessionId(const std::string &sessionId) {
        sessionId_ = sessionId;
        return *this;
    }

    /**
     * @brief Build StateMachine with dependency injection
     *
     * Returns StateMachine pointer.
     * Caller is responsible for wrapping in StateMachineContext and managing
     * EventRaiser/EventDispatcher lifecycle (e.g., via TestResources).
     *
     * @return Unique pointer to StateMachine
     * @throws std::runtime_error if required dependencies are missing
     */
    std::unique_ptr<StateMachine> build() {
        std::unique_ptr<StateMachine> stateMachine;

        // Create StateMachine with or without session ID
        if (!sessionId_.empty()) {
            stateMachine = std::make_unique<StateMachine>(sessionId_);
        } else {
            stateMachine = std::make_unique<StateMachine>();
        }

        // Inject dependencies after construction
        if (eventDispatcher_) {
            stateMachine->setEventDispatcher(eventDispatcher_);
        }

        if (eventRaiser_) {
            stateMachine->setEventRaiser(eventRaiser_);
        }

        return stateMachine;
    }
};

}  // namespace RSM