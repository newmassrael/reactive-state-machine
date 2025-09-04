#include "core/DataNode.h"
#include "common/Logger.h"
#include "core/DataNode.h"
#include "runtime/DataModelEngine.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace SCXML {
namespace Core {

DataNode::DataNode(const std::string &id) : id_(id), src_(), expr_(), content_(), format_("text") {
    SCXML::Common::Logger::debug("DataNode::Constructor - Creating data node: " + id);
}

void DataNode::setSrc(const std::string &src) {
    src_ = src;
    SCXML::Common::Logger::debug("DataNode::setSrc - Set source: " + src);
}

void DataNode::setExpr(const std::string &expr) {
    expr_ = expr;
    SCXML::Common::Logger::debug("DataNode::setExpr - Set expression: " + expr);
}

void DataNode::setContent(const std::string &content) {
    content_ = content;
    SCXML::Common::Logger::debug("DataNode::setContent - Set content (" + std::to_string(content.length()) + " chars)");
}

void DataNode::setFormat(const std::string &format) {
    format_ = format;
    SCXML::Common::Logger::debug("DataNode::setFormat - Set format: " + format);
}

bool DataNode::initialize(::SCXML::Runtime::RuntimeContext &context) {
    SCXML::Common::Logger::debug("DataNode::initialize - Initializing data variable: " + id_);

    try {
        // Get data model engine
        auto dataModel = context.getDataModelEngine();
        if (!dataModel) {
            SCXML::Common::Logger::error("DataNode::initialize - No data model engine available");
            return false;
        }

        // Get the initial value
        DataValue value = getValue(context);

        // Convert value to string for data model
        std::string valueStr;
        std::visit(
            [&valueStr](const auto &v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    valueStr = "";
                } else if constexpr (std::is_same_v<T, std::string>) {
                    valueStr = v;
                } else if constexpr (std::is_same_v<T, int>) {
                    valueStr = std::to_string(v);
                } else if constexpr (std::is_same_v<T, double>) {
                    valueStr = std::to_string(v);
                } else if constexpr (std::is_same_v<T, bool>) {
                    valueStr = v ? "true" : "false";
                }
            },
            value);

        // Set the variable in data model
        auto result = dataModel->setValue(id_, valueStr);
        if (result.success) {
            SCXML::Common::Logger::info("DataNode::initialize - Successfully initialized " + id_ + " = '" + valueStr + "'");
            return true;
        } else {
            SCXML::Common::Logger::error("DataNode::initialize - Failed to set variable " + id_ + ": " + result.errorMessage);
            return false;
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("DataNode::initialize - Exception initializing " + id_ + ": " + std::string(e.what()));
        return false;
    }

    // Success
    return true;
}

bool DataNode::supportsDataModel(const std::string &dataModelType) const {
    // DataNode supports all data model types
    (void)dataModelType;  // Suppress unused parameter warning
    return true;
}

std::optional<std::string> DataNode::queryXPath(const std::string &xpath) const {
    // XPath not supported for DataNode
    (void)xpath;  // Suppress unused parameter warning
    return std::nullopt;
}

bool DataNode::isXmlContent() const {
    // DataNode is not XML content
    return false;
}

const std::vector<std::string> &DataNode::getContentItems() const {
    return contentItems_;
}

void DataNode::addContent(const std::string &content) {
    contentItems_.push_back(content);
}

const std::unordered_map<std::string, std::string> &DataNode::getAttributes() const {
    return attributes_;
}

const std::string &DataNode::getAttribute(const std::string &name) const {
    static const std::string emptyString;
    auto it = attributes_.find(name);
    return (it != attributes_.end()) ? it->second : emptyString;
}

void DataNode::setAttribute(const std::string &name, const std::string &value) {
    attributes_[name] = value;
}

void DataNode::setType(const std::string &type) {
    type_ = type;
}

const std::string &DataNode::getType() const {
    return type_;
}

void DataNode::setScope(const std::string &scope) {
    scope_ = scope;
}

const std::string &DataNode::getScope() const {
    return scope_;
}

std::vector<std::string> DataNode::validate() const {
    // DataNode validation - return empty vector for no errors
    return std::vector<std::string>();
}

DataNode::DataValue DataNode::getValue(::SCXML::Runtime::RuntimeContext &context) const {
    // Priority order: expr > src > content > undefined

    // 1. Expression has highest priority
    if (!expr_.empty()) {
        return evaluateExpression(context);
    }

    // 2. External source
    if (!src_.empty()) {
        std::string srcContent = loadFromSrc(context);
        if (!srcContent.empty()) {
            return parseContent(srcContent, format_);
        }
    }

    // 3. Inline content
    if (!content_.empty()) {
        return parseContent(content_, format_);
    }

    // 4. Default to undefined
    return std::monostate{};
}

std::string DataNode::loadFromSrc(::SCXML::Runtime::RuntimeContext &context) const {
    (void)context;  // Suppress unused parameter warning
    if (src_.empty()) {
        return "";
    }

    try {
        // For now, support local file paths
        // In a full implementation, this would support HTTP/HTTPS URLs
        std::ifstream file(src_);
        if (!file.is_open()) {
            SCXML::Common::Logger::error("DataNode::loadFromSrc - Cannot open data source: " + src_);
            return "";
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        SCXML::Common::Logger::debug("DataNode::loadFromSrc - Loaded " + std::to_string(content.length()) + " characters from " +
                      src_);

        return content;

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("DataNode::loadFromSrc - Exception loading from " + src_ + ": " + std::string(e.what()));
        return "";
    }
}

DataNode::DataValue DataNode::parseContent(const std::string &content, const std::string &format) const {
    if (content.empty()) {
        return std::monostate{};
    }

    std::string lowerFormat = format;
    std::transform(lowerFormat.begin(), lowerFormat.end(), lowerFormat.begin(), ::tolower);

    try {
        if (lowerFormat == "json") {
            // Simple JSON parsing - in a full implementation, use a JSON library
            std::string trimmed = content;
            // Remove leading/trailing whitespace
            trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(),
                                                        [](unsigned char ch) { return !std::isspace(ch); }));
            trimmed.erase(
                std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                    .base(),
                trimmed.end());

            // Simple type detection
            if (trimmed == "true") {
                return true;
            }
            if (trimmed == "false") {
                return false;
            }
            if (trimmed == "null" || trimmed.empty()) {
                return std::monostate{};
            }

            // Try integer
            if (std::regex_match(trimmed, std::regex(R"(-?\d+)"))) {
                return std::stoi(trimmed);
            }

            // Try double
            if (std::regex_match(trimmed, std::regex(R"(-?\d+\.\d+)"))) {
                return std::stod(trimmed);
            }

            // String (remove quotes if present)
            if (trimmed.front() == '"' && trimmed.back() == '"' && trimmed.length() >= 2) {
                return trimmed.substr(1, trimmed.length() - 2);
            }

            // Default to string
            return trimmed;

        } else if (lowerFormat == "ecmascript") {
            // For ECMAScript format, return as string for now
            // A full implementation would evaluate the expression
            return content;

        } else if (lowerFormat == "xml") {
            // For XML format, return as string for now
            // A full implementation would parse XML structure
            return content;

        } else {  // text or unknown
            // Try to detect type from plain text
            std::string trimmed = content;
            // Remove leading/trailing whitespace
            trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(),
                                                        [](unsigned char ch) { return !std::isspace(ch); }));
            trimmed.erase(
                std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                    .base(),
                trimmed.end());

