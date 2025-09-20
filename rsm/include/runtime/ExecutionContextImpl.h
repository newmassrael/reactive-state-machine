#pragma once

#include "IActionExecutor.h"
#include "IExecutionContext.h"
#include <memory>
#include <string>

namespace RSM {

/**
 * @brief Concrete implementation of IExecutionContext
 *
 * This class provides the execution context for SCXML action processing,
 * maintaining current state information and providing access to the
 * action executor.
 */
class ExecutionContextImpl : public IExecutionContext {
public:
    /**
     * @brief Construct execution context
     * @param executor Action executor to use
     * @param sessionId Current session identifier
     */
    ExecutionContextImpl(std::shared_ptr<IActionExecutor> executor, const std::string &sessionId);

    /**
     * @brief Destructor
     */
    virtual ~ExecutionContextImpl() = default;

    // IExecutionContext implementation
    IActionExecutor &getActionExecutor() override;
    std::string getCurrentSessionId() const override;
    std::string getCurrentEventData() const override;
    std::string getCurrentEventName() const override;
    std::string getCurrentStateId() const override;
    bool isValid() const override;

    /**
     * @brief Set current event information
     * @param eventName Event name
     * @param eventData Event data as JSON string
     */
    void setCurrentEvent(const std::string &eventName, const std::string &eventData);

    /**
     * @brief Set current state identifier
     * @param stateId Current state ID
     */
    void setCurrentStateId(const std::string &stateId);

    /**
     * @brief Clear current event data
     */
    void clearCurrentEvent();

private:
    std::shared_ptr<IActionExecutor> executor_;
    std::string sessionId_;
    std::string currentEventName_;
    std::string currentEventData_;
    std::string currentStateId_;
};

}  // namespace RSM