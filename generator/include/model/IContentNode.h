#pragma once
#include "IExecutionContext.h"
#include "model/IDataNode.h"
#include <string>
#include <vector>

namespace SCXML {
namespace Model {

// Forward declarations

/**
 * @brief Interface for SCXML <content> element nodes
 *
 * The <content> element allows SCXML documents to include data content
 * for send events or donedata. It supports both inline content and
 * expression-based content generation.
 *
 * SCXML W3C Specification:
 * - Can contain inline content (text, XML, JSON)
 * - Supports expr attribute for dynamic content generation
 * - Used within <send>, <donedata>, and other data-carrying elements
 */
class IContentNode : public IDataNode {
public:
    virtual ~IContentNode() = default;

    /**
     * @brief Get the content as a string
     * @param context Runtime context for expression evaluation
     * @return Content string (either inline or evaluated expression)
     */
    virtual std::string getContent(SCXML::Model::IExecutionContext &context) const = 0;

    /**
     * @brief Set inline content
     * @param content The content string
     */
    virtual void setInlineContent(const std::string &content) = 0;

    /**
     * @brief Set expression for dynamic content
     * @param expr The expression to evaluate
     */
    virtual void setContentExpression(const std::string &expr) = 0;

    /**
     * @brief Check if this content node has an expression
     * @return True if using expression, false if using inline content
     */
    virtual bool hasExpression() const = 0;

    /**
     * @brief Get inline content directly
     * @return The inline content string
     */
    virtual const std::string &getInlineContent() const = 0;

    /**
     * @brief Get expression directly
     * @return The expression string
     */
    virtual const std::string &getExpression() const = 0;
};

}  // namespace Model
}  // namespace SCXML
