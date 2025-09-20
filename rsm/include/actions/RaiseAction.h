#pragma once

#include "BaseAction.h"
#include <string>

namespace RSM {

/**
 * @brief SCXML <raise> action implementation
 *
 * The <raise> element is used to raise internal events within the same SCXML interpreter.
 * These events are processed immediately and have higher priority than external events.
 *
 * Example SCXML:
 * <raise event="internal.ready" data="'success'"/>
 */
class RaiseAction : public BaseAction {
public:
    /**
     * @brief Construct a new Raise Action
     * @param event Event name to raise
     * @param id Action identifier (optional)
     */
    explicit RaiseAction(const std::string &event = "", const std::string &id = "");

    /**
     * @brief Destructor
     */
    virtual ~RaiseAction() = default;

    /**
     * @brief Set the event name to raise
     * @param event Event name (e.g., "internal.ready", "error.validation")
     */
    void setEvent(const std::string &event);

    /**
     * @brief Get the event name
     * @return Event name string
     */
    const std::string &getEvent() const;

    /**
     * @brief Set event data payload
     * @param data Data to include with the raised event
     */
    void setData(const std::string &data);

    /**
     * @brief Get event data
     * @return Event data string
     */
    const std::string &getData() const;

    // IActionNode implementation
    bool execute(IExecutionContext &context) override;
    std::string getActionType() const override;
    std::shared_ptr<IActionNode> clone() const override;

protected:
    // BaseAction implementation
    std::vector<std::string> validateSpecific() const override;
    std::string getSpecificDescription() const override;

private:
    std::string event_;  // Event name to raise
    std::string data_;   // Event data payload
};

}  // namespace RSM