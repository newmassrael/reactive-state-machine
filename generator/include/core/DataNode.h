#pragma once
#include "model/IDataModelItem.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

using SCXML::Model::IDataModelItem;


namespace SCXML {

namespace Model {
class IDataModelItem;
}

// Forward declarations
namespace Runtime {
class RuntimeContext;
}

namespace Core {
class IExecutionContext;
}

namespace Core {

/**
 * @brief SCXML <data> element implementation
 *
 * The <data> element is used to declare and optionally initialize data model variables.
 * It defines initial values and data structures available throughout the state machine.
 *
 * SCXML W3C Specification compliance:
 * - id attribute: Variable identifier
 * - src attribute: External data source URI
 * - expr attribute: Expression for initial value
 * - Inline content: Direct data specification
 */
class DataNode : public IDataModelItem {
public:
    /**
     * @brief Data value type supporting multiple formats
     */
    using DataValue = std::variant<std::monostate,  // undefined/null
                                   std::string,     // string data
                                   int,             // integer data
                                   double,          // floating point data
                                   bool             // boolean data
                                   >;

    /**
     * @brief Construct a new Data Node
     * @param id Data variable identifier
     */
    explicit DataNode(const std::string &id);

    /**
     * @brief Virtual destructor
     */
    virtual ~DataNode() = default;

    /**
     * @brief Get the data variable identifier
     * @return Variable ID string
     */
    const std::string &getId() const override {
        return id_;
    }

    /**
     * @brief Set external data source URI
     * @param src URI to external data source (file, HTTP, etc.)
     */
    void setSrc(const std::string &src);

    /**
     * @brief Get external data source URI
     * @return Source URI string
     */
    const std::string &getSrc() const {
        return src_;
    }

    /**
     * @brief Set initialization expression
     * @param expr ECMAScript expression for computing initial value
     */
    void setExpr(const std::string &expr);

    /**
     * @brief Get initialization expression
     * @return Expression string
     */
    const std::string &getExpr() const {
        return expr_;
    }

    /**
     * @brief Set inline data content
     * @param content Direct data value (JSON, XML, plain text)
     */
    void setContent(const std::string &content);

    /**
     * @brief Get inline data content
     * @return Content string
     */
    const std::string &getContent() const {
        return content_;
    }

    /**
     * @brief Set data format/type hint
     * @param format Data format ("json", "xml", "text", "ecmascript")
     */
    void setFormat(const std::string &format);

    /**
     * @brief Get data format
     * @return Format string
     */
    const std::string &getFormat() const {
        return format_;
    }

    /**
     * @brief Initialize data variable in runtime context
     * @param context Runtime context for data model access
     * @return true if initialization succeeded
     */
    bool initialize(::SCXML::Runtime::RuntimeContext &context);

    // IDataModelItem interface implementation
    bool supportsDataModel(const std::string &dataModelType) const override;
    std::optional<std::string> queryXPath(const std::string &xpath) const override;
    bool isXmlContent() const override;
    const std::vector<std::string> &getContentItems() const override;
    void addContent(const std::string &content) override;
    const std::unordered_map<std::string, std::string> &getAttributes() const override;
    const std::string &getAttribute(const std::string &name) const override;
    void setAttribute(const std::string &name, const std::string &value) override;
    void setType(const std::string &type) override;
    const std::string &getType() const override;
    void setScope(const std::string &scope) override;
    const std::string &getScope() const override;
    std::vector<std::string> validate() const;

    /**
     * @brief Get computed data value
     * @param context Runtime context for expression evaluation
     * @return Resolved data value
     */
    DataValue getValue(::SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Clone this data node
     * @return Deep copy of this DataNode
     */

protected:
    /**
     * @brief Load data from external source
     * @param context Runtime context
     * @return Loaded data content
     */
    std::string loadFromSrc(::SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Parse data content based on format
     * @param content Raw data content
     * @param format Data format hint
     * @return Parsed data value
     */
    DataValue parseContent(const std::string &content, const std::string &format) const;

    /**
     * @brief Evaluate expression to get data value
     * @param context Runtime context
     * @return Evaluated data value
     */
    DataValue evaluateExpression(::SCXML::Runtime::RuntimeContext &context) const;

private:
    std::string id_;                                           // Data variable identifier
    std::string src_;                                          // External data source URI
    std::string expr_;                                         // Initialization expression
    std::string content_;                                      // Inline data content
    std::string format_;                                       // Data format hint
    std::vector<std::string> contentItems_;                    // Content items for IDataModelItem interface
    std::unordered_map<std::string, std::string> attributes_;  // Attributes for IDataModelItem interface
    std::string type_;                                         // Type for IDataModelItem interface
    std::string scope_;                                        // Scope for IDataModelItem interface
};

}  // namespace Core
}  // namespace SCXML
