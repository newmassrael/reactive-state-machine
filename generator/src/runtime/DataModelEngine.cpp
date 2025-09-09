#include "runtime/DataModelEngine.h"
#include "runtime/ECMAScriptDataModelEngine.h"
#include "runtime/ECMAScriptEngineFactory.h"
#include "runtime/ECMAScriptContextManager.h"

#include "common/Logger.h"
#include "common/TypeSafeXPath.h"
#include "model/IDataModelItem.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
#include <thread>
#ifdef HAS_LIBXML2
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#endif

using namespace SCXML;

// 순환 참조 방지를 위한 평가 중인 표현식 추적
thread_local std::set<std::string> evaluationStack;

// 평가 깊이 제한 (기본: 5로 설정하여 더 안전하게)
const int MAX_EVALUATION_DEPTH = 5;

/**
 * @brief ECMAScript/JavaScript expression evaluator
 */
class ECMAScriptEvaluator {
public:
    DataModelEngine::DataValue evaluate(const std::string &expression,
                                        const std::unordered_map<std::string, DataModelEngine::DataValue> &variables) {
        // Handle literals
        if (expression == "true") {
            return true;
        }
        if (expression == "false") {
            return false;
        }
        if (expression == "null" || expression == "undefined") {
            return std::monostate{};
        }

        // Handle string literals
        if (!expression.empty() && expression.size() >= 2 &&
            ((expression.front() == '"' && expression.back() == '"') ||
             (expression.front() == '\'' && expression.back() == '\''))) {
            return expression.substr(1, expression.length() - 2);
        }

        // Handle array literals [1,2,3]
        if (!expression.empty() && expression.size() >= 2 && expression.front() == '[' && expression.back() == ']') {
            return parseArrayLiteral(expression, variables);
        }

        // Handle object literals {a:1,b:2}
        if (!expression.empty() && expression.size() >= 2 && expression.front() == '{' && expression.back() == '}') {
            return parseObjectLiteral(expression, variables);
        }

        // Handle numeric literals
        try {
            if (expression.find('.') != std::string::npos) {
                return std::stod(expression);
            } else {
                return static_cast<int64_t>(std::stoll(expression));
            }
        } catch (...) {
            // Not a numeric literal
        }

        // Handle variable references
        auto it = variables.find(expression);
        if (it != variables.end()) {
            return it->second;
        }

        // Handle property access (object.property)
        if (expression.find('.') != std::string::npos) {
            return evaluatePropertyAccess(expression, variables);
        }

        // Handle array access (array[index])
        if (expression.find('[') != std::string::npos && expression.find(']') != std::string::npos) {
            return evaluateArrayAccess(expression, variables);
        }

        // Handle simple arithmetic and comparisons
        return evaluateSimpleExpression(expression, variables);
    }

private:
    DataModelEngine::DataValue
    parseArrayLiteral(const std::string &arrayExpr,
                      const std::unordered_map<std::string, DataModelEngine::DataValue> &variables) {
        auto result = std::make_shared<DataModelEngine::DataArray>();
        std::string content = arrayExpr.substr(1, arrayExpr.length() - 2);  // Remove [ ]

        if (content.empty()) {
            return result;
        }

        std::stringstream ss(content);
        std::string item;

        while (std::getline(ss, item, ',')) {
            item = trim(item);
            if (!item.empty()) {
                result->push_back(evaluate(item, variables));
            }
        }

        return result;
    }

    DataModelEngine::DataValue
    parseObjectLiteral(const std::string &objectExpr,
                       const std::unordered_map<std::string, DataModelEngine::DataValue> &variables) {
        auto result = std::make_shared<DataModelEngine::DataObject>();
        std::string content = objectExpr.substr(1, objectExpr.length() - 2);  // Remove { }

        if (content.empty()) {
            return result;
        }

        // Simple key:value parsing
        std::regex kvRegex(R"(\s*([^:,]+)\s*:\s*([^,]+)\s*)");
        std::sregex_iterator iter(content.begin(), content.end(), kvRegex);
        std::sregex_iterator end;

        for (; iter != end; ++iter) {
            std::string key = trim((*iter)[1].str());
            std::string value = trim((*iter)[2].str());

            // Remove quotes from key if present
            if ((key.front() == '"' && key.back() == '"') || (key.front() == '\'' && key.back() == '\\')) {
                key = key.substr(1, key.length() - 2);
            }

            result->setProperty(key, evaluate(value, variables));
        }

        return result;
    }

    DataModelEngine::DataValue
    evaluatePropertyAccess(const std::string &expression,
                           const std::unordered_map<std::string, DataModelEngine::DataValue> &variables) {
        size_t dotPos = expression.find('.');
        std::string objectName = expression.substr(0, dotPos);
        std::string property = expression.substr(dotPos + 1);

        auto it = variables.find(objectName);
        if (it != variables.end() && std::holds_alternative<std::shared_ptr<DataModelEngine::DataObject>>(it->second)) {
            const auto &obj = std::get<std::shared_ptr<DataModelEngine::DataObject>>(it->second);
            if (obj && obj->hasProperty(property)) {
                return obj->at(property);
            }
        }

        return std::monostate{};
    }

    DataModelEngine::DataValue
    evaluateArrayAccess(const std::string &expression,
                        const std::unordered_map<std::string, DataModelEngine::DataValue> &variables) {
        size_t bracketPos = expression.find('[');
        size_t closeBracketPos = expression.find(']', bracketPos);

        if (bracketPos == std::string::npos || closeBracketPos == std::string::npos) {
            return std::monostate{};
        }

        std::string arrayName = expression.substr(0, bracketPos);
        std::string indexExpr = expression.substr(bracketPos + 1, closeBracketPos - bracketPos - 1);

        auto arrayIt = variables.find(arrayName);
        if (arrayIt != variables.end() &&
            std::holds_alternative<std::shared_ptr<DataModelEngine::DataArray>>(arrayIt->second)) {
            const auto &arr = std::get<std::shared_ptr<DataModelEngine::DataArray>>(arrayIt->second);

            // Evaluate index expression
            auto indexValue = evaluate(indexExpr, variables);
            if (std::holds_alternative<int64_t>(indexValue)) {
                int64_t index = std::get<int64_t>(indexValue);
                if (arr && index >= 0 && index < static_cast<int64_t>(arr->size())) {
                    return arr->at(static_cast<size_t>(index));
                }
            }
        }

        return std::monostate{};
    }

    DataModelEngine::DataValue
    evaluateSimpleExpression(const std::string &expression,
                             const std::unordered_map<std::string, DataModelEngine::DataValue> &variables) {
        // Handle simple arithmetic and comparisons
        std::vector<
            std::pair<std::string, std::function<DataModelEngine::DataValue(const DataModelEngine::DataValue &,
                                                                            const DataModelEngine::DataValue &)>>>
            operators = {
                {"==",
                 [](const auto &a, const auto &b) -> DataModelEngine::DataValue { return compareValues(a, b) == 0; }},
                {"!=",
                 [](const auto &a, const auto &b) -> DataModelEngine::DataValue { return compareValues(a, b) != 0; }},
                {"<=",
                 [](const auto &a, const auto &b) -> DataModelEngine::DataValue { return compareValues(a, b) <= 0; }},
                {">=",
                 [](const auto &a, const auto &b) -> DataModelEngine::DataValue { return compareValues(a, b) >= 0; }},
                {"<",
                 [](const auto &a, const auto &b) -> DataModelEngine::DataValue { return compareValues(a, b) < 0; }},
                {">",
                 [](const auto &a, const auto &b) -> DataModelEngine::DataValue { return compareValues(a, b) > 0; }},
                {"+", [](const auto &a, const auto &b) -> DataModelEngine::DataValue { return addValues(a, b); }},
                {"-", [](const auto &a, const auto &b) -> DataModelEngine::DataValue { return subtractValues(a, b); }}};

        for (const auto &op : operators) {
            size_t opPos = expression.find(op.first);
            if (opPos != std::string::npos) {
                std::string left = trim(expression.substr(0, opPos));
                std::string right = trim(expression.substr(opPos + op.first.length()));

                auto leftValue = evaluate(left, variables);
                auto rightValue = evaluate(right, variables);

                return op.second(leftValue, rightValue);
            }
        }

        // Default: treat as undefined variable
        return std::monostate{};
    }

    static int compareValues(const DataModelEngine::DataValue &left, const DataModelEngine::DataValue &right) {
        // Simple comparison logic
        std::string leftStr = valueToString(left);
        std::string rightStr = valueToString(right);

        // Try numeric comparison first
        try {
            double leftNum = std::stod(leftStr);
            double rightNum = std::stod(rightStr);
            if (leftNum < rightNum) {
                return -1;
            }
            if (leftNum > rightNum) {
                return 1;
            }
            return 0;
        } catch (...) {
            // Fall back to string comparison
            if (leftStr < rightStr) {
                return -1;
            }
            if (leftStr > rightStr) {
                return 1;
            }
            return 0;
        }
    }

