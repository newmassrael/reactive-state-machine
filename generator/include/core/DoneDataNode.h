#pragma once

#include "model/IDataNode.h"
#include <memory>
#include <string>
#include <vector>

namespace SCXML {

// Forward declarations
namespace Runtime {
class RuntimeContext;
}

namespace Model {
class IContentionContext;
class IContentNode;
class IParamNode;
}

namespace Core {

using SCXML::Model::IDataNode;

/**
 * @brief SCXML <donedata> element implementation
 *
 * The <donedata> element is used to specify the data that should be returned
 * when a state machine reaches a final state or completes an atomic state.
 * This data is included in the 'done' event that is generated.
 *
 * SCXML W3C Specification compliance:
 * - Can contain <content> for structured data
 * - Can contain <param> elements for key-value pairs
 * - Used in final states and atomic state completion
 *
 * Thread Safety: This class is NOT thread-safe
 */
class DoneDataNode : public IDataNode {
public:
    /**
     * @brief Construct a new Done Data Node
     * @param id Node identifier (usually state ID + "_donedata")
     */
    explicit DoneDataNode(const std::string &id);

    /**
     * @brief Virtual destructor
     */
    virtual ~DoneDataNode() = default;

    /**
     * @brief Get the node identifier
     * @return Node ID string
     */
    const std::string &getId() const override {
        return id_;
    }

    /**
     * @brief Set content node for structured data
     * @param content Content node containing the data payload
     */
    void setContent(std::shared_ptr<Model::IContentNode> content);

    /**
     * @brief Get content node
     * @return Content node pointer (may be nullptr)
     */
    std::shared_ptr<Model::IContentNode> getContent() const {
        return content_;
    }

    /**
     * @brief Add parameter node for key-value data
     * @param param Parameter node to add
     */
    void addParam(std::shared_ptr<Model::IParamNode> param);

    /**
     * @brief Get all parameter nodes
     * @return Vector of parameter nodes
     */
    const std::vector<std::shared_ptr<Model::IParamNode>> &getParams() const {
        return params_;
    }

    /**
     * @brief Initialize the done data node
     * @param context Runtime context for initialization
     * @return true if initialization succeeded
     */
    bool initialize(SCXML::Model::IExecutionContext &context) override;

    /**
     * @brief Generate done data for state completion
     * @param context Runtime context for data evaluation
     * @return Generated data as string (JSON format)
     */
    std::string generateDoneData(SCXML::Model::IExecutionContext &context) const;

    /**
     * @brief Validate the done data configuration
     * @return List of validation errors (empty if valid)
     */
    std::vector<std::string> validate() const override;

    /**
     * @brief Clone this done data node
     * @return Deep copy of this DoneDataNode
     */
    std::shared_ptr<IDataNode> clone() const override;

    /**
     * @brief Check if done data is empty
     * @return true if no content or params are specified
     */
    bool isEmpty() const;

protected:
    /**
     * @brief Build data object from content and parameters
     * @param context Runtime context for evaluation
     * @return JSON string representation of the data
     */
    std::string buildDataObject(SCXML::Model::IExecutionContext &context) const;

    /**
     * @brief Escape string for JSON format
     * @param str String to escape
     * @return JSON-escaped string
     */
    std::string escapeJsonString(const std::string &str) const;

private:
    std::string id_;                                   // Node identifier
    std::shared_ptr<Model::IContentNode> content_;            // Structured content data
    std::vector<std::shared_ptr<Model::IParamNode>> params_;  // Parameter key-value pairs
};

} // namespace Core
}  // namespace SCXML