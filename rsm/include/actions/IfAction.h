#pragma once

#include "BaseAction.h"
#include <memory>
#include <vector>

namespace RSM {

/**
 * @brief SCXML <if>/<elseif>/<else> conditional execution action implementation
 *
 * The <if> element provides conditional execution of executable content.
 * This is one of the most critical SCXML control structures for decision logic.
 *
 * W3C SCXML Specification:
 * - <if> requires a 'cond' attribute with boolean expression
 * - <elseif> elements can follow with their own 'cond' attributes
 * - <else> element can be the final branch with no condition
 * - Only the first matching condition's content is executed
 *
 * Example SCXML:
 * <if cond="counter > 5">
 *     <assign location="status" expr="'high'"/>
 * <elseif cond="counter > 0"/>
 *     <assign location="status" expr="'medium'"/>
 * <else/>
 *     <assign location="status" expr="'low'"/>
 * </if>
 */
class IfAction : public BaseAction {
public:
    /**
     * @brief Conditional branch containing condition and executable content
     */
    struct ConditionalBranch {
        std::string condition;                              // Boolean expression (empty for <else>)
        std::vector<std::shared_ptr<IActionNode>> actions;  // Actions to execute if condition is true
        bool isElseBranch = false;                          // true for <else> branch

        ConditionalBranch() = default;

        ConditionalBranch(const std::string &cond) : condition(cond) {}

        ConditionalBranch(bool isElse) : isElseBranch(isElse) {}
    };

public:
    /**
     * @brief Construct a new If Action
     * @param condition Main if condition
     * @param id Action identifier (optional)
     */
    explicit IfAction(const std::string &condition = "", const std::string &id = "");

    /**
     * @brief Destructor
     */
    virtual ~IfAction() = default;

    /**
     * @brief Set the main if condition
     * @param condition Boolean expression for the if branch
     */
    void setIfCondition(const std::string &condition);

    /**
     * @brief Get the main if condition
     * @return If condition expression
     */
    const std::string &getIfCondition() const;

    /**
     * @brief Add executable content to the if branch
     * @param action Action to execute if condition is true
     */
    void addIfAction(std::shared_ptr<IActionNode> action);

    /**
     * @brief Add an elseif branch
     * @param condition Boolean expression for this elseif
     * @return Reference to the created elseif branch for adding actions
     */
    ConditionalBranch &addElseIfBranch(const std::string &condition);

    /**
     * @brief Add the else branch (unconditional fallback)
     * @return Reference to the else branch for adding actions
     */
    ConditionalBranch &addElseBranch();

    /**
     * @brief Add an if condition (for building complex if statements)
     * @param condition Boolean expression for the if branch
     */
    void addIfCondition(const std::string &condition);

    /**
     * @brief Add an elseif condition
     * @param condition Boolean expression for the elseif branch
     */
    void addElseIfCondition(const std::string &condition);

    /**
     * @brief Get a specific branch by index
     * @param index Branch index
     * @return Reference to the branch
     */
    const ConditionalBranch &getBranch(size_t index) const;

    /**
     * @brief Add action to a specific branch
     * @param branchIndex Branch index (0 = if, 1+ = elseif branches, last = else if exists)
     * @param action Action to add to the branch
     */
    void addActionToBranch(size_t branchIndex, std::shared_ptr<IActionNode> action);

    /**
     * @brief Get all conditional branches
     * @return Vector of all branches (if, elseif, else)
     */
    const std::vector<ConditionalBranch> &getBranches() const;

    /**
     * @brief Check if this if statement has an else branch
     * @return true if else branch exists
     */
    bool hasElseBranch() const;

    /**
     * @brief Get number of branches (if + elseif + else)
     * @return Total number of branches
     */
    size_t getBranchCount() const;

    // IActionNode implementation
    bool execute(IExecutionContext &context) override;
    std::string getActionType() const override;
    std::shared_ptr<IActionNode> clone() const override;

protected:
    // BaseAction implementation
    std::vector<std::string> validateSpecific() const override;
    std::string getSpecificDescription() const override;

private:
    std::vector<ConditionalBranch> branches_;  // All conditional branches

    /**
     * @brief Clone a conditional branch
     * @param original Original branch to clone
     * @return Deep copy of the branch
     */
    ConditionalBranch cloneBranch(const ConditionalBranch &original) const;
};

}  // namespace RSM