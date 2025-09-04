#include "core/actions/ForeachActionNode.h"
#include "common/Logger.h"
#include "runtime/DataModelEngine.h"
#include "runtime/RuntimeContext.h"
#include <sstream>

namespace SCXML {
namespace Core {

ForeachActionNode::ForeachActionNode(const std::string &id) : ActionNode(id) {
    SCXML::Common::Logger::debug("ForeachActionNode::Constructor - Creating foreach action: " + id);
}

void ForeachActionNode::setArray(const std::string &array) {
    array_ = array;
    SCXML::Common::Logger::debug("ForeachActionNode::setArray - Set array: " + array);
}

void ForeachActionNode::setItem(const std::string &item) {
    item_ = item;
    SCXML::Common::Logger::debug("ForeachActionNode::setItem - Set item variable: " + item);
}

void ForeachActionNode::setIndex(const std::string &index) {
    index_ = index;
    SCXML::Common::Logger::debug("ForeachActionNode::setIndex - Set index variable: " + index);
}

void ForeachActionNode::addIterationAction(std::shared_ptr<SCXML::Model::IActionNode> action) {
    if (action) {
        iterationActions_.push_back(action);
        SCXML::Common::Logger::debug("ForeachActionNode::addIterationAction - Added iteration action: " + action->getId());
    }
}

bool ForeachActionNode::execute(SCXML::Runtime::RuntimeContext &context) {
    SCXML::Common::Logger::debug("ForeachActionNode::execute - Executing foreach action: " + getId());

    // Validate configuration
    auto errors = validate();
    if (!errors.empty()) {
        SCXML::Common::Logger::error("ForeachActionNode::execute - Validation errors:");
        for (const auto &error : errors) {
            SCXML::Common::Logger::error("  " + error);
        }
        return false;
    }

    try {
        // Resolve the array to iterate over
        std::vector<std::string> arrayValues = resolveArray(context);

        SCXML::Common::Logger::debug("ForeachActionNode::execute - Iterating over array with " + std::to_string(arrayValues.size()) +
                      " elements");

        // Execute iterations
        bool allSucceeded = true;
        for (size_t i = 0; i < arrayValues.size(); ++i) {
            SCXML::Common::Logger::debug("ForeachActionNode::execute - Executing iteration " + std::to_string(i));

            auto iterationResult = executeIteration(context, arrayValues[i], static_cast<int>(i));
            if (!iterationResult) {
                SCXML::Common::Logger::warning("ForeachActionNode::execute - Iteration " + std::to_string(i) + " failed");
                allSucceeded = false;
                // Continue with remaining iterations (SCXML behavior)
            }
        }

        // Clean up loop variables
        cleanupLoopVariables(context);

        SCXML::Common::Logger::debug("ForeachActionNode::execute - Foreach completed, success: " +
                      std::string(allSucceeded ? "true" : "false"));

        if (allSucceeded) {
            return true;  // Success
        } else {
            return false;
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("ForeachActionNode::execute - Exception during foreach execution: " + std::string(e.what()));
        cleanupLoopVariables(context);
        return false;
    }
}

std::shared_ptr<SCXML::Model::IActionNode> ForeachActionNode::clone() const {
    auto clone = std::make_shared<ForeachActionNode>(getId());
    clone->setArray(array_);
    clone->setItem(item_);
    clone->setIndex(index_);

    // Clone iteration actions
    for (const auto &action : iterationActions_) {
        if (action) {
            auto clonedAction = action->clone();
            clone->addIterationAction(clonedAction);
        }
    }

    return clone;
}

std::vector<std::string> ForeachActionNode::validate() const {
    std::vector<std::string> errors;

    // Array expression is required
    if (array_.empty()) {
        errors.push_back("Foreach action must have an 'array' attribute");
    }

    // Item variable is required
    if (item_.empty()) {
        errors.push_back("Foreach action must have an 'item' attribute");
    }

    // Index variable is optional but commonly used
    // No validation required for index_

    return errors;
}

std::vector<std::string> ForeachActionNode::resolveArray(SCXML::Runtime::RuntimeContext &context) {
    std::vector<std::string> result;

    if (array_.empty()) {
        return result;
    }

    // Get data model engine
    auto *dataModel = context.getDataModelEngine();
    if (!dataModel) {
        SCXML::Common::Logger::warning("ForeachActionNode::resolveArray - No data model engine available");
        return result;
    }

    try {
        // Try to resolve array as a variable or expression
        auto evalResult = dataModel->evaluateExpression(array_, context);
        if (!evalResult.success) {
            SCXML::Common::Logger::warning("ForeachActionNode::resolveArray - Failed to evaluate array expression: " +
                            evalResult.errorMessage);
            return result;
        }

        // Convert to array of strings
        // This is a simplified implementation - a full implementation would handle
        // various array types from the data model
        std::string arrayStr = dataModel->valueToString(evalResult.value);

        // Simple parsing: assume comma-separated values for now
        // A full implementation would handle JSON arrays, etc.
        if (!arrayStr.empty()) {
            std::istringstream stream(arrayStr);
            std::string item;

            while (std::getline(stream, item, ',')) {
                // Trim whitespace
                size_t start = item.find_first_not_of(" 	");
                size_t end = item.find_last_not_of(" 	");

                if (start != std::string::npos && end != std::string::npos) {
                    result.push_back(item.substr(start, end - start + 1));
                }
            }
        }

        SCXML::Common::Logger::debug("ForeachActionNode::resolveArray - Resolved array with " + std::to_string(result.size()) +
                      " elements");

    } catch (const std::exception &e) {
        SCXML::Common::Logger::warning("ForeachActionNode::resolveArray - Failed to resolve array: " + std::string(e.what()));
    }

    return result;
}

bool ForeachActionNode::executeIteration(SCXML::Runtime::RuntimeContext &context, const std::string &itemValue,
                                         int indexValue) {
    SCXML::Common::Logger::debug("ForeachActionNode::executeIteration - Executing iteration with item='" + itemValue +
                  "', index=" + std::to_string(indexValue));

    // Set loop variables
    setLoopVariables(context, itemValue, indexValue);

    // Execute all iteration actions
    bool allSucceeded = true;
    for (const auto &action : iterationActions_) {
        if (action) {
            SCXML::Common::Logger::debug("ForeachActionNode::executeIteration - Executing action: " + action->getId());

            // Execute action directly through interface
            bool actionResult = action->execute(context);
            if (!actionResult) {
                SCXML::Common::Logger::warning("ForeachActionNode::executeIteration - Action failed: " + action->getId());
                allSucceeded = false;
                // Continue with remaining actions in iteration
            }
        }
    }
    
    return allSucceeded;
}

void ForeachActionNode::setLoopVariables(SCXML::Runtime::RuntimeContext &context, const std::string &itemValue,
                                         int indexValue) {
    auto *dataModel = context.getDataModelEngine();
    if (!dataModel) {
        SCXML::Common::Logger::warning("ForeachActionNode::setLoopVariables - No data model engine available");
        return;
    }

    // Set item variable
    if (!item_.empty()) {
        auto setResult = dataModel->setValue(item_, itemValue);
        if (setResult.success) {
            SCXML::Common::Logger::debug("ForeachActionNode::setLoopVariables - Set " + item_ + " = '" + itemValue + "'");
        } else {
            SCXML::Common::Logger::warning("ForeachActionNode::setLoopVariables - Failed to set " + item_ + ": " +
                            setResult.errorMessage);
        }
    }

    // Set index variable if specified
    if (!index_.empty()) {
        auto setResult = dataModel->setValue(index_, std::to_string(indexValue));
        if (setResult.success) {
            SCXML::Common::Logger::debug("ForeachActionNode::setLoopVariables - Set " + index_ + " = " +
                          std::to_string(indexValue));
        } else {
            SCXML::Common::Logger::warning("ForeachActionNode::setLoopVariables - Failed to set " + index_ + ": " +
                            setResult.errorMessage);
        }
    }
}

void ForeachActionNode::cleanupLoopVariables(SCXML::Runtime::RuntimeContext &context) {
    auto *dataModel = context.getDataModelEngine();
    if (!dataModel) {
        return;
    }

    // Remove item variable
    if (!item_.empty()) {
        auto removeResult = dataModel->removeValue(item_);
        if (removeResult.success) {
            SCXML::Common::Logger::debug("ForeachActionNode::cleanupLoopVariables - Removed " + item_);
        } else {
            SCXML::Common::Logger::debug("ForeachActionNode::cleanupLoopVariables - Could not remove " + item_ + ": " +
                          removeResult.errorMessage);
        }
    }

    // Remove index variable
    if (!index_.empty()) {
        auto removeResult = dataModel->removeValue(index_);
        if (removeResult.success) {
            SCXML::Common::Logger::debug("ForeachActionNode::cleanupLoopVariables - Removed " + index_);
        } else {
            SCXML::Common::Logger::debug("ForeachActionNode::cleanupLoopVariables - Could not remove " + index_ + ": " +
                          removeResult.errorMessage);
        }
    }
}

}  // namespace Core
}  // namespace SCXML
