#include "parsing/ParsingCommon.h"
#include <algorithm>
#include <filesystem>

// Constant definitions
const std::string RSM::ParsingCommon::Constants::SCXML_NAMESPACE = "http://www.w3.org/2005/07/scxml";
const std::string RSM::ParsingCommon::Constants::CODE_NAMESPACE = "http://www.example.org/code-extensions";
const std::string RSM::ParsingCommon::Constants::CTX_NAMESPACE = "http://www.example.org/context-extensions";
const std::string RSM::ParsingCommon::Constants::DI_NAMESPACE = "http://www.example.org/dependency-injection";

bool RSM::ParsingCommon::matchNodeName(const std::string &nodeName, const std::string &baseName) {
    // Exact match
    if (nodeName == baseName) {
        return true;
    }

    // With namespace (e.g., "code:action")
    size_t colonPos = nodeName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < nodeName.length()) {
        std::string localName = nodeName.substr(colonPos + 1);
        return localName == baseName;
    }

    return false;
}

std::vector<const xmlpp::Element *> RSM::ParsingCommon::findChildElements(const xmlpp::Element *element,
                                                                          const std::string &childName) {
    std::vector<const xmlpp::Element *> result;

    if (!element) {
        LOG_WARN("Null parent element");
        return result;
    }

    // Find child elements with exact name
    auto children = element->get_children(childName);
    for (auto *child : children) {
        auto *childElement = dynamic_cast<const xmlpp::Element *>(child);
        if (childElement) {
            result.push_back(childElement);
        }
    }

    // Additional search for different namespaces
    if (result.empty()) {
        auto allChildren = element->get_children();
        for (auto *child : allChildren) {
            auto *childElement = dynamic_cast<const xmlpp::Element *>(child);
            if (childElement) {
                std::string nodeName = childElement->get_name();
                if (matchNodeName(nodeName, childName)) {
                    result.push_back(childElement);
                }
            }
        }
    }

    return result;
}

const xmlpp::Element *RSM::ParsingCommon::findFirstChildElement(const xmlpp::Element *element,
                                                                const std::string &childName) {
    if (!element) {
        LOG_WARN("Null parent element");
        return nullptr;
    }

    // Find first child element with exact name
    auto child = element->get_first_child(childName);
    if (child) {
        return dynamic_cast<const xmlpp::Element *>(child);
    }

    // Additional search for different namespaces
    auto allChildren = element->get_children();
    for (auto *c : allChildren) {
        auto *childElement = dynamic_cast<const xmlpp::Element *>(c);
        if (childElement) {
            std::string nodeName = childElement->get_name();
            if (matchNodeName(nodeName, childName)) {
                return childElement;
            }
        }
    }

    return nullptr;
}

std::string RSM::ParsingCommon::findElementId(const xmlpp::Element *element) {
    if (!element) {
        return "";
    }

    // Find id attribute directly
    auto idAttr = element->get_attribute("id");
    if (idAttr) {
        return idAttr->get_value();
    }

    // Try alternative attribute
    auto nameAttr = element->get_attribute("name");
    if (nameAttr) {
        return nameAttr->get_value();
    }

    // Find id from parent element
    auto parent = element->get_parent();
    if (parent) {
        auto *parentElement = dynamic_cast<const xmlpp::Element *>(parent);
        if (parentElement) {
            return findElementId(parentElement);
        }
    }

    return "";
}

std::string RSM::ParsingCommon::getAttributeValue(const xmlpp::Element *element,
                                                  const std::vector<std::string> &attrNames) {
    if (!element) {
        LOG_DEBUG("Null element provided");
        return "";
    }

    LOG_DEBUG("Searching for attributes in element: {}", element->get_name());

    // Try each attribute name in the given list
    for (const auto &attrName : attrNames) {
        LOG_DEBUG("Checking attribute: {}", attrName);

        auto attr = element->get_attribute(attrName);
        if (attr) {
            LOG_DEBUG("Found attribute: {} = {}", attrName, attr->get_value());
            return attr->get_value();
        }

        // Find attribute separated by namespace
        std::vector<std::string> namespaces = {"code", "ctx", "di"};
        for (const auto &ns : namespaces) {
            auto nsAttr = element->get_attribute(attrName, ns);
            if (nsAttr) {
                LOG_DEBUG("Found namespaced attribute: {}:{} = {}", ns, attrName, nsAttr->get_value());
                return nsAttr->get_value();
            }
        }
    }

    LOG_DEBUG("No matching attribute found for element: {}", element->get_name());
    return "";
}

