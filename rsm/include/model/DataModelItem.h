#pragma once

#include "IDataModelItem.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Include xmlpp headers
#include <libxml++/document.h>
#include <libxml++/nodes/node.h>

/**
 * @brief Implementation class for data model item
 *
 * This class represents an item in the SCXML data model.
 * Corresponds to the <data> element in an SCXML document.
 */

namespace RSM {

class DataModelItem : public IDataModelItem {
public:
    /**
     * @brief Constructor
     * @param id Item identifier
     * @param expr Expression (optional)
     */
    explicit DataModelItem(const std::string &id, const std::string &expr = "");

    /**
     * @brief Destructor
     */
    virtual ~DataModelItem();

    /**
     * @brief Return item ID
     * @return Item ID
     */
    virtual const std::string &getId() const override;

    /**
     * @brief Set expression
     * @param expr Expression
     */
    virtual void setExpr(const std::string &expr) override;

    /**
     * @brief Return expression
     * @return Expression
     */
    virtual const std::string &getExpr() const override;

    /**
     * @brief Set type
     * @param type Data type
     */
    virtual void setType(const std::string &type) override;

    /**
     * @brief Return type
     * @return Data type
     */
    virtual const std::string &getType() const override;

    /**
     * @brief Set scope
     * @param scope Data scope (e.g., "local", "global")
     */
    virtual void setScope(const std::string &scope) override;

    /**
     * @brief Return scope
     * @return Data scope
     */
    virtual const std::string &getScope() const override;

    /**
     * @brief Set content
     * @param content Item content
     */
    virtual void setContent(const std::string &content) override;

    /**
     * @brief Return content
     * @return Item content
     */
    virtual const std::string &getContent() const override;

    /**
     * @brief Set source URL
     * @param src External data source URL
     */
    virtual void setSrc(const std::string &src) override;

    /**
     * @brief Return source URL
     * @return External data source URL
     */
    virtual const std::string &getSrc() const override;

    /**
     * @brief Set additional attribute
     * @param name Attribute name
     * @param value Attribute value
     */
    virtual void setAttribute(const std::string &name, const std::string &value) override;

    /**
     * @brief Return attribute value
     * @param name Attribute name
     * @return Attribute value, empty string if not found
     */
    virtual const std::string &getAttribute(const std::string &name) const override;

    /**
     * @brief Return all attributes
     * @return Map of all attributes
     */
    virtual const std::unordered_map<std::string, std::string> &getAttributes() const override;

    /**
     * @brief Add XML content
     * @param content XML content to add
     * This method adds new content to the existing XML content.
     * Useful when the data model is XML-based, preserves existing content structure.
     */
    virtual void addContent(const std::string &content) override;

    /**
     * @brief Return all content items
     * @return List of all content items in order of addition
     */
    virtual const std::vector<std::string> &getContentItems() const override;

    /**
     * @brief Check if content is in XML format
     * @return true if XML format, false otherwise
     */
    virtual bool isXmlContent() const override;

    /**
     * @brief Execute XPath query (applies to XML content only)
     * @param xpath XPath query string
     * @return Query result string, empty optional if failed
     */
    virtual std::optional<std::string> queryXPath(const std::string &xpath) const override;

    /**
     * @brief Check if content can be processed according to data model type
     * @param dataModelType Data model type (e.g., "ecmascript", "xpath", "null")
     * @return true if processing supported, false otherwise
     */
    virtual bool supportsDataModel(const std::string &dataModelType) const override;

    /**
     * @brief Set XML content
     * @param content XML content string
     */
    void setXmlContent(const std::string &content);

    /**
     * @brief Return XML content node
     * @return XML node pointer, nullptr if not found
     */
    xmlpp::Node *getXmlContent() const;

private:
    std::string id_;
    std::string expr_;
    std::string type_;
    std::string scope_;
    std::string content_;
    std::unique_ptr<xmlpp::Document> xmlContent_;
    std::string src_;
    std::unordered_map<std::string, std::string> attributes_;
    std::vector<std::string> contentItems_;  // Store added content items
    std::string emptyString_;
};

}  // namespace RSM