            // Boolean detection
            if (trimmed == "true") {
                return true;
            }
            if (trimmed == "false") {
                return false;
            }

            // Integer detection
            if (std::regex_match(trimmed, std::regex(R"(-?\d+)"))) {
                return std::stoi(trimmed);
            }

            // Double detection
            if (std::regex_match(trimmed, std::regex(R"(-?\d+\.\d+)"))) {
                return std::stod(trimmed);
            }

            // Default to string
            return content;
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::warning("DataNode::parseContent - Exception parsing content as " + format + ": " +
                        std::string(e.what()) + ", defaulting to string");
        return content;
    }
}

DataNode::DataValue DataNode::evaluateExpression(::SCXML::Runtime::RuntimeContext &context) const {
    if (expr_.empty()) {
        return std::monostate{};
    }

    try {
        // Get data model engine for expression evaluation
        auto dataModel = context.getDataModelEngine();
        if (!dataModel) {
            SCXML::Common::Logger::error("DataNode::evaluateExpression - No data model engine available");
            return std::monostate{};
        }

        // Evaluate expression
        SCXML::Common::Logger::info("DataNode::evaluateExpression - About to evaluate: '" + expr_ + "'");
        auto result = dataModel->evaluateExpression(expr_, context);
        SCXML::Common::Logger::info("DataNode::evaluateExpression - Result success: " +
                     std::string(result.success ? "true" : "false"));
        if (!result.success) {
            SCXML::Common::Logger::error("DataNode::evaluateExpression - Expression evaluation failed: " + result.errorMessage);
            return std::monostate{};
        }

        // Convert result to DataValue
        std::string valueStr = dataModel->valueToString(result.value);

        // Try to parse the result based on its apparent type
        return parseContent(valueStr, "text");

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("DataNode::evaluateExpression - Exception evaluating expression: " + std::string(e.what()));
        return std::monostate{};
    }
}

}  // namespace Core
}  // namespace SCXML
