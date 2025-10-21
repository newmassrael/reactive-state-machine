// ITransitionNode.h
#pragma once

#include <memory>
#include <string>
#include <vector>

namespace RSM {
class IActionNode;
}

/**
 * @brief Transition node interface
 *
 * This interface represents transitions between states.
 * Corresponds to <transition> element in SCXML documents.
 */

namespace RSM {

class ITransitionNode {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~ITransitionNode() {}

    /**
     * @brief Return event
     * @return Transition event
     */
    virtual const std::string &getEvent() const = 0;

    /**
     * @brief Return list of target states
     * @return Vector of individual target state IDs
     */
    virtual std::vector<std::string> getTargets() const = 0;

    /**
     * @brief Add target state
     * @param target Target state ID to add
     */
    virtual void addTarget(const std::string &target) = 0;

    /**
     * @brief Remove all target states
     */
    virtual void clearTargets() = 0;

    /**
     * @brief Check if targets exist
     * @return true if one or more targets exist, false otherwise
     */
    virtual bool hasTargets() const = 0;

    /**
     * @brief Set guard condition
     * @param guard Guard condition ID
     */
    virtual void setGuard(const std::string &guard) = 0;

    /**
     * @brief Return guard condition
     * @return Guard condition ID
     */
    virtual const std::string &getGuard() const = 0;

    /**
     * @brief Add ActionNode (SCXML specification compliant)
     * @param actionNode ActionNode object
     */
    virtual void addActionNode(std::shared_ptr<RSM::IActionNode> actionNode) = 0;

    /**
     * @brief Return list of ActionNodes (SCXML specification compliant)
     * @return List of ActionNode objects
     */
    virtual const std::vector<std::shared_ptr<RSM::IActionNode>> &getActionNodes() const = 0;

    /**
     * @brief Set reactive status
     * @param reactive Reactive status
     */
    virtual void setReactive(bool reactive) = 0;

    /**
     * @brief Return reactive status
     * @return Reactive status
     */
    virtual bool isReactive() const = 0;

    /**
     * @brief Set internal transition status
     * @param internal Internal transition status
     */
    virtual void setInternal(bool internal) = 0;

    /**
     * @brief Return internal transition status
     * @return Internal transition status
     */
    virtual bool isInternal() const = 0;

    /**
     * @brief Set attribute
     * @param name Attribute name
     * @param value Attribute value
     */
    virtual void setAttribute(const std::string &name, const std::string &value) = 0;

    /**
     * @brief Return attribute value
     * @param name Attribute name
     * @return Attribute value, empty string if not found
     */
    virtual std::string getAttribute(const std::string &name) const = 0;

    /**
     * @brief Add event
     * @param event Event name
     */
    virtual void addEvent(const std::string &event) = 0;

    /**
     * @brief Return list of events
     * @return List of event names
     */
    virtual const std::vector<std::string> &getEvents() const = 0;
};

}  // namespace RSM