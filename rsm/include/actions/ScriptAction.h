#pragma once

#include "BaseAction.h"
#include <string>

namespace RSM {
namespace Actions {

/**
 * @brief SCXML <script> action implementation
 *
 * Executes JavaScript code within the SCXML data model context.
 * This is equivalent to the <script> element in SCXML specification.
 */
class ScriptAction : public BaseAction {
public:
    /**
     * @brief Construct script action with content
     * @param content JavaScript code to execute
     * @param id Optional action identifier
     */
    explicit ScriptAction(const std::string &content, const std::string &id = "");

    /**
     * @brief Destructor
     */
    virtual ~ScriptAction() = default;

    // IActionNode implementation
    bool execute(IExecutionContext &context) override;
    std::string getActionType() const override;
    std::shared_ptr<IActionNode> clone() const override;

    /**
     * @brief Get script content
     * @return JavaScript code string
     */
    const std::string &getContent() const;

    /**
     * @brief Set script content
     * @param content JavaScript code to execute
     */
    void setContent(const std::string &content);

    /**
     * @brief Check if script content is empty
     * @return true if no script content
     */
    bool isEmpty() const;

protected:
    // BaseAction implementation
    std::vector<std::string> validateSpecific() const override;
    std::string getSpecificDescription() const override;

private:
    std::string content_;
};

}  // namespace Actions
}  // namespace RSM