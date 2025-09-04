#include "core/ContentNode.h"
#include "common/Logger.h"
#include "core/ContentNode.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/RuntimeContext.h"

namespace SCXML {
namespace Core {

ContentNode::ContentNode(const std::string &id) : id_(id), hasExpression_(false) {}

bool ContentNode::initialize(SCXML::Model::IExecutionContext &context) {
    (void)context;  // Suppress unused parameter warning
    // Content nodes don't require special initialization
    // They are evaluated when needed
    SCXML::Common::Logger::debug("ContentNode: Initialized content node" + (id_.empty() ? "" : " with id: " + id_));
    return true;
}

std::string ContentNode::getContent(SCXML::Model::IExecutionContext &context) const {
    (void)context;  // Suppress unused parameter warning

    try {
        if (hasExpression_ && !expression_.empty()) {
            // Note: Expression evaluation requires concrete runtime context
            // For now, return the expression itself as placeholder
            SCXML::Common::Logger::debug("ContentNode: Expression placeholder: " + expression_);
            return expression_;  // Placeholder - should evaluate expression
        } else {
            // Return inline content
            SCXML::Common::Logger::debug("ContentNode: Returning inline content");
            return inlineContent_;
        }
    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("ContentNode: Exception in getContent: " + std::string(e.what()));
        return inlineContent_;
    }
}

const std::string &ContentNode::getExpression() const {
    return expression_;
}

bool ContentNode::isEmpty() const {
    if (hasExpression_) {
        return expression_.empty();
    } else {
        return inlineContent_.empty();
    }
}

void ContentNode::clear() {
    inlineContent_.clear();
    expression_.clear();
    hasExpression_ = false;
    SCXML::Common::Logger::debug("ContentNode: Cleared all content" + (id_.empty() ? "" : " for id: " + id_));
}

std::shared_ptr<Model::IDataNode> ContentNode::clone() const {
    auto cloned = std::make_shared<ContentNode>(id_);
    if (hasExpression_) {
        cloned->setContentExpression(expression_);
    } else {
        cloned->setInlineContent(inlineContent_);
    }
    return cloned;
}

}  // namespace Core
}  // namespace SCXML
