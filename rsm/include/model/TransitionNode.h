// TransitionNode.h
#pragma once

#include "ITransitionNode.h"
#include "actions/IActionNode.h"
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Implementation class for transition node
 *
 * This class represents transitions between states.
 * Corresponds to <transition> element in SCXML documents.
 */

namespace RSM {

class TransitionNode : public ITransitionNode {
public:
    /**
     * @brief Constructor
     * @param event Transition event
     * @param target Target state ID
     */
    TransitionNode(const std::string &event, const std::string &target);

    /**
     * @brief Destructor
     */
    virtual ~TransitionNode();

    /**
     * @brief Return event
     * @return Transition event
     */
    virtual const std::string &getEvent() const override;

    /**
     * @brief Return list of target states
     * @return Vector of individual target state IDs
     */
    virtual std::vector<std::string> getTargets() const override;

    /**
     * @brief Add target state
     * @param target Target state ID to add
     */
    virtual void addTarget(const std::string &target) override;

    /**
     * @brief Remove all target states
     */
    virtual void clearTargets() override;

    /**
     * @brief Check if targets exist
     * @return true if one or more targets exist, false otherwise
     */
    virtual bool hasTargets() const override;

    /**
     * @brief Set guard condition
     * @param guard Guard condition ID
     */
    virtual void setGuard(const std::string &guard) override;

    /**
     * @brief Return guard condition
     * @return Guard condition ID
     */
    virtual const std::string &getGuard() const override;

    /**
     * @brief Add action
     * @param actionNode ActionNode object
     */
    virtual void addActionNode(std::shared_ptr<RSM::IActionNode> actionNode) override;

    /**
     * @brief Return list of ActionNodes (SCXML specification compliant)
     * @return List of ActionNode objects
     */
    virtual const std::vector<std::shared_ptr<RSM::IActionNode>> &getActionNodes() const override;

    /**
     * @brief Set reactive status
     * @param reactive Reactive status
     */
    virtual void setReactive(bool reactive) override;

    /**
     * @brief Return reactive status
     * @return Reactive status
     */
    virtual bool isReactive() const override;

    virtual void setInternal(bool internal) override;
    virtual bool isInternal() const override;
    virtual void setAttribute(const std::string &name, const std::string &value) override;
    virtual std::string getAttribute(const std::string &name) const override;
    virtual void addEvent(const std::string &event) override;
    virtual const std::vector<std::string> &getEvents() const override;

private:
    /**
     * @brief Parse target ID list from target string
     */
    void parseTargets() const;

    std::string event_;
    std::string target_;
    std::string guard_;
    std::vector<std::shared_ptr<RSM::IActionNode>> actionNodes_;  // Store ActionNodes (SCXML specification compliant)
    bool reactive_;
    bool internal_;
    std::unordered_map<std::string, std::string> attributes_;
    std::vector<std::string> events_;
    mutable std::vector<std::string> cachedTargets_;
    mutable bool targetsDirty_;  // Indicates whether target cache is up-to-date
};

}  // namespace RSM