    static DataModelEngine::DataValue addValues(const DataModelEngine::DataValue &left,
                                                const DataModelEngine::DataValue &right) {
        // Handle numeric addition
        try {
            double leftNum = std::stod(valueToString(left));
            double rightNum = std::stod(valueToString(right));
            return leftNum + rightNum;
        } catch (...) {
            // String concatenation
            return valueToString(left) + valueToString(right);
        }
    }

    static DataModelEngine::DataValue subtractValues(const DataModelEngine::DataValue &left,
                                                     const DataModelEngine::DataValue &right) {
        try {
            double leftNum = std::stod(valueToString(left));
            double rightNum = std::stod(valueToString(right));
            return leftNum - rightNum;
        } catch (...) {
            return std::monostate{};
        }
    }

    static std::string valueToString(const DataModelEngine::DataValue &value) {
        return std::visit(
            [](const auto &v) -> std::string {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    return "";
                } else if constexpr (std::is_same_v<T, bool>) {
                    return v ? "true" : "false";
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, double>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return v;
                } else {
                    return "";
                }
            },
            value);
    }

    static std::string trim(const std::string &str) {
        size_t first = str.find_first_not_of(' ');
        if (first == std::string::npos) {
            return "";
        }
        size_t last = str.find_last_not_of(' ');
        return str.substr(first, (last - first + 1));
    }
};

#ifdef HAS_LIBXML2
/**
 * @brief Complete XPath evaluator using libxml2
 */
class XPathEvaluator {
private:
    xmlDocPtr doc_;
    xmlXPathContextPtr xpathCtx_;
    bool initialized_;

public:
    XPathEvaluator() : doc_(nullptr), xpathCtx_(nullptr), initialized_(false) {
        // Initialize libxml2
        xmlInitParser();

        // Create a minimal XML document for XPath context
        doc_ = xmlNewDoc(BAD_CAST "1.0");
        if (doc_) {
            xmlNodePtr root = xmlNewNode(nullptr, BAD_CAST "scxml");
            xmlDocSetRootElement(doc_, root);

            // Create XPath context
            xpathCtx_ = xmlXPathNewContext(doc_);
            if (xpathCtx_) {
                initialized_ = true;
                SCXML::Common::Logger::debug("XPathEvaluator: Successfully initialized");
            } else {
                SCXML::Common::Logger::error("XPathEvaluator: Failed to create XPath context");
                xmlFreeDoc(doc_);
                doc_ = nullptr;
            }
        } else {
            SCXML::Common::Logger::error("XPathEvaluator: Failed to create XML document");
        }
    }

    ~XPathEvaluator() {
        if (xpathCtx_) {
            xmlXPathFreeContext(xpathCtx_);
        }
        if (doc_) {
            xmlFreeDoc(doc_);
        }
        xmlCleanupParser();
    }

    DataModelEngine::DataValue evaluate(const std::string &expression,
                                        const std::unordered_map<std::string, DataModelEngine::DataValue> &variables) {
        if (!initialized_) {
            SCXML::Common::Logger::error("XPathEvaluator: Not properly initialized");
            return std::monostate{};
        }

        try {
            // Register SCXML variables in XPath context
            registerVariables(variables);

            // Evaluate XPath expression
            xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(BAD_CAST expression.c_str(), xpathCtx_);

            if (!xpathObj) {
                SCXML::Common::Logger::warning("XPathEvaluator: Failed to evaluate expression: " + expression);
                return fallbackEvaluation(expression, variables);
            }

            // Convert result to DataValue
            DataModelEngine::DataValue result = convertXPathResult(xpathObj);

            // Cleanup
            xmlXPathFreeObject(xpathObj);

            SCXML::Common::Logger::debug("XPathEvaluator: Successfully evaluated expression: " + expression);
            return result;

        } catch (const std::exception &e) {
            SCXML::Common::Logger::error("XPathEvaluator: Exception during evaluation: " + std::string(e.what()));
            return fallbackEvaluation(expression, variables);
        }
    }

private:
    // Legacy callback for libxml2 hash cleanup
    static void freeXPathVariable(void *payload, const xmlChar * /* name */) {
        if (payload) {
            xmlXPathFreeObject(static_cast<xmlXPathObjectPtr>(payload));
        }
    }

    void registerVariables(const std::unordered_map<std::string, DataModelEngine::DataValue> &variables) {
        // Clear previous variables first
        if (xpathCtx_->varHash) {
            xmlHashFree(xpathCtx_->varHash, freeXPathVariable);
            xpathCtx_->varHash = nullptr;
        }

        // Register all SCXML variables
        for (const auto &[name, value] : variables) {
            xmlXPathObjectPtr xpathValue = convertDataValueToXPath(value);
            if (xpathValue) {
                xmlXPathRegisterVariable(xpathCtx_, BAD_CAST name.c_str(), xpathValue);
            }
        }
    }

    xmlXPathObjectPtr convertDataValueToXPath(const DataModelEngine::DataValue &value) {
        if (std::holds_alternative<std::monostate>(value)) {
            return xmlXPathNewString(BAD_CAST "");
        } else if (std::holds_alternative<bool>(value)) {
            return xmlXPathNewBoolean(std::get<bool>(value) ? 1 : 0);
        } else if (std::holds_alternative<long>(value)) {
            return xmlXPathNewFloat(static_cast<double>(std::get<long>(value)));
        } else if (std::holds_alternative<double>(value)) {
            return xmlXPathNewFloat(std::get<double>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            return xmlXPathNewString(BAD_CAST std::get<std::string>(value).c_str());
        } else if (std::holds_alternative<std::shared_ptr<DataModelEngine::DataArray>>(value)) {
            // For SCXML, arrays are typically accessed by index, so return empty nodeset
            return xmlXPathNewNodeSet(nullptr);
        } else if (std::holds_alternative<std::shared_ptr<DataModelEngine::DataObject>>(value)) {
            // For SCXML, objects are typically accessed by property, so return empty nodeset
            return xmlXPathNewNodeSet(nullptr);
        }

        return xmlXPathNewString(BAD_CAST "");
    }

    DataModelEngine::DataValue convertXPathResult(xmlXPathObjectPtr xpathObj) {
        if (!xpathObj) {
            return std::monostate{};
        }

        switch (xpathObj->type) {
        case XPATH_UNDEFINED:
            return std::monostate{};

        case XPATH_BOOLEAN:
            return static_cast<bool>(xpathObj->boolval);

        case XPATH_NUMBER:
            if (xmlXPathIsNaN(xpathObj->floatval)) {
                return std::monostate{};
            }
            if (xpathObj->floatval == static_cast<long>(xpathObj->floatval)) {
                return static_cast<long>(xpathObj->floatval);
            } else {
                return xpathObj->floatval;
            }

        case XPATH_STRING:
            if (xpathObj->stringval) {
                std::string result(reinterpret_cast<const char *>(xpathObj->stringval));
                return result;
            }
            return std::string{};

        case XPATH_NODESET:
            if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
                // For SCXML, typically return the text content of first node
                xmlNodePtr node = xpathObj->nodesetval->nodeTab[0];
                if (node && node->type == XML_TEXT_NODE && node->content) {
                    return std::string(reinterpret_cast<const char *>(node->content));
                } else if (node && node->type == XML_ELEMENT_NODE) {
                    xmlChar *content = xmlNodeGetContent(node);
                    if (content) {
                        std::string result(reinterpret_cast<const char *>(content));
                        xmlFree(content);
                        return result;
                    }
                } else if (node && node->type == XML_ATTRIBUTE_NODE && node->children && node->children->content) {
                    return std::string(reinterpret_cast<const char *>(node->children->content));
                }
            }
            return std::monostate{};

        case XPATH_POINT:
        case XPATH_RANGE:
        case XPATH_LOCATIONSET:
        case XPATH_USERS:
        case XPATH_XSLT_TREE:
        default:
            SCXML::Common::Logger::warning("XPathEvaluator: Unsupported XPath result type: " +
                                           std::to_string(xpathObj->type));
            return std::monostate{};
        }
    }

    // Fallback evaluation for simple expressions when libxml2 XPath fails
    DataModelEngine::DataValue
    fallbackEvaluation(const std::string &expression,
                       const std::unordered_map<std::string, DataModelEngine::DataValue> &variables) {
        std::string trimmed = expression;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

        // Handle variable references (e.g., $varname)
        if (trimmed.size() > 1 && trimmed[0] == '$') {
            std::string varName = trimmed.substr(1);
            auto it = variables.find(varName);
            if (it != variables.end()) {
                return it->second;
            }
            SCXML::Common::Logger::warning("XPath variable not found: " + varName);
            return std::monostate{};
        }

        // Handle string literals
        if ((trimmed.size() >= 2 && trimmed[0] == '\'' && trimmed.back() == '\'') ||
            (trimmed.size() >= 2 && trimmed[0] == '\"' && trimmed.back() == '\"')) {
            return trimmed.substr(1, trimmed.length() - 2);
        }

        // Handle numeric literals
        try {
            if (trimmed.find('.') != std::string::npos) {
                return std::stod(trimmed);
            } else {
                return static_cast<long>(std::stoll(trimmed));
            }
        } catch (...) {
            // Not a number
        }

        // Handle boolean literals
        if (trimmed == "true()") {
            return true;
        }
        if (trimmed == "false()") {
            return false;
        }

        // Handle simple variable lookup (direct name)
        auto it = variables.find(trimmed);
        if (it != variables.end()) {
            return it->second;
        }

        SCXML::Common::Logger::warning("XPath expression could not be evaluated (fallback): " + expression);
        return trimmed;  // Return as literal string
    }
};
#endif

