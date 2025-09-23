#pragma once

#include "BaseAction.h"
#include <memory>
#include <vector>

namespace RSM {

/**
 * @brief SCXML <foreach> action implementation
 *
 * The <foreach> element provides iteration over array-like data structures.
 * It executes its contained actions once for each item in the specified array,
 * setting loop variables for the current item and optional index.
 *
 * W3C SCXML Specification:
 * - <foreach> requires an 'array' attribute with iterable collection expression
 * - <foreach> requires an 'item' attribute with variable name for current element
 * - <foreach> supports optional 'index' attribute with variable name for current position
 * - If array is not iterable or item is invalid variable name, generate error.execution
 * - If any child action fails, terminate foreach and generate error.execution
 *
 * Example SCXML:
 * <foreach array="users" item="user" index="i">
 *     <assign location="user.processed" expr="true"/>
 *     <log expr="'Processing user ' + i + ': ' + user.name"/>
 * </foreach>
 */
class ForeachAction : public BaseAction {
public:
    /**
     * @brief Construct a new Foreach Action
     * @param array Array expression to iterate over (optional)
     * @param item Variable name for current item (optional)
     * @param index Variable name for current index (optional)
     * @param id Action identifier (optional)
     */
    explicit ForeachAction(const std::string &array = "", const std::string &item = "", const std::string &index = "",
                           const std::string &id = "");

    /**
     * @brief Destructor
     */
    virtual ~ForeachAction() = default;

    /**
     * @brief Set the array expression to iterate over
     * @param array Array expression or variable name (e.g., "users", "data.items", "[1,2,3]")
     */
    void setArray(const std::string &array);

    /**
     * @brief Get the array expression
     * @return Array expression string
     */
    const std::string &getArray() const;

    /**
     * @brief Set the item variable name for current element
     * @param item Variable name to hold current array element during iteration
     */
    void setItem(const std::string &item);

    /**
     * @brief Get the item variable name
     * @return Item variable name string
     */
    const std::string &getItem() const;

    /**
     * @brief Set the index variable name for current position
     * @param index Variable name to hold current array index during iteration
     */
    void setIndex(const std::string &index);

    /**
     * @brief Get the index variable name
     * @return Index variable name string
     */
    const std::string &getIndex() const;

    /**
     * @brief Add a child action to execute in each iteration
     * @param action Action to execute for each array element
     */
    void addIterationAction(std::shared_ptr<IActionNode> action);

    /**
     * @brief Get all iteration actions
     * @return Vector of actions to execute per iteration
     */
    const std::vector<std::shared_ptr<IActionNode>> &getIterationActions() const;

    /**
     * @brief Remove all iteration actions
     */
    void clearIterationActions();

    /**
     * @brief Get number of iteration actions
     * @return Count of actions to execute per iteration
     */
    size_t getIterationActionCount() const;

    // IActionNode implementation
    bool execute(IExecutionContext &context) override;
    std::string getActionType() const override;
    std::shared_ptr<IActionNode> clone() const override;

protected:
    // BaseAction implementation
    std::vector<std::string> validateSpecific() const override;
    std::string getSpecificDescription() const override;

private:
    std::string array_;  // Array expression to iterate over
    std::string item_;   // Variable name for current item
    std::string index_;  // Variable name for current index (optional)

    std::vector<std::shared_ptr<IActionNode>> iterationActions_;  // Actions to execute per iteration

    /**
     * @brief Clone all iteration actions for deep copy
     * @param source Source actions to clone
     * @return Vector of cloned actions
     */
    std::vector<std::shared_ptr<IActionNode>>
    cloneIterationActions(const std::vector<std::shared_ptr<IActionNode>> &source) const;
};

}  // namespace RSM