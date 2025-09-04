#pragma once
#include "common/SCXMLCommon.h"
#include "model/IContentNode.h"
#include "model/IExecutionContext.h"
#include <string>
#include <vector>

using SCXML::Model::IContentNode;
using SCXML::Model::IExecutionContext;


namespace SCXML {
namespace Core {

// Forward declarations

/**
 * @brief Implementation of SCXML <content> element
 *
 * The ContentNode represents content data that can be included in various
 * SCXML elements like <send>, <donedata>, etc. It supports both inline
 * content and dynamic content generation through expressions.
 *
 * Features:
 * - Inline text content
 * - Expression-based dynamic content generation
 * - Content type handling (text, XML, JSON)
 * - Runtime content evaluation
 *
 * SCXML W3C Specification compliance:
 * - Supports expr attribute for dynamic content
 * - Handles inline content properly
 * - Integrates with data model for expression evaluation
 */
class ContentNode : public IContentNode {
private:
    std::string id_;             ///< Optional identifier
    std::string inlineContent_;  ///< Inline content string
    std::string expression_;     ///< Expression for dynamic content
    bool hasExpression_;         ///< True if using expression vs inline content

public:
    /**
     * @brief Constructor
     * @param id Optional identifier for the content node
     */
    explicit ContentNode(const std::string &id = "");

    /**
     * @brief Destructor
     */
    virtual ~ContentNode() = default;

    // IDataNode interface
    bool initialize(SCXML::Model::IExecutionContext &context) override;
    std::vector<std::string> validate() const override;
    std::shared_ptr<Model::IDataNode> clone() const override;

    // IContentNode interface
    std::string getContent(SCXML::Model::IExecutionContext &context) const override;
    void setInlineContent(const std::string &content) override;
    void setContentExpression(const std::string &expr) override;
    bool hasExpression() const override;
    const std::string &getInlineContent() const override;
    const std::string &getExpression() const override;

    // Additional methods
    /**
     * @brief Get the node identifier
     * @return The node ID
     */
    const std::string &getId() const;

    /**
     * @brief Check if content is empty
     * @return True if no content or expression is set
     */
    bool isEmpty() const;

    /**
     * @brief Clear all content and expressions
     */
    void clear();
};

} // namespace Core
}  // namespace SCXML