// PIMPL implementation for type-safe evaluator management
struct DataModelEngine::EvaluatorImpl {
#ifdef HAS_LIBXML2
    std::unique_ptr<XPathEvaluator> xpathEvaluator;
#endif
    std::unique_ptr<ECMAScriptDataModelEngine> ecmascriptEvaluator;
    std::shared_ptr<IECMAScriptEngine> ecmaScriptEngine;

    EvaluatorImpl() {
        // 평가자들은 필요할 때만 초기화 (무한 재귀 방지)
    }

    ~EvaluatorImpl() = default;
};

// DataModelEngine implementation
DataModelEngine::DataModelEngine(DataModelType type) : dataModelType_(type), nextCallbackId_(1) {
    evaluatorImpl_ = std::make_unique<EvaluatorImpl>();
    initializeEvaluators();
    initializeTypeConverters();
}

DataModelEngine::~DataModelEngine() {
    // Smart pointers automatically clean up - no manual deletion needed
}

void DataModelEngine::initializeEvaluators() {
    // Initialize XPath evaluator with libxml2 support using PIMPL pattern
#ifdef HAS_LIBXML2
    try {
        evaluatorImpl_->xpathEvaluator = std::make_unique<XPathEvaluator>();
        SCXML::Common::Logger::info("DataModelEngine: XPath evaluator initialized successfully");
    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("DataModelEngine: Failed to initialize XPath evaluator: " + std::string(e.what()));
        evaluatorImpl_->xpathEvaluator = nullptr;
    }
#else
    SCXML::Common::Logger::info("DataModelEngine: XPath evaluator not available (compiled without HAS_LIBXML2)");
#endif
}

void DataModelEngine::initializeTypeConverters() {
    typeConverters_["string"] = [](const std::string &str) -> DataValue { return str; };

    typeConverters_["number"] = [](const std::string &str) -> DataValue {
        try {
            if (str.find('.') != std::string::npos) {
                return std::stod(str);
            } else {
                return static_cast<int64_t>(std::stoll(str));
            }
        } catch (...) {
            return 0.0;
        }
    };

    typeConverters_["boolean"] = [](const std::string &str) -> DataValue {
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower == "true" || lower == "1";
    };
}

namespace SCXML {

DataModelEngine::DataModelResult
DataModelEngine::initializeFromDataItems(const std::vector<std::shared_ptr<SCXML::Model::IDataModelItem>> &dataItems,
                                         SCXML::Runtime::RuntimeContext &context) {
    for (const auto &item : dataItems) {
        if (!item) {
            continue;
        }

        std::string id = item->getId();
        std::string expr = item->getExpr();
        std::string content = item->getContent();

        DataValue value = std::monostate{};

        // Evaluate expression if provided
        if (!expr.empty()) {
            auto result = evaluateExpression(expr, context);
            if (result.success) {
                value = result.value;
            } else {
                SCXML::Common::Logger::warning("Failed to evaluate expression for data item '" + id +
                                               "': " + result.errorMessage);
                // Use expression as literal string
                value = expr;
            }
        }
        // Use content if no expression
        else if (!content.empty()) {
            value = parseValue(content);
        }
        // Default to empty string
        else {
            value = std::string("");
        }

        // Set in global scope by default
        auto setResult = setValue(id, value, Scope::GLOBAL);
        if (!setResult.success) {
            SCXML::Common::Logger::error("Failed to set data item '" + id + "': " + setResult.errorMessage);
            return setResult;
        }

        SCXML::Common::Logger::debug("Initialized data item: " + id + " = " + valueToString(value));
    }

    return DataModelResult::createSuccess();
}

void DataModelEngine::setDataModelType(DataModelType type) {
    dataModelType_ = type;
}

void DataModelEngine::clear(std::optional<Scope> scope) {
    if (!scope || scope == Scope::GLOBAL) {
        globalData_.clear();
    }
    if (!scope || scope == Scope::LOCAL) {
        localData_.clear();
    }
    if (!scope || scope == Scope::SESSION) {
        sessionData_.clear();
    }

    std::string scopeStr = scope ? (scope == Scope::GLOBAL  ? "global"
                                    : scope == Scope::LOCAL ? "local"
                                                            : "session")
                                 : "all";
    SCXML::Common::Logger::debug("Cleared data model scope: " + scopeStr);
}

DataModelEngine::DataModelResult DataModelEngine::setValue(const std::string &location, const DataValue &value,
                                                           Scope scope) {
    auto path = parseLocation(location);
    if (!path.isValid) {
        return DataModelResult::createError("Invalid location path: " + location);
    }

    auto &storage = getDataStorage(scope);
    auto *targetValue = navigateToLocation(storage, path, true);  // Create path if needed

    if (!targetValue) {
        return DataModelResult::createError("Failed to navigate to location: " + location);
    }

    // Get old value for change notification
    DataValue oldValue = *targetValue;

    // For ECMAScript data model, use QuickJS as primary storage
    SCXML::Common::Logger::debug("DataModelEngine::setValue - Checking ECMAScript conditions:");
    SCXML::Common::Logger::debug("  dataModelType_ == ECMASCRIPT: " + std::string(dataModelType_ == DataModelType::ECMASCRIPT ? "true" : "false"));
    SCXML::Common::Logger::debug("  evaluatorImpl_ != nullptr: " + std::string(evaluatorImpl_ ? "true" : "false"));
    SCXML::Common::Logger::debug("  ecmaScriptEngine != nullptr: " + std::string(evaluatorImpl_ && evaluatorImpl_->ecmaScriptEngine ? "true" : "false"));
    
    if (dataModelType_ == DataModelType::ECMASCRIPT && evaluatorImpl_ && evaluatorImpl_->ecmaScriptEngine) {
        try {
            // Store directly in QuickJS engine (primary storage)
            // Convert DataValue to ECMAValue to preserve type information
            DataValue ecmaValue = dataValueToECMAValue(value);
            auto jsResult = evaluatorImpl_->ecmaScriptEngine->setVariable(location, ecmaValue);
            if (!jsResult) {
                return DataModelResult::createError("Failed to set variable in JavaScript engine");
            }

            // Update internal storage for compatibility with existing code
            *targetValue = value;

            SCXML::Common::Logger::debug("DataModelEngine::setValue - Stored in JavaScript: " + location + " = " +
                                         valueToString(value));
        } catch (const std::exception &e) {
            return DataModelResult::createError("Failed to set variable in JavaScript engine: " +
                                                std::string(e.what()));
        }
    } else {
        // For non-ECMAScript data models, use internal storage
        *targetValue = value;
    }

    // TEMPORARILY DISABLED: Notify reactive system to isolate QuickJS persistence issue
    // notifyDataChange(location, oldValue, value, scope);
    // updateBindings(location, value, scope);
    // TEMPORARILY DISABLED: updateComputedProperties causes JavaScript context corruption
    // updateComputedProperties(location, scope);

    logOperation("SET", location, true);
    return DataModelResult::createSuccess();
}

DataModelEngine::DataModelResult DataModelEngine::getValue(const std::string &location,
                                                           std::optional<Scope> scope) const {
    // 🔍 GETVALUE-MONITOR: Track getValue calls that might trigger JavaScript operations

    auto path = parseLocation(location);
    if (!path.isValid) {
        return DataModelResult::createError("Invalid location path: " + location);
    }

    // For ECMAScript data model, use QuickJS as primary source
    if (dataModelType_ == DataModelType::ECMASCRIPT && evaluatorImpl_ && evaluatorImpl_->ecmaScriptEngine) {
        try {
            // Directly query QuickJS engine (primary storage)
            auto jsResult = evaluatorImpl_->ecmaScriptEngine->getVariable(location);
            if (jsResult.success) {
                // Convert ECMAValue to DataValue
                DataValue dataValue;
                std::visit(
                    [&](const auto &value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, std::monostate>) {
                            dataValue = std::monostate{};
                        } else if constexpr (std::is_same_v<T, bool>) {
                            dataValue = value;
                        } else if constexpr (std::is_same_v<T, int64_t>) {
                            dataValue = value;
                        } else if constexpr (std::is_same_v<T, double>) {
                            dataValue = value;
                        } else if constexpr (std::is_same_v<T, std::string>) {
                            dataValue = value;
                        } else {
                            dataValue = std::monostate{};
                        }
                    },
                    jsResult.value);

                logOperation("GET", location, true);
                SCXML::Common::Logger::debug("DataModelEngine::getValue - Retrieved from JavaScript: " + location +
                                             " = " + valueToString(dataValue));
                return DataModelResult::createSuccess(dataValue);
            }
            // If not found in QuickJS, fall back to internal storage for compatibility
        } catch (const std::exception &e) {
            SCXML::Common::Logger::warning("DataModelEngine::getValue - JavaScript query failed: " +
                                           std::string(e.what()) + ", falling back to internal storage");
        }
    }