std::unordered_map<std::string, std::string>
RSM::ParsingCommon::collectAttributes(const xmlpp::Element *element, const std::vector<std::string> &excludeAttrs) {
    std::unordered_map<std::string, std::string> result;

    if (!element) {
        return result;
    }

    auto attributes = element->get_attributes();
    for (auto *attr : attributes) {
        auto *xmlAttr = dynamic_cast<const xmlpp::Attribute *>(attr);
        if (xmlAttr) {
            std::string name = xmlAttr->get_name();

            // Check if attribute should be excluded
            bool excluded = false;
            for (const auto &excludeName : excludeAttrs) {
                if (matchNodeName(name, excludeName)) {
                    excluded = true;
                    break;
                }
            }

            if (!excluded) {
                // Remove namespace option
                size_t colonPos = name.find(':');
                if (colonPos != std::string::npos && colonPos + 1 < name.length()) {
                    std::string localName = name.substr(colonPos + 1);
                    result[localName] = xmlAttr->get_value();
                } else {
                    result[name] = xmlAttr->get_value();
                }
            }
        }
    }

    return result;
}

std::string RSM::ParsingCommon::resolveRelativePath(const std::string &basePath, const std::string &relativePath) {
    // Return as-is if relative path is absolute
    if (std::filesystem::path(relativePath).is_absolute()) {
        return relativePath;
    }

    // Extract directory part of base path
    std::filesystem::path baseDir = std::filesystem::path(basePath).parent_path();

    // Resolve relative path
    std::filesystem::path resolvedPath = baseDir / relativePath;

    // Normalize
    return std::filesystem::canonical(resolvedPath).string();
}

std::string RSM::ParsingCommon::extractTextContent(const xmlpp::Element *element, bool trimWhitespace) {
    if (!element) {
        return "";
    }

    std::string result;
    auto children = element->get_children();

    for (auto *child : children) {
        // Process regular text nodes
        if (auto *textNode = dynamic_cast<const xmlpp::TextNode *>(child)) {
            result += textNode->get_content();
            continue;
        }

        // Process CDATA sections
        if (auto *cdataNode = dynamic_cast<const xmlpp::CdataNode *>(child)) {
            result += cdataNode->get_content();
            continue;
        }
    }

    if (trimWhitespace) {
        // Trim leading and trailing whitespace
        auto start = result.find_first_not_of(" \t\n\r\f\v");
        if (start == std::string::npos) {
            return "";  // Whitespace only
        }

        auto end = result.find_last_not_of(" \t\n\r\f\v");
        result = result.substr(start, end - start + 1);
    }

    return result;
}

std::string RSM::ParsingCommon::getLocalName(const xmlpp::Element *element) {
    if (!element) {
        return "";
    }

    std::string fullName = element->get_name();
    size_t colonPos = fullName.find(':');

    if (colonPos != std::string::npos && colonPos + 1 < fullName.length()) {
        return fullName.substr(colonPos + 1);
    }

    return fullName;
}

std::vector<const xmlpp::Element *>
RSM::ParsingCommon::findChildElementsWithNamespace(const xmlpp::Element *parent, const std::string &elementName,
                                                   const std::string &namespaceURI) {
    std::vector<const xmlpp::Element *> result;
    if (!parent) {
        return result;
    }

    auto children = parent->get_children();
    for (auto child : children) {
        auto element = dynamic_cast<const xmlpp::Element *>(child);
        if (element) {
            std::string fullName = element->get_name();
            std::string ns = element->get_namespace_uri();

            // Check namespace URI and extract local name
            if (ns == namespaceURI) {
                size_t colonPos = fullName.find(':');
                std::string localName = (colonPos != std::string::npos) ? fullName.substr(colonPos + 1) : fullName;

                if (localName == elementName) {
                    result.push_back(element);
                }
            }
        }
    }
    return result;
}

std::string RSM::ParsingCommon::trimString(const std::string &str) {
    // Trim leading and trailing whitespace
    auto start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) {
        return "";  // Whitespace only
    }

    auto end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}
