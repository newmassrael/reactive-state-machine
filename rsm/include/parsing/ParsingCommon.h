#pragma once

#include "common/Logger.h"
#include <libxml++/libxml++.h>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Common utility class for parsing
 *
 * This class provides functions and constants commonly used
 * across multiple parser classes. Includes utilities for
 * namespace handling, attribute validation, path processing, etc.
 */

namespace RSM {

class ParsingCommon {
public:
    /**
     * @brief SCXML-related constants
     */
    struct Constants {
        static const std::string SCXML_NAMESPACE;
        static const std::string CODE_NAMESPACE;
        static const std::string CTX_NAMESPACE;
        static const std::string DI_NAMESPACE;
    };

    /**
     * @brief Compare node names considering namespace
     * @param nodeName Node name to check
     * @param baseName Base name (without namespace)
     * @return Whether they match
     */
    static bool matchNodeName(const std::string &nodeName, const std::string &baseName);

    /**
     * @brief Find child elements with specified name (considering namespace)
     * @param element Parent element
     * @param childName Child name to find
     * @return Found child elements
     */
    static std::vector<const xmlpp::Element *> findChildElements(const xmlpp::Element *element,
                                                                 const std::string &childName);

    /**
     * @brief Find first child element with specified name (considering namespace)
     * @param element Parent element
     * @param childName Child name to find
     * @return Found child element, nullptr if not found
     */
    static const xmlpp::Element *findFirstChildElement(const xmlpp::Element *element, const std::string &childName);

    /**
     * @brief Find ID attribute in element or parent element
     * @param element Element to check
     * @return Found ID, empty string if not found
     */
    static std::string findElementId(const xmlpp::Element *element);

    /**
     * @brief Get attribute value (try multiple names)
     * @param element Element with attributes
     * @param attrNames List of attribute names to try
     * @return Attribute value, empty string if not found
     */
    static std::string getAttributeValue(const xmlpp::Element *element, const std::vector<std::string> &attrNames);

    /**
     * @brief Collect all attributes into map
     * @param element Element with attributes
     * @param excludeAttrs List of attribute names to exclude
     * @return Attribute map (name -> value)
     */
    static std::unordered_map<std::string, std::string>
    collectAttributes(const xmlpp::Element *element, const std::vector<std::string> &excludeAttrs = {});

    /**
     * @brief Convert relative path to absolute path
     * @param basePath Base path
     * @param relativePath Relative path
     * @return Absolute path
     */
    static std::string resolveRelativePath(const std::string &basePath, const std::string &relativePath);

    /**
     * @brief Extract text content from text node
     * @param element Element containing text
     * @param trimWhitespace Whether to trim whitespace
     * @return Extracted text
     */
    static std::string extractTextContent(const xmlpp::Element *element, bool trimWhitespace = true);

    /**
     * @brief Extract XML element name (remove namespace)
     * @param element XML element
     * @return Element name without namespace
     */
    static std::string getLocalName(const xmlpp::Element *element);

    static std::vector<const xmlpp::Element *> findChildElementsWithNamespace(const xmlpp::Element *parent,
                                                                              const std::string &elementName,
                                                                              const std::string &namespaceURI);

    static std::string trimString(const std::string &str);

private:
    // Prevent instance creation
    ParsingCommon() = delete;
};

}  // namespace RSM