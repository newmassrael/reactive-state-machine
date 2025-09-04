#pragma once
#include "core/ActionNode.h"
#include <memory>
#include <vector>

namespace SCXML {
namespace Runtime {
class RuntimeContext;
}

class GuardEvaluator;

namespace Core {

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
class IfActionNode : public SCXML::Core::ActionNode {
public:
    /**
     * @brief Conditional branch containing condition and executable content
     */
    struct ConditionalBranch {
        std::string condition;                              // Boolean expression (empty for <else>)
        std::vector<std::shared_ptr<SCXML::Model::IActionNode>> actions;  // Actions to execute if condition is true
        bool isElseBranch = false;                          // true for <else> branch

        ConditionalBranch() = default;

        ConditionalBranch(const std::string &cond) : condition(cond) {}

        ConditionalBranch(bool isElse) : isElseBranch(isElse) {}
    };

public:
    /**
     * @brief Construct a new If Action Node
     * @param id Action identifier
     */
    explicit IfActionNode(const std::string &id);

    /**
     * @brief Destructor
     */
    virtual ~IfActionNode() = default;

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
    void addIfAction(std::shared_ptr<SCXML::Model::IActionNode> action);

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
     * @brief Add action to a specific branch
     * @param branchIndex Branch index (0 = if, 1+ = elseif branches, last = else if exists)
     * @param action Action to add to the branch
     */
    void addActionToBranch(size_t branchIndex, std::shared_ptr<SCXML::Model::IActionNode> action);

    /**
     * @brief Get all conditional branches
     * @return Vector of all branches (if, elseif, else)
     */
    const std::vector<ConditionalBranch> &getBranches() const {
        return branches_;
    }

    /**
     * @brief Check if this if statement has an else branch
     * @return true if else branch exists
     */
    bool hasElseBranch() const;

    /**
     * @brief Get number of branches (if + elseif + else)
     * @return Total number of branches
     */
    size_t getBranchCount() const {
        return branches_.size();
    }

    /**
     * @brief Execute the conditional logic
     * @param context Runtime context for execution
     * @return true if action executed successfully
     */
    bool execute(::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Get action type name
     * @return "if"
     */
    std::string getActionType() const {
        return "if";
    }

    /**
     * @brief Clone this action node
     * @return Deep copy of this IfActionNode
     */
    std::shared_ptr<SCXML::Model::IActionNode> clone() const;

    /**
     * @brief Validate the conditional structure
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validate() const;

protected:
    /**
     * @brief Evaluate a condition expression
     * @param condition Boolean expression to evaluate
     * @param context Runtime context
     * @return true if condition evaluates to true
     */
    bool evaluateCondition(const std::string &condition, ::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute all actions in a branch
     * @param branch Branch containing actions to execute
     * @param context Runtime context
     * @return true if all actions executed successfully
     */
    bool executeBranch(const ConditionalBranch &branch, ::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Find the first matching branch
     * @param context Runtime context for condition evaluation
     * @return Index of first matching branch, or SIZE_MAX if no match
     */
    size_t findMatchingBranch(::SCXML::Runtime::RuntimeContext &context);

private:
    std::vector<ConditionalBranch> branches_;         // All conditional branches
    std::shared_ptr<SCXML::GuardEvaluator> guardEvaluator_;  // For condition evaluation

    /**
     * @brief Initialize guard evaluator
     */
    void initializeGuardEvaluator();

    /**
     * @brief Clone a conditional branch
     * @param original Original branch to clone
     * @return Deep copy of the branch
     */
    ConditionalBranch cloneBranch(const ConditionalBranch &original) const;
};

} // namespace Core
}  // namespace SCXML
