#pragma once

#include "IActionNode.h"
#include <string>

namespace RSM {

/**
 * @brief Base implementation for common action functionality
 *
 * This abstract base class provides common functionality for all
 * action implementations, following the Template Method pattern.
 */
class BaseAction : public IActionNode {
public:
    /**
     * @brief Construct base action with optional ID
     * @param id Action identifier (optional)
     */
    explicit BaseAction(const std::string &id = "");

    /**
     * @brief Virtual destructor
     */
    virtual ~BaseAction() = default;

    // IActionNode implementation (common parts)
    std::string getId() const override;
    void setId(const std::string &id) override;
    std::string getDescription() const override;
    std::vector<std::string> validate() const override;

protected:
    /**
     * @brief Validate action-specific configuration
     * @return Vector of validation errors (empty if valid)
     */
    virtual std::vector<std::string> validateSpecific() const = 0;

    /**
     * @brief Get action-specific description
     * @return Description string for this specific action type
     */
    virtual std::string getSpecificDescription() const = 0;

    /**
     * @brief Check if a string is empty or whitespace only
     * @param str String to check
     * @return true if string is effectively empty
     */
    bool isEmptyString(const std::string &str) const;

    /**
     * @brief Trim whitespace from string
     * @param str String to trim
     * @return Trimmed string
     */
    std::string trimString(const std::string &str) const;

private:
    std::string id_;
};

}  // namespace RSM