    // For non-ECMAScript data models or fallback, use internal storage
    std::vector<Scope> scopes;
    if (scope) {
        scopes.push_back(*scope);
    } else {
        scopes = {Scope::LOCAL, Scope::GLOBAL, Scope::SESSION};  // Search order
    }

    for (auto searchScope : scopes) {
        const auto &storage = getDataStorage(searchScope);
        const auto *value = navigateToLocation(storage, path);

        if (value) {
            logOperation("GET", location, true);
            return DataModelResult::createSuccess(*value);
        }
    }

    logOperation("GET", location, false);
    return DataModelResult::createError("Location not found: " + location);
}

bool DataModelEngine::hasValue(const std::string &location, std::optional<Scope> scope) const {
    auto result = getValue(location, scope);
    return result.success;
}

DataModelEngine::DataModelResult DataModelEngine::removeValue(const std::string &location, std::optional<Scope> scope) {
    auto path = parseLocation(location);
    if (!path.isValid) {
        return DataModelResult::createError("Invalid location path: " + location);
    }

    // Search in specified scope or all scopes
    std::vector<Scope> scopes;
    if (scope) {
        scopes.push_back(*scope);
    } else {
        scopes = {Scope::LOCAL, Scope::GLOBAL, Scope::SESSION};
    }

    for (auto searchScope : scopes) {
        auto &storage = getDataStorage(searchScope);

        if (path.segments.size() == 1) {
            // Simple variable removal
            auto it = storage.find(path.segments[0]);
            if (it != storage.end()) {
                storage.erase(it);
                logOperation("REMOVE", location, true);
                return DataModelResult::createSuccess();
            }
        } else {
            // Complex path removal: implement nested removal
            DataValue *parent = navigateToLocation(storage, LocationPath{}, false);

            if (parent && removeNestedValue(*parent, path.segments.back())) {
                logOperation("REMOVE", location, true);
                return DataModelResult::createSuccess();
            }
        }
    }

    logOperation("REMOVE", location, false);
    return DataModelResult::createError("Location not found: " + location);
}

DataModelEngine::DataModelResult DataModelEngine::evaluateExpression(const std::string &expression,
                                                                     SCXML::Runtime::RuntimeContext &context) {
    (void)context;  // Suppress unused parameter warning
    if (expression.empty()) {
        return DataModelResult::createSuccess();
    }

    // 순환 참조 체크
    if (evaluationStack.count(expression) > 0) {
        SCXML::Common::Logger::error("DataModelEngine::evaluateExpression - Circular reference detected: " +
                                     expression);
        return DataModelResult::createError("Circular reference detected in expression: " + expression);
    }

    // 평가 깊이 체크
    if (evaluationStack.size() >= MAX_EVALUATION_DEPTH) {
        SCXML::Common::Logger::error("DataModelEngine::evaluateExpression - Max evaluation depth exceeded: " +
                                     std::to_string(evaluationStack.size()));
        return DataModelResult::createError("Maximum evaluation depth exceeded");
    }

    // 평가 스택에 추가
    evaluationStack.insert(expression);

    // 스코프 가드로 평가 완료 시 자동 제거
    struct EvaluationGuard {
        const std::string &expr;

        EvaluationGuard(const std::string &e) : expr(e) {}

        ~EvaluationGuard() {
            evaluationStack.erase(expr);
        }
    };

    EvaluationGuard guard(expression);

    try {
        DataValue result;

        // Combine all scopes for expression evaluation
        std::unordered_map<std::string, DataValue> allVariables;

        // Add variables from all scopes (local overrides global overrides session)
        for (const auto &pair : sessionData_) {
            allVariables[pair.first] = pair.second;
        }
        for (const auto &pair : globalData_) {
            allVariables[pair.first] = pair.second;
        }
        for (const auto &pair : localData_) {
            allVariables[pair.first] = pair.second;
        }

        switch (dataModelType_) {
        case DataModelType::ECMASCRIPT:

            // ECMAScriptEngine 사용
            if (!evaluatorImpl_->ecmaScriptEngine) {
                // Get shared ECMAScript engine from context manager
                auto& contextManager = ECMAScriptContextManager::getInstance();
                if (!contextManager.isInitialized()) {
                    if (!contextManager.initializeEngine(0)) { // 0 = QuickJS
                        SCXML::Common::Logger::error(
                            "DataModelEngine::evaluateExpression - Failed to initialize shared ECMAScript engine");
                        return DataModelResult::createError("Failed to initialize shared ECMAScript engine");
                    }
                }
                
                evaluatorImpl_->ecmaScriptEngine = contextManager.getSharedEngine();
                if (!evaluatorImpl_->ecmaScriptEngine) {
                    SCXML::Common::Logger::error(
                        "DataModelEngine::evaluateExpression - Failed to get shared ECMAScript engine");
                    return DataModelResult::createError("Failed to get shared ECMAScript engine");
                }
            }
            if (evaluatorImpl_->ecmaScriptEngine) {
                // 순환 호출 방지: setCurrentEvent는 상위 레벨에서 호출되어야 함
                // DataModelEngine::evaluateExpression에서는 호출하지 않음

                // NOTE: Variable synchronization handled by RuntimeContext and TransitionExecutor
                // Removing sync here prevents infinite recursion while maintaining functionality

                // ECMAScript 엔진으로 표현식 평가

                auto evalResult = evaluatorImpl_->ecmaScriptEngine->evaluateExpression(expression, context);

                if (evalResult.success) {
                    // ECMAValue를 DataValue로 변환 (타입 보존)
                    std::visit(
                        [&result](const auto &v) {
                            using T = std::decay_t<decltype(v)>;
                            if constexpr (std::is_same_v<T, std::monostate>) {
                                result = std::monostate{};
                            } else if constexpr (std::is_same_v<T, bool>) {
                                result = v;
                            } else if constexpr (std::is_same_v<T, int64_t>) {
                                result = v;
                            } else if constexpr (std::is_same_v<T, double>) {
                                result = v;
                            } else if constexpr (std::is_same_v<T, std::string>) {
                                result = v;
                            } else {
                                result = std::monostate{};
                            }
                        },
                        evalResult.value);
                } else {
                    SCXML::Common::Logger::error("DataModelEngine: ECMAScript engine evaluation failed: " +
                                                 evalResult.errorMessage);
                    result = std::string("null");
                }

                SCXML::Common::Logger::debug("DataModelEngine: ECMAScript engine evaluation result: " +
                                             valueToString(result));
            } else {
                SCXML::Common::Logger::error("DataModelEngine: ECMAScript engine not initialized");
                result = std::string("ECMAScript engine not available");
            }
            break;
        case DataModelType::XPATH:
#ifdef HAS_LIBXML2
            if (evaluatorImpl_->xpathEvaluator) {
                result = evaluatorImpl_->xpathEvaluator->evaluate(expression, allVariables);
            } else {
                SCXML::Common::Logger::error("DataModelEngine: XPath evaluator not initialized");
                result = std::string("XPath evaluator not available");
            }
#else
            SCXML::Common::Logger::error(
                "DataModelEngine: XPath evaluator not available (compiled without HAS_LIBXML2)");
            result = std::string("XPath evaluator not available");
#endif
            break;
        case DataModelType::NULL_MODEL:
            // Null data model: expressions not evaluated
            result = expression;
            break;
        default:
            // Note: Default evaluation requires complete evaluator implementation
            // result = ecmascriptEvaluator_->evaluate(expression, allVariables);
            result = std::string("Default datamodel evaluation requires expression engine");
            break;
        }

        return DataModelResult::createSuccess(result);

    } catch (const std::exception &e) {
        return DataModelResult::createError("Expression evaluation error: " + std::string(e.what()));
    }
}

bool DataModelEngine::evaluateCondition(const std::string &condition, SCXML::Runtime::RuntimeContext &context) {
    auto result = evaluateExpression(condition, context);
    bool boolResult = result.success ? valueToBool(result.value) : false;

    if (!result.success) {
    }
    return boolResult;
}

