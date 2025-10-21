// IDataModelItem.h
#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Data model item interface
 *
 * This interface represents an item in the SCXML data model.
 * Corresponds to <data> element in SCXML documents.
 */

namespace RSM {

class IDataModelItem {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IDataModelItem() {}

    /**
     * @brief Return item ID
     * @return Item ID
     */
    virtual const std::string &getId() const = 0;

    /**
     * @brief Set expression
     * @param expr Expression
     */
    virtual void setExpr(const std::string &expr) = 0;

    /**
     * @brief Return expression
     * @return Expression
     */
    virtual const std::string &getExpr() const = 0;

    /**
     * @brief Set type
     * @param type Data type
     */
    virtual void setType(const std::string &type) = 0;

    /**
     * @brief Return type
     * @return Data type
     */
    virtual const std::string &getType() const = 0;

    /**
     * @brief Set scope
     * @param scope Data scope (e.g., "local", "global")
     */
    virtual void setScope(const std::string &scope) = 0;

    /**
     * @brief Return scope
     * @return Data scope
     */
    virtual const std::string &getScope() const = 0;

    /**
     * @brief Set content
     * @param content Item content
     */
    virtual void setContent(const std::string &content) = 0;

    /**
     * @brief Return content
     * @return Item content
     */
    virtual const std::string &getContent() const = 0;

    /**
     * @brief Set source URL
     * @param src External data source URL
     */
    virtual void setSrc(const std::string &src) = 0;

    /**
     * @brief Return source URL
     * @return External data source URL
     */
    virtual const std::string &getSrc() const = 0;

    /**
     * @brief Set additional attribute
     * @param name Attribute name
     * @param value Attribute value
     */
    virtual void setAttribute(const std::string &name, const std::string &value) = 0;

    /**
     * @brief Return attribute value
     * @param name Attribute name
     * @return Attribute value, empty string if not found
     */
    virtual const std::string &getAttribute(const std::string &name) const = 0;

    /**
     * @brief Return all attributes
     * @return Map of all attributes
     */
    virtual const std::unordered_map<std::string, std::string> &getAttributes() const = 0;

    /**
     * @brief Add XML content
     * @param content XML content to add
     * This method adds new content to existing XML content.
     * Useful when data model is XML-based, preserves existing content structure.
     */
    virtual void addContent(const std::string &content) = 0;

    /**
     * @brief Return all content items
     * @return List of all content items in order of addition
     */
    virtual const std::vector<std::string> &getContentItems() const = 0;

    /**
     * @brief Check if content is in XML format
     * @return true if XML format, false otherwise
     */
    virtual bool isXmlContent() const = 0;

    /**
     * @brief Execute XPath query (applies to XML content only)
     * @param xpath XPath query string
     * @return Query result string, empty optional if failed
     */
    virtual std::optional<std::string> queryXPath(const std::string &xpath) const = 0;

    /**
     * @brief Check if content can be processed according to data model type
     * @param dataModelType Data model type (e.g., "ecmascript", "xpath", "null")
     * @return true if processing supported, false otherwise
     */
    virtual bool supportsDataModel(const std::string &dataModelType) const = 0;
};

}  // namespace RSM