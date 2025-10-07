#pragma once

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

// Forward declarations
class XMLElement;
class XMLDocument;

/**
 * W3C SCXML B.2: XML DOM wrapper for libxml2 integration
 * Provides JavaScript-accessible DOM API for XML content
 */
class XMLElement {
public:
    explicit XMLElement(xmlNodePtr node);
    ~XMLElement() = default;

    // DOM API methods
    std::vector<std::shared_ptr<XMLElement>> getElementsByTagName(const std::string &tagName);
    std::string getAttribute(const std::string &attrName);
    std::string getTagName() const;

    // Internal access
    xmlNodePtr getNode() const {
        return node_;
    }

    xmlNodePtr node_;

public:
    static void findElementsByTagNameStatic(xmlNodePtr node, const std::string &tagName,
                                            std::vector<std::shared_ptr<XMLElement>> &result);

private:
    void findElementsByTagName(xmlNodePtr node, const std::string &tagName,
                               std::vector<std::shared_ptr<XMLElement>> &result);
};

/**
 * W3C SCXML B.2: XML Document wrapper
 * Root object for XML DOM tree
 */
class XMLDocument {
public:
    explicit XMLDocument(const std::string &xmlContent);
    ~XMLDocument();

    // DOM API methods
    std::vector<std::shared_ptr<XMLElement>> getElementsByTagName(const std::string &tagName);
    std::shared_ptr<XMLElement> getDocumentElement();

    bool isValid() const {
        return doc_ != nullptr;
    }

    std::string getErrorMessage() const {
        return errorMessage_;
    }

private:
    xmlDocPtr doc_;
    std::string errorMessage_;
};

}  // namespace RSM