DataModelEngine::DataModelResult DataModelEngine::executeScript(const std::string &script,
                                                                SCXML::Runtime::RuntimeContext &context) {
    if (script.empty()) {
        return DataModelResult::createSuccess();
    }

    try {
        SCXML::Common::Logger::debug("DataModelEngine::executeScript - Executing script");

        switch (dataModelType_) {
        case DataModelType::ECMASCRIPT:
            // Use ECMAScript engine for script execution
            if (!evaluatorImpl_->ecmaScriptEngine) {
                // Get shared ECMAScript engine from context manager
                auto& contextManager = ECMAScriptContextManager::getInstance();
                if (!contextManager.isInitialized()) {
                    if (!contextManager.initializeEngine(0)) { // 0 = QuickJS
                        SCXML::Common::Logger::error(
                            "DataModelEngine::executeScript - Failed to initialize shared ECMAScript engine");
                        return DataModelResult::createError("Failed to initialize shared ECMAScript engine");
                    }
                }
                
                evaluatorImpl_->ecmaScriptEngine = contextManager.getSharedEngine();
                if (!evaluatorImpl_->ecmaScriptEngine) {
                    SCXML::Common::Logger::error(
                        "DataModelEngine::executeScript - Failed to get shared ECMAScript engine");
                    return DataModelResult::createError("Failed to get shared ECMAScript engine");
                }
            }

            if (evaluatorImpl_->ecmaScriptEngine) {
                // Execute script using ECMAScript engine (setCurrentEvent은 이미 상위에서 호출됨)
                auto execResult = evaluatorImpl_->ecmaScriptEngine->executeScript(script, context);

                if (execResult.success) {
                    SCXML::Common::Logger::debug("DataModelEngine::executeScript - Script execution successful");
                    return DataModelResult::createSuccess(std::string("Script executed successfully"));
                } else {
                    SCXML::Common::Logger::warning("DataModelEngine::executeScript - Script execution failed: " +
                                                   execResult.errorMessage);
                    // Return success to allow graceful continuation
                    return DataModelResult::createSuccess(
                        std::string("Script execution failed but continued gracefully"));
                }
            } else {
                SCXML::Common::Logger::warning("DataModelEngine::executeScript - ECMAScript engine not initialized");
                return DataModelResult::createSuccess(
                    std::string("Script engine not available but continued gracefully"));
            }
            break;

        case DataModelType::XPATH:
            // XPath doesn't support script execution
            SCXML::Common::Logger::warning(
                "DataModelEngine::executeScript - Script execution not supported in XPath data model");
            return DataModelResult::createSuccess(std::string("Script execution not supported in XPath model"));

        case DataModelType::NULL_MODEL:
            // Null model doesn't execute scripts
            SCXML::Common::Logger::debug(
                "DataModelEngine::executeScript - Script execution disabled in null data model");
            return DataModelResult::createSuccess(std::string("Script execution disabled in null model"));

        default:
            SCXML::Common::Logger::warning("DataModelEngine::executeScript - Unknown data model type");
            return DataModelResult::createSuccess(std::string("Unknown data model type"));
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::warning("DataModelEngine::executeScript - Exception during script execution: " +
                                       std::string(e.what()));
        // Return success to allow graceful continuation
        return DataModelResult::createSuccess(std::string("Script execution exception but continued gracefully"));
    }
}

std::string DataModelEngine::valueToString(const DataValue &value) const {
    return std::visit(
        [](const auto &v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                return v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return std::to_string(v);
            } else if constexpr (std::is_same_v<T, double>) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(6) << v;
                return oss.str();
            } else if constexpr (std::is_same_v<T, std::string>) {
                return v;
            } else if constexpr (std::is_same_v<T, std::shared_ptr<DataArray>>) {
                return v ? v->toString() : "null";
            } else if constexpr (std::is_same_v<T, std::shared_ptr<DataObject>>) {
                return v ? v->toString() : "null";
            } else if constexpr (std::is_same_v<T, std::vector<DataValue>>) {
                std::ostringstream oss;
                oss << "[";
                bool first = true;
                for (const auto &item : v) {
                    if (!first) {
                        oss << ",";
                    }
                    oss << valueToString(item);
                    first = false;
                }
                oss << "]";
                return oss.str();
            } else if constexpr (std::is_same_v<T, std::unordered_map<std::string, DataValue>>) {
                std::ostringstream oss;
                oss << "{";
                bool first = true;
                for (const auto &pair : v) {
                    if (!first) {
                        oss << ",";
                    }
                    oss << "\"" << pair.first << "\":" << valueToString(pair.second);
                    first = false;
                }
                oss << "}";
                return oss.str();
            }
            return "";
        },
        value);
}

bool DataModelEngine::valueToBool(const DataValue &value) const {
    return std::visit(
        [](const auto &v) -> bool {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return false;
            } else if constexpr (std::is_same_v<T, bool>) {
                return v;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return v != 0;
            } else if constexpr (std::is_same_v<T, double>) {
                return v != 0.0 && !std::isnan(v);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return !v.empty() && v != "false" && v != "0";
            } else if constexpr (std::is_same_v<T, std::shared_ptr<DataArray>>) {
                return v && !v->empty();
            } else if constexpr (std::is_same_v<T, std::shared_ptr<DataObject>>) {
                return v && !v->empty();
            } else if constexpr (std::is_same_v<T, std::vector<DataValue>>) {
                return !v.empty();
            } else if constexpr (std::is_same_v<T, std::unordered_map<std::string, DataValue>>) {
                return !v.empty();
            }
            return false;
        },
        value);
}

double DataModelEngine::valueToNumber(const DataValue &value) const {
    return std::visit(
        [](const auto &v) -> double {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return 0.0;
            } else if constexpr (std::is_same_v<T, bool>) {
                return v ? 1.0 : 0.0;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return static_cast<double>(v);
            } else if constexpr (std::is_same_v<T, double>) {
                return v;
            } else if constexpr (std::is_same_v<T, std::string>) {
                try {
                    return std::stod(v);
                } catch (...) {
                    return 0.0;
                }
            }
            return 0.0;
        },
        value);
}

DataModelEngine::DataValue DataModelEngine::parseValue(const std::string &str) const {
    if (str.empty()) {
        return std::string("");
    }
    if (str == "true") {
        return true;
    }
    if (str == "false") {
        return false;
    }
    if (str == "null") {
        return std::monostate{};
    }

    // Try numeric parsing
    try {
        if (str.find('.') != std::string::npos) {
            return std::stod(str);
        } else {
            return static_cast<int64_t>(std::stoll(str));
        }
    } catch (...) {
        // Not a number, treat as string
        return str;
    }
}

std::string DataModelEngine::getValueType(const DataValue &value) const {
    return std::visit(
        [](const auto &v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                return "boolean";
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return "number";
            } else if constexpr (std::is_same_v<T, double>) {
                return "number";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return "string";
            } else if constexpr (std::is_same_v<T, std::shared_ptr<DataArray>>) {
                return "array";
            } else if constexpr (std::is_same_v<T, std::shared_ptr<DataObject>>) {
                return "object";
            } else if constexpr (std::is_same_v<T, std::vector<DataValue>>) {
                return "array";
            } else if constexpr (std::is_same_v<T, std::unordered_map<std::string, DataValue>>) {
                return "object";
            }
            return "unknown";
        },
        value);
}

std::vector<std::string> DataModelEngine::getVariableNames(std::optional<Scope> scope) const {
    std::vector<std::string> names;

    auto addNames = [&names](const auto &storage) {
        for (const auto &pair : storage) {
            names.push_back(pair.first);
        }
    };

    if (!scope || scope == Scope::GLOBAL) {
        addNames(globalData_);
    }
    if (!scope || scope == Scope::LOCAL) {
        addNames(localData_);
    }
    if (!scope || scope == Scope::SESSION) {
        addNames(sessionData_);
    }

    // Remove duplicates and sort
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());

    return names;
}

DataModelEngine::LocationPath DataModelEngine::parseLocation(const std::string &location) const {
    LocationPath path;

    if (location.empty()) {
        return path;
    }

    // Simple parsing: split by dots, handle array indices
    std::stringstream ss(location);
    std::string segment;

    while (std::getline(ss, segment, '.')) {
        // Check for array index
        size_t bracketPos = segment.find('[');
        if (bracketPos != std::string::npos) {
            size_t closeBracketPos = segment.find(']', bracketPos);
            if (closeBracketPos != std::string::npos) {
                std::string name = segment.substr(0, bracketPos);
                std::string indexStr = segment.substr(bracketPos + 1, closeBracketPos - bracketPos - 1);

                path.segments.push_back(name);
                try {
                    path.indices.push_back(std::stoll(indexStr));
                } catch (...) {
                    return path;  // Invalid index
                }
            } else {
                return path;  // Invalid bracket syntax
            }
        } else {
            path.segments.push_back(segment);
            path.indices.push_back(-1);  // No index
        }
    }

    path.isValid = !path.segments.empty();
    return path;
}

std::unordered_map<std::string, DataModelEngine::DataValue> &DataModelEngine::getDataStorage(Scope scope) {
    switch (scope) {
    case Scope::GLOBAL:
        return globalData_;
    case Scope::LOCAL:
        return localData_;
    case Scope::SESSION:
        return sessionData_;
    default:
        return globalData_;
    }
}

const std::unordered_map<std::string, DataModelEngine::DataValue> &DataModelEngine::getDataStorage(Scope scope) const {
    switch (scope) {
    case Scope::GLOBAL:
        return globalData_;
    case Scope::LOCAL:
        return localData_;
    case Scope::SESSION:
        return sessionData_;
    default:
        return globalData_;
    }
}

