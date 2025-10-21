// SCXMLContext.h
#pragma once

#include <string>
#include <unordered_map>

/**
 * @brief Class containing SCXML parsing context information
 *
 * This class contains contextual information needed during SCXML parsing.
 * For example, it includes global data model type, namespace information, etc.
 */

namespace RSM {

class SCXMLContext {
public:
    /**
     * @brief Default constructor
     */
    SCXMLContext() = default;

    /**
     * @brief Set data model type
     * @param datamodelType Data model type (e.g., "ecmascript", "xpath", "null")
     */
    void setDatamodelType(const std::string &datamodelType);

    /**
     * @brief Return data model type
     * @return Data model type
     */
    const std::string &getDatamodelType() const;

    /**
     * @brief Set binding mode
     * @param binding Binding mode (e.g., "early", "late")
     */
    void setBinding(const std::string &binding);

    /**
     * @brief Return binding mode
     * @return Binding mode
     */
    const std::string &getBinding() const;

    /**
     * @brief Add namespace
     * @param prefix Namespace prefix
     * @param uri Namespace URI
     */
    void addNamespace(const std::string &prefix, const std::string &uri);

    /**
     * @brief Get namespace URI
     * @param prefix Namespace prefix
     * @return Namespace URI (empty string if not found)
     */
    const std::string &getNamespaceURI(const std::string &prefix) const;

    /**
     * @brief Set additional attribute
     * @param name Attribute name
     * @param value Attribute value
     */
    void setAttribute(const std::string &name, const std::string &value);

    /**
     * @brief Return attribute value
     * @param name Attribute name
     * @return Attribute value (empty string if not found)
     */
    const std::string &getAttribute(const std::string &name) const;

private:
    std::string datamodelType_;                                ///< Data model type
    std::string binding_;                                      ///< Binding mode
    std::unordered_map<std::string, std::string> namespaces_;  ///< Namespace mapping
    std::unordered_map<std::string, std::string> attributes_;  ///< Additional attributes
    std::string emptyString_;                                  ///< For returning empty string
};

}  // namespace RSM