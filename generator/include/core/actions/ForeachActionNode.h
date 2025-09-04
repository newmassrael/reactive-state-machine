#pragma once
#include "common/Result.h"
#include "core/ActionNode.h"
#include <memory>
#include <vector>

namespace SCXML {
namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

namespace Runtime {
class RuntimeContext;
}

namespace Core {

/**
 * @brief SCXML <foreach> action implementation
 *
 * The <foreach> element provides iteration over array-like data structures.
 * It executes its contained actions once for each item in the specified array,
 * setting loop variables for the current item and index.
 */
class ForeachActionNode : public ActionNode {
public:
    /**
     * @brief Construct a new Foreach Action Node
     * @param id Action identifier
     */
    explicit ForeachActionNode(const std::string &id);

    /**
     * @brief Destructor
     */
    virtual ~ForeachActionNode() = default;

    /**
     * @brief Set the array to iterate over
     * @param array Array expression or variable name (e.g., "myArray", "data.items")
     */
    void setArray(const std::string &array);

    /**
     * @brief Get the array expression
     * @return array expression string
     */
    const std::string &getArray() const {
        return array_;
    }

    /**
     * @brief Set the item variable name for current element
     * @param item Variable name to hold current array element
     */
    void setItem(const std::string &item);

    /**
     * @brief Get the item variable name
     * @return item variable string
     */
    const std::string &getItem() const {
        return item_;
    }

    /**
     * @brief Set the index variable name for current position
     * @param index Variable name to hold current array index
     */
    void setIndex(const std::string &index);

    /**
     * @brief Get the index variable name
     * @return index variable string
     */
    const std::string &getIndex() const {
        return index_;
    }

    /**
     * @brief Add a child action to execute in each iteration
     * @param action Action to execute for each array element
     */
    void addIterationAction(std::shared_ptr<IActionNode> action);

    /**
     * @brief Get all iteration actions
     * @return Vector of actions to execute per iteration
     */
    const std::vector<std::shared_ptr<IActionNode>> &getIterationActions() const {
        return iterationActions_;
    }

    /**
     * @brief Execute the foreach action
     * @param context Runtime context for execution
     * @return true if foreach loop completed successfully
     */
    virtual bool execute(::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Get action type name
     * @return "foreach"
     */
    std::string getActionType() const {
        return "foreach";
    }

    /**
     * @brief Clone this action node
     * @return Deep copy of this ForeachActionNode
     */
    std::shared_ptr<IActionNode> clone() const;

    /**
     * @brief Validate foreach action configuration
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validate() const;

protected:
    /**
     * @brief Resolve the array to iterate over
     * @param context Runtime context for evaluation
     * @return Array data structure, empty if resolution fails
     */
    std::vector<std::string> resolveArray(::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute all actions for a single iteration
     * @param context Runtime context
     * @param itemValue Current item value
     * @param indexValue Current index value
     * @return true if all iteration actions succeeded
     */
    bool executeIteration(::SCXML::Runtime::RuntimeContext &context, const std::string &itemValue, int indexValue);

    /**
     * @brief Set loop variables in the data model
     * @param context Runtime context
     * @param itemValue Current item value
     * @param indexValue Current index value
     */
    void setLoopVariables(::SCXML::Runtime::RuntimeContext &context, const std::string &itemValue, int indexValue);

    /**
     * @brief Clean up loop variables from the data model
     * @param context Runtime context
     */
    void cleanupLoopVariables(::SCXML::Runtime::RuntimeContext &context);

private:
    std::string array_;  // Array expression to iterate over
    std::string item_;   // Variable name for current item
    std::string index_;  // Variable name for current index

    std::vector<std::shared_ptr<IActionNode>> iterationActions_;  // Actions to execute per iteration
};

} // namespace Core
}  // namespace SCXML