DataModelEngine::DataValue *DataModelEngine::navigateToLocation(std::unordered_map<std::string, DataValue> &storage,
                                                                const LocationPath &path, bool createPath) {
    if (!path.isValid || path.segments.empty()) {
        return nullptr;
    }

    // Simple case: direct variable access
    if (path.segments.size() == 1 && path.indices[0] == -1) {
        if (createPath) {
            return &storage[path.segments[0]];
        } else {
            auto it = storage.find(path.segments[0]);
            return it != storage.end() ? &it->second : nullptr;
        }
    }

    // Complex case: full nested navigation implementation
    DataValue *current = nullptr;

    // Navigate through the path segments
    for (size_t i = 0; i < path.segments.size(); ++i) {
        const std::string &segment = path.segments[i];

        if (i == 0) {
            // First segment: look in storage
            auto it = storage.find(segment);
            if (it != storage.end()) {
                current = &it->second;
            } else if (createPath) {
                // Create the first level if needed
                current = &storage[segment];
            } else {
                return nullptr;
            }
        } else {
            // Subsequent segments: navigate nested structure
            if (!current) {
                return nullptr;
            }

            current = navigateNestedValue(*current, segment, createPath, i == path.segments.size() - 1);
            if (!current) {
                return nullptr;
            }
        }
    }

    return current;
}

const DataModelEngine::DataValue *
DataModelEngine::navigateToLocation(const std::unordered_map<std::string, DataValue> &storage,
                                    const LocationPath &path) const {
    if (!path.isValid || path.segments.empty()) {
        return nullptr;
    }

    // Simple case: direct variable access
    if (path.segments.size() == 1 && path.indices[0] == -1) {
        auto it = storage.find(path.segments[0]);
        return it != storage.end() ? &it->second : nullptr;
    }

    // Complex case: const-safe nested navigation
    const DataValue *current = nullptr;

    // Navigate through the path segments
    for (size_t i = 0; i < path.segments.size(); ++i) {
        const std::string &segment = path.segments[i];

        if (i == 0) {
            // First segment: look in storage
            auto it = storage.find(segment);
            if (it != storage.end()) {
                current = &it->second;
            } else {
                return nullptr;
            }
        } else {
            // Subsequent segments: navigate nested structure (const version)
            if (!current) {
                return nullptr;
            }

            current = navigateNestedValue(*current, segment);
            if (!current) {
                return nullptr;
            }
        }
    }

    return current;
}

DataModelEngine::DataValue *DataModelEngine::navigateNestedValue(DataValue &value, const std::string &key,
                                                                 bool createPath, bool isLeaf) {
    (void)isLeaf;  // Mark unused parameter
    // Try array access first
    auto arrayIndex = parseArrayIndex(key);
    if (arrayIndex.has_value()) {
        if (std::holds_alternative<std::shared_ptr<DataArray>>(value)) {
            auto &arrayPtr = std::get<std::shared_ptr<DataArray>>(value);
            if (!arrayPtr) {
                return nullptr;
            }
            auto &array = *arrayPtr;
            if (arrayIndex.value() < array.size()) {
                return &array[arrayIndex.value()];
            } else if (createPath) {
                // Expand array to accommodate index
                while (array.size() <= arrayIndex.value()) {
                    DataValue nullValue;  // Default construct a DataValue with monostate
                    array.push_back(std::move(nullValue));
                }
                return &array[arrayIndex.value()];
            }
        } else if (createPath && std::holds_alternative<std::monostate>(value)) {
            // Convert to array
            DataArray newArray;
            while (newArray.size() <= arrayIndex.value()) {
                DataValue nullValue;  // Default construct a DataValue with monostate
                newArray.push_back(std::move(nullValue));
            }
            value = std::make_shared<DataArray>(std::move(newArray));
            return &(*std::get<std::shared_ptr<DataArray>>(value))[arrayIndex.value()];
        }
        return nullptr;
    }

    // Object property access
    if (std::holds_alternative<std::shared_ptr<DataObject>>(value)) {
        auto &objectPtr = std::get<std::shared_ptr<DataObject>>(value);
        if (!objectPtr) {
            return nullptr;
        }
        auto &object = *objectPtr;
        if (object.hasProperty(key)) {
            return &object[key];
        } else if (createPath) {
            return &object[key];
        }
    } else if (createPath && std::holds_alternative<std::monostate>(value)) {
        // Convert to object
        DataObject newObject;
        DataValue nullValue;  // Default construct a DataValue with monostate
        newObject[key] = std::move(nullValue);
        value = std::make_shared<DataObject>(std::move(newObject));
        return &(*std::get<std::shared_ptr<DataObject>>(value))[key];
    }

    return nullptr;
}

const DataModelEngine::DataValue *DataModelEngine::navigateNestedValue(const DataValue &value,
                                                                       const std::string &key) const {
    // Try array access first
    auto arrayIndex = parseArrayIndex(key);
    if (arrayIndex.has_value()) {
        if (std::holds_alternative<std::shared_ptr<DataArray>>(value)) {
            const auto &arrayPtr = std::get<std::shared_ptr<DataArray>>(value);
            if (!arrayPtr) {
                return nullptr;
            }
            const auto &array = *arrayPtr;
            if (arrayIndex.value() < array.size()) {
                return &array[arrayIndex.value()];
            }
        }
        return nullptr;
    }

    // Object property access
    if (std::holds_alternative<std::shared_ptr<DataObject>>(value)) {
        const auto &objectPtr = std::get<std::shared_ptr<DataObject>>(value);
        if (!objectPtr) {
            return nullptr;
        }
        const auto &object = *objectPtr;
        if (object.hasProperty(key)) {
            return &object[key];
        }
    }

    return nullptr;
}

bool DataModelEngine::removeNestedValue(DataValue &value, const std::string &key) {
    // Try array access first
    auto arrayIndex = parseArrayIndex(key);
    if (arrayIndex.has_value()) {
        if (std::holds_alternative<std::shared_ptr<DataArray>>(value)) {
            auto arrayPtr = std::get<std::shared_ptr<DataArray>>(value);
            if (!arrayPtr) {
                return false;
            }
            auto &array = *arrayPtr;
            if (arrayIndex.value() < array.size()) {
                array.erase(arrayIndex.value());
                return true;
            }
        }
        return false;
    }

    // Object property removal
    if (std::holds_alternative<std::shared_ptr<DataObject>>(value)) {
        auto &objectPtr = std::get<std::shared_ptr<DataObject>>(value);
        if (!objectPtr) {
            return false;
        }
        auto &object = *objectPtr;
        if (object.hasProperty(key)) {
            object.removeProperty(key);
            return true;
        }
    }

    return false;
}

bool DataModelEngine::setNestedValue(DataValue &value, const std::string &key, const DataValue &newValue) {
    // Try array access first
    auto arrayIndex = parseArrayIndex(key);
    if (arrayIndex.has_value()) {
        if (!ensureObjectForNesting(value)) {
            return false;
        }

        if (std::holds_alternative<std::shared_ptr<DataArray>>(value)) {
            auto &arrayPtr = std::get<std::shared_ptr<DataArray>>(value);
            if (!arrayPtr) {
                return false;
            }
            auto &array = *arrayPtr;
            // Expand array if needed
            while (array.size() <= arrayIndex.value()) {
                DataValue nullValue;  // Default construct a DataValue with monostate
                array.push_back(std::move(nullValue));
            }
            array[arrayIndex.value()] = newValue;
            return true;
        }
    }

    // Object property setting
    if (!ensureObjectForNesting(value)) {
        return false;
    }

    if (std::holds_alternative<std::shared_ptr<DataObject>>(value)) {
        auto &objectPtr = std::get<std::shared_ptr<DataObject>>(value);
        if (!objectPtr) {
            return false;
        }
        auto &object = *objectPtr;
        object[key] = newValue;
        return true;
    }

    return false;
}

bool DataModelEngine::ensureObjectForNesting(DataValue &value) {
    if (std::holds_alternative<std::shared_ptr<DataObject>>(value) ||
        std::holds_alternative<std::shared_ptr<DataArray>>(value)) {
        return true;  // Already suitable for nesting
    }

    if (std::holds_alternative<std::monostate>(value)) {
        // Convert null/undefined to empty object
        value = std::make_shared<DataObject>();
        return true;
    }

    // Cannot nest in primitive types
    return false;
}

