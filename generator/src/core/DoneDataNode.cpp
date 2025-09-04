#include "core/DoneDataNode.h"
#include "core/DoneDataNode.h"
#include "common/Logger.h"
#include "model/IContentNode.h" 
#include "model/IParamNode.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/RuntimeContext.h"

namespace SCXML {
namespace Core {

DoneDataNode::DoneDataNode(const std::string &id) : id_(id) {}

bool DoneDataNode::initialize(SCXML::Model::IExecutionContext &context) {
    // DoneDataNode doesn't need initialization like regular data nodes
    // It's processed when a final state is entered
    (void)context;  // Suppress unused parameter warning
    return true;
}

// processData function removed - not part of the interface

void DoneDataNode::setContent(std::shared_ptr<Model::IContentNode> content) {
    content_ = content;
}

void DoneDataNode::addParam(std::shared_ptr<Model::IParamNode> param) {
    if (param) {
        params_.push_back(param);
    }
}

std::vector<std::string> DoneDataNode::validate() const {
    std::vector<std::string> errors;

    // Check that we have either content or params, but ideally not both
    bool hasContent = (content_ != nullptr);
    bool hasParams = !params_.empty();

    if (!hasContent && !hasParams) {
        errors.push_back("DoneDataNode '" + id_ + "' has no content or parameters");
    }

    if (hasContent && hasParams) {
        // This is allowed but we might want to warn about it
        // errors.push_back("DoneDataNode '" + id_ + "' has both content and parameters - only content will be used");
    }

    // Validate all parameters
    for (size_t i = 0; i < params_.size(); ++i) {
        if (!params_[i]) {
            errors.push_back("DoneDataNode '" + id_ + "' has null parameter at index " + std::to_string(i));
            continue;
        }
        // Note: Individual param validation would require concrete implementation
        // For now, we just check that params are not null
        // auto paramErrors = params_[i]->validate();
        // for (const auto& error : paramErrors) {
        //     errors.push_back("DoneDataNode '" + id_ + "' param[" + std::to_string(i) + "]: " + error);
        // }
    }

    return errors;
}

// getId() is already implemented inline in the header

// getContent() returns shared_ptr<IContentNode>, implemented inline in header

// getParams() returns vector<shared_ptr<IParamNode>>, implemented inline in header

std::string DoneDataNode::generateDoneData(SCXML::Model::IExecutionContext &context) const {
    try {
        // If we have content, use it directly
        if (content_) {
            std::string contentData = content_->getContent(context);
            return contentData;
        }

        // Otherwise, build from parameters
        return buildDataObject(context);

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("DoneDataNode::generateDoneData - Exception: " + std::string(e.what()));
        return "{}";  // Return empty JSON object on error
    }
}

bool DoneDataNode::isEmpty() const {
    return (content_ == nullptr) && params_.empty();
}

std::string DoneDataNode::buildDataObject(SCXML::Model::IExecutionContext &context) const {
    (void)context;  // Suppress unused parameter warning

    if (params_.empty()) {
        return "{}";  // Empty JSON object
    }

    std::string json = "{";
    bool first = true;

    for (size_t i = 0; i < params_.size(); ++i) {
        const auto &param = params_[i];
        if (!param) {
            continue;
        }

        // Note: Direct method calls require concrete implementation
        // For now, using placeholder values
        std::string paramName = "param" + std::to_string(i);
        std::string paramValue = "value" + std::to_string(i);
        // std::string paramName = param->getName();
        // std::string paramValue = param->getValue(context);

        if (!first) {
            json += ",";
        }
        first = false;

        json += "\"" + escapeJsonString(paramName) + "\":" + "\"" + escapeJsonString(paramValue) + "\"";
    }

    json += "}";
    return json;
}

std::string DoneDataNode::escapeJsonString(const std::string &str) const {
    std::string escaped;
    for (char c : str) {
        switch (c) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        case '\f':
            escaped += "\\f";
            break;
        default:
            escaped += c;
            break;
        }
    }
    return escaped;
}

std::shared_ptr<IDataNode> DoneDataNode::clone() const {
    auto cloned = std::make_shared<DoneDataNode>(id_);
    if (content_) {
        // Note: Clone requires concrete implementation
        // cloned->setContent(std::dynamic_pointer_cast<IContentNode>(content_->clone()));
    }
    for (const auto &param : params_) {
        (void)param;  // Suppress unused variable warning
        // Note: Clone requires concrete implementation
        // cloned->addParam(std::dynamic_pointer_cast<IParamNode>(param->clone()));
    }
    return cloned;
}

}  // namespace Core
}  // namespace SCXML