// IGuardNode.h
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Guard node interface
 *
 * This interface defines guard nodes that represent transition conditions.
 * Corresponds to <code:guard> element in SCXML documents.
 */

namespace RSM {

class IGuardNode {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IGuardNode() {}

    /**
     * @brief Return guard ID
     * @return Guard ID
     */
    virtual const std::string &getId() const = 0;

    /**
     * @brief Set target state
     * @param targetState Target state ID for transition
     */
    virtual void setTargetState(const std::string &targetState) = 0;

    /**
     * @brief Return target state
     * @return Target state ID for transition (if exists)
     */
    virtual const std::string &getTargetState() const = 0;

    /**
     * @brief Set condition expression
     * @param condition Guard condition expression
     */
    virtual void setCondition(const std::string &condition) = 0;

    /**
     * @brief Return condition expression
     * @return Guard condition expression
     */
    virtual const std::string &getCondition() const = 0;

    /**
     * @brief Add dependency
     * @param property Property name to depend on
     */
    virtual void addDependency(const std::string &property) = 0;

    /**
     * @brief Return list of dependencies
     * @return List of dependent property names
     */
    virtual const std::vector<std::string> &getDependencies() const = 0;

    /**
     * @brief Set external class
     * @param className External implementation class name
     */
    virtual void setExternalClass(const std::string &className) = 0;

    /**
     * @brief Return external class name
     * @return External implementation class name
     */
    virtual const std::string &getExternalClass() const = 0;

    /**
     * @brief Set external factory
     * @param factoryName External factory name
     */
    virtual void setExternalFactory(const std::string &factoryName) = 0;

    /**
     * @brief Return external factory name
     * @return External factory name
     */
    virtual const std::string &getExternalFactory() const = 0;

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
    virtual const std::string &getAttribute(const std::string &name) const = 0;

    /**
     * @brief Return all attributes
     * @return Map of all attributes
     */
    virtual const std::unordered_map<std::string, std::string> &getAttributes() const = 0;
};

}  // namespace RSM