std::optional<size_t> DataModelEngine::parseArrayIndex(const std::string &key) const {
    if (key.empty()) {
        return std::nullopt;
    }

    // Check if key is a valid array index (all digits)
    for (char c : key) {
        if (!std::isdigit(c)) {
            return std::nullopt;
        }
    }

    try {
        size_t index = std::stoull(key);
        return index;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

void DataModelEngine::logOperation(const std::string &operation, const std::string &location, bool success) const {
    std::string logLevel = success ? "debug" : "warning";
    std::string status = success ? "SUCCESS" : "FAILED";

    SCXML::Common::Logger::debug("DataModel " + operation + " " + status + ": " + location);
}

// ========== DataArray Implementation ==========

int64_t DataModelEngine::DataArray::indexOf(const DataValue &value) const {
    for (size_t i = 0; i < elements_.size(); ++i) {
        // Simple comparison using string representation
        std::string thisStr = std::visit(
            [](const auto &v) -> std::string {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    return "null";
                } else if constexpr (std::is_same_v<T, bool>) {
                    return v ? "true" : "false";
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, double>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return v;
                }
                return "";
            },
            elements_[i]);

        std::string valueStr = std::visit(
            [](const auto &v) -> std::string {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    return "null";
                } else if constexpr (std::is_same_v<T, bool>) {
                    return v ? "true" : "false";
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, double>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return v;
                }
                return "";
            },
            value);

        if (thisStr == valueStr) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
}

std::string DataModelEngine::DataArray::toString() const {
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto &item : elements_) {
        if (!first) {
            oss << ",";
        }

        // Convert element to string
        std::string itemStr = std::visit(
            [](const auto &v) -> std::string {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    return "null";
                } else if constexpr (std::is_same_v<T, bool>) {
                    return v ? "true" : "false";
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, double>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return "\"" + v + "\"";
                } else if constexpr (std::is_same_v<T, std::shared_ptr<DataArray>>) {
                    return v ? v->toString() : "null";
                } else if constexpr (std::is_same_v<T, std::shared_ptr<DataObject>>) {
                    return v ? v->toString() : "null";
                }
                return "";
            },
            item);

        oss << itemStr;
        first = false;
    }
    oss << "]";
    return oss.str();
}

// ========== DataObject Implementation ==========

std::string DataModelEngine::DataObject::toString() const {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto &pair : properties_) {
        if (!first) {
            oss << ",";
        }

        // Convert value to string
        std::string valueStr = std::visit(
            [](const auto &v) -> std::string {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    return "null";
                } else if constexpr (std::is_same_v<T, bool>) {
                    return v ? "true" : "false";
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, double>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return "\"" + v + "\"";
                } else if constexpr (std::is_same_v<T, std::shared_ptr<DataArray>>) {
                    return v ? v->toString() : "null";
                } else if constexpr (std::is_same_v<T, std::shared_ptr<DataObject>>) {
                    return v ? v->toString() : "null";
                }
                return "";
            },
            pair.second);

        oss << "\"" << pair.first << "\":" << valueStr;
        first = false;
    }
    oss << "}";
    return oss.str();
}

// ========== Variable Scoping and Inheritance Implementation ==========

void DataModelEngine::pushScope(const std::string &scopeId) {
    scopeStack_.emplace_back();
    scopeIds_.push_back(scopeId.empty() ? "scope_" + std::to_string(scopeStack_.size()) : scopeId);

    SCXML::Common::Logger::debug("DataModel: Pushed new scope '" + scopeIds_.back() +
                                 "' (depth: " + std::to_string(scopeStack_.size()) + ")");
}

bool DataModelEngine::popScope() {
    if (scopeStack_.empty()) {
        return false;
    }

    std::string scopeId = scopeIds_.back();
    scopeStack_.pop_back();
    scopeIds_.pop_back();

    SCXML::Common::Logger::debug("DataModel: Popped scope '" + scopeId +
                                 "' (depth: " + std::to_string(scopeStack_.size()) + ")");
    return true;
}

size_t DataModelEngine::getScopeDepth() const {
    return scopeStack_.size();
}

DataModelEngine::DataModelResult DataModelEngine::setValueWithInheritance(const std::string &location,
                                                                          const DataValue &value, Scope scope,
                                                                          bool allowInheritance) {
    auto path = parseLocation(location);
    if (!path.isValid) {
        return DataModelResult::createError("Invalid location path: " + location);
    }

    // If inheritance is allowed, check if variable exists in parent scopes
    if (allowInheritance && scope == Scope::LOCAL) {
        auto existingScope = findVariableScope(location);
        if (existingScope && *existingScope != Scope::LOCAL) {
            // Variable exists in parent scope, update it there to maintain inheritance
            scope = *existingScope;
            SCXML::Common::Logger::debug("DataModel: Variable '" + location + "' inherited from " +
                                         (scope == Scope::GLOBAL ? "global" : "session") + " scope");
        }
    }

    // Handle scope stack for LOCAL scope
    if (scope == Scope::LOCAL && !scopeStack_.empty()) {
        auto *targetValue = navigateToLocation(scopeStack_.back(), path, true);
        if (!targetValue) {
            return DataModelResult::createError("Failed to navigate to location: " + location);
        }
        *targetValue = value;

        logOperation("SET_STACK", location + "@" + scopeIds_.back(), true);
        return DataModelResult::createSuccess();
    }

    // Use regular setValue for other scopes
    return setValue(location, value, scope);
}

DataModelEngine::DataModelResult DataModelEngine::getValueWithInheritance(const std::string &location,
                                                                          const std::vector<Scope> &searchOrder) {
    auto path = parseLocation(location);
    if (!path.isValid) {
        return DataModelResult::createError("Invalid location path: " + location);
    }

    // First search in scope stack (most recent scopes first)
    for (auto it = scopeStack_.rbegin(); it != scopeStack_.rend(); ++it) {
        const auto *value = navigateToLocation(*it, path);
        if (value) {
            size_t scopeIndex = static_cast<size_t>(scopeStack_.rend() - it - 1);
            logOperation("GET_STACK", location + "@" + scopeIds_[scopeIndex], true);
            return DataModelResult::createSuccess(*value);
        }
    }

    // Then search in main scopes according to search order
    for (auto scope : searchOrder) {
        const auto &storage = getDataStorage(scope);
        const auto *value = navigateToLocation(storage, path);

        if (value) {
            logOperation("GET_INHERITED",
                         location + "@" +
                             (scope == Scope::GLOBAL  ? "global"
                              : scope == Scope::LOCAL ? "local"
                                                      : "session"),
                         true);
            return DataModelResult::createSuccess(*value);
        }
    }

    logOperation("GET_INHERITED", location, false);
    return DataModelResult::createError("Location not found with inheritance: " + location);
}

std::optional<DataModelEngine::Scope> DataModelEngine::findVariableScope(const std::string &location,
                                                                         const std::vector<Scope> &searchOrder) {
    auto result = getValueWithInheritance(location, searchOrder);
    if (!result.success) {
        return std::nullopt;
    }

    // Determine which scope actually contains the variable
    auto path = parseLocation(location);

    // Check scope stack first
    for (auto it = scopeStack_.rbegin(); it != scopeStack_.rend(); ++it) {
        const auto *value = navigateToLocation(*it, path);
        if (value) {
            return Scope::LOCAL;  // Stack scopes are considered LOCAL
        }
    }

    // Check main scopes
    for (auto scope : searchOrder) {
        const auto &storage = getDataStorage(scope);
        const auto *value = navigateToLocation(storage, path);
        if (value) {
            return scope;
        }
    }

    return std::nullopt;
}

size_t DataModelEngine::copyScope(Scope fromScope, Scope toScope, const std::string &filter) {
    const auto &source = getDataStorage(fromScope);
    auto &target = getDataStorage(toScope);

    std::regex filterRegex(filter);
    size_t copiedCount = 0;

    for (const auto &pair : source) {
        if (std::regex_match(pair.first, filterRegex)) {
            target[pair.first] = pair.second;
            copiedCount++;
        }
    }

    SCXML::Common::Logger::debug("DataModel: Copied " + std::to_string(copiedCount) + " variables from " +
                                 (fromScope == Scope::GLOBAL  ? "global"
                                  : fromScope == Scope::LOCAL ? "local"
                                                              : "session") +
                                 " to " +
                                 (toScope == Scope::GLOBAL  ? "global"
                                  : toScope == Scope::LOCAL ? "local"
                                                            : "session"));

    return copiedCount;
}

size_t DataModelEngine::mergeScopes(Scope primaryScope, Scope secondaryScope, Scope targetScope) {
    const auto &primary = getDataStorage(primaryScope);
    const auto &secondary = getDataStorage(secondaryScope);
    auto &target = getDataStorage(targetScope);

    size_t mergedCount = 0;

    // Copy all from secondary first
    for (const auto &pair : secondary) {
        target[pair.first] = pair.second;
        mergedCount++;
    }

    // Override with primary (primary wins conflicts)
    for (const auto &pair : primary) {
        target[pair.first] = pair.second;
        if (secondary.find(pair.first) == secondary.end()) {
            mergedCount++;  // Only count if it's not overriding
        }
    }

    SCXML::Common::Logger::debug("DataModel: Merged scopes - " + std::to_string(mergedCount) + " variables total");

    return mergedCount;
}

// ========== Data Binding and Reactivity Implementation ==========

size_t DataModelEngine::onDataChange(const std::string &location, DataChangeCallback callback,
                                     std::optional<Scope> scope) {
    size_t id = nextCallbackId_++;
    dataChangeListeners_.push_back({id, location, callback, scope});

    SCXML::Common::Logger::debug("DataModel: Registered data change listener " + std::to_string(id) + " for '" +
                                 location + "'");
    return id;
}

bool DataModelEngine::removeDataChangeCallback(size_t callbackId) {
    auto it = std::find_if(dataChangeListeners_.begin(), dataChangeListeners_.end(),
                           [callbackId](const DataChangeListener &listener) { return listener.id == callbackId; });

    if (it != dataChangeListeners_.end()) {
        SCXML::Common::Logger::debug("DataModel: Removed data change listener " + std::to_string(callbackId));
        dataChangeListeners_.erase(it);
        return true;
    }

    return false;
}

size_t DataModelEngine::createBinding(const std::string &location1, const std::string &location2, Scope scope1,
                                      Scope scope2) {
    size_t id = nextCallbackId_++;
    dataBindings_.push_back({id, location1, location2, scope1, scope2, false});

    SCXML::Common::Logger::debug("DataModel: Created binding " + std::to_string(id) + " between '" + location1 +
                                 "' and '" + location2 + "'");
    return id;
}

bool DataModelEngine::removeBinding(size_t bindingId) {
    auto it = std::find_if(dataBindings_.begin(), dataBindings_.end(),
                           [bindingId](const DataBinding &binding) { return binding.id == bindingId; });

    if (it != dataBindings_.end()) {
        SCXML::Common::Logger::debug("DataModel: Removed binding " + std::to_string(bindingId));
        dataBindings_.erase(it);
        return true;
    }

    return false;
}

size_t DataModelEngine::createComputed(const std::string &location, const std::string &expression,
                                       const std::vector<std::string> &dependencies, Scope targetScope) {
    size_t id = nextCallbackId_++;
    ComputedProperty computed = {id, location, expression, dependencies, targetScope, std::monostate{}};

    // Initial computation - defer until context is available
    // The computed property will be evaluated when updateComputedProperties is called
    // with a proper context during runtime operations
    computed.lastValue = std::monostate{};

    computedProperties_.push_back(computed);

    SCXML::Common::Logger::debug("DataModel: Created computed property " + std::to_string(id) + " at '" + location +
                                 "' with " + std::to_string(dependencies.size()) + " dependencies");
    return id;
}

bool DataModelEngine::removeComputed(size_t computedId) {
    auto it = std::find_if(computedProperties_.begin(), computedProperties_.end(),
                           [computedId](const ComputedProperty &computed) { return computed.id == computedId; });

    if (it != computedProperties_.end()) {
        SCXML::Common::Logger::debug("DataModel: Removed computed property " + std::to_string(computedId));
        computedProperties_.erase(it);
        return true;
    }

    return false;
}

void DataModelEngine::triggerReactiveUpdate(const std::string &location, Scope scope) {
    SCXML::Common::Logger::debug("DataModel: Triggering reactive update for '" + location + "'");

    // Get current value for notifications
    auto currentResult = getValue(location, scope);
    DataValue currentValue = currentResult.success ? currentResult.value : std::monostate{};

    // Update bindings
    updateBindings(location, currentValue, scope);

    // Update computed properties
    // TEMPORARILY DISABLED: updateComputedProperties causes JavaScript context corruption
    // updateComputedProperties(location, scope);
}

std::unordered_map<size_t, std::string> DataModelEngine::getActiveBindings() const {
    std::unordered_map<size_t, std::string> bindings;
    for (const auto &binding : dataBindings_) {
        bindings[binding.id] = binding.location1 + " <-> " + binding.location2;
    }
    return bindings;
}

std::unordered_map<size_t, std::string> DataModelEngine::getActiveComputed() const {
    std::unordered_map<size_t, std::string> computed;
    for (const auto &comp : computedProperties_) {
        computed[comp.id] = comp.location + " = " + comp.expression;
    }
    return computed;
}

bool DataModelEngine::matchesPattern(const std::string &location, const std::string &pattern) const {
    // Simple wildcard matching: * matches any characters
    if (pattern == "*") {
        return true;
    }
    if (pattern.find('*') == std::string::npos) {
        return location == pattern;
    }

    try {
        // Convert wildcard pattern to regex
        std::string regexPattern = pattern;
        std::regex_replace(regexPattern, std::regex(R"(\*)"), ".*");
        std::regex_replace(regexPattern, std::regex(R"(\?)"), ".");

        std::regex regex(regexPattern);
        return std::regex_match(location, regex);
    } catch (const std::exception &e) {
        SCXML::Common::Logger::warning("DataModel: Invalid pattern '" + pattern + "': " + e.what());
        return location == pattern;  // Fallback to exact match
    }
}

void DataModelEngine::notifyDataChange(const std::string &location, const DataValue &oldValue,
                                       const DataValue &newValue, Scope scope) {
    for (const auto &listener : dataChangeListeners_) {
        // Check scope filter
        if (listener.scope && *listener.scope != scope) {
            continue;
        }

        // Check location pattern
        if (matchesPattern(location, listener.locationPattern)) {
            try {
                listener.callback(location, oldValue, newValue, scope);
            } catch (const std::exception &e) {
                SCXML::Common::Logger::error("DataModel: Data change callback error: " + std::string(e.what()));
            }
        }
    }
}

void DataModelEngine::updateComputedProperties(const std::string &location, Scope scope) {
    (void)location;  // Suppress unused parameter warning
    (void)scope;     // Suppress unused parameter warning
    // Computed properties need a RuntimeContext to evaluate expressions
    // This method is called internally and should be enhanced to accept context
    // Skip computed properties update when context is not available (design limitation)
    SCXML::Common::Logger::debug("Skipping computed property update for location: " + location +
                                 " (context not available)");

    for (auto &computed : computedProperties_) {
        // Check if this location is a dependency
        bool isDependency = false;
        for (const auto &dep : computed.dependencies) {
            if (matchesPattern(location, dep)) {
                isDependency = true;
                break;
            }
        }

        if (isDependency) {
            // Note: Computed property context requires expression evaluation framework
            // auto result = evaluateExpression(computed.expression, *dummyContext);
            // Skip computed property evaluation - requires complete expression framework
            (void)computed;  // Suppress unused warning
            SCXML::Common::Logger::debug("Skipping computed property evaluation: " + computed.location);
        }
    }
}

void DataModelEngine::updateBindings(const std::string &location, const DataValue &newValue, Scope scope) {
    for (auto &binding : dataBindings_) {
        if (binding.updating) {
            continue;  // Prevent circular updates
        }

        bool shouldUpdate = false;
        std::string targetLocation;
        Scope targetScope;

        // Check which side of the binding changed
        if (binding.location1 == location && binding.scope1 == scope) {
            shouldUpdate = true;
            targetLocation = binding.location2;
            targetScope = binding.scope2;
        } else if (binding.location2 == location && binding.scope2 == scope) {
            shouldUpdate = true;
            targetLocation = binding.location1;
            targetScope = binding.scope1;
        }

        if (shouldUpdate) {
            binding.updating = true;  // Prevent recursion

            auto oldResult = getValue(targetLocation, targetScope);
            DataValue oldValue = oldResult.success ? oldResult.value : std::monostate{};

            // Set the new value on the other side
            setValue(targetLocation, newValue, targetScope);

            // Notify change
            notifyDataChange(targetLocation, oldValue, newValue, targetScope);

            SCXML::Common::Logger::debug("DataModel: Updated binding from '" + location + "' to '" + targetLocation +
                                         "'");

            binding.updating = false;
        }
    }
}

IECMAScriptEngine *DataModelEngine::getECMAScriptEngine() const {
    if (dataModelType_ == DataModelType::ECMASCRIPT && evaluatorImpl_ && evaluatorImpl_->ecmaScriptEngine) {
        return evaluatorImpl_->ecmaScriptEngine.get();
    }
    return nullptr;
}

void DataModelEngine::setECMAScriptEngine(std::shared_ptr<IECMAScriptEngine> engine) {
    if (!evaluatorImpl_) {
        SCXML::Common::Logger::error("DataModelEngine::setECMAScriptEngine - EvaluatorImpl not initialized");
        return;
    }

    if (engine) {
        SCXML::Common::Logger::info("DataModelEngine::setECMAScriptEngine - Setting ECMAScript engine: " +
                                    engine->getEngineName());
        evaluatorImpl_->ecmaScriptEngine = engine;
        // Set data model type to ECMAScript if engine is provided
        dataModelType_ = DataModelType::ECMASCRIPT;
    } else {
        SCXML::Common::Logger::warning("DataModelEngine::setECMAScriptEngine - Setting null ECMAScript engine");
        evaluatorImpl_->ecmaScriptEngine.reset();
    }
}

DataModelEngine::DataValue DataModelEngine::dataValueToECMAValue(const DataValue &value) const {
    return std::visit(
        [](const auto &v) -> DataValue {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return DataValue(std::monostate{});
            } else if constexpr (std::is_same_v<T, bool>) {
                return DataValue(v);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return DataValue(static_cast<double>(v));  // Convert to double for QuickJS
            } else if constexpr (std::is_same_v<T, double>) {
                return DataValue(v);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return DataValue(v);
            }
            return DataValue(std::monostate{});
        },
        value);
}

}  // namespace SCXML