#pragma once

#include "common/Result.h"
#include <libxml++/libxml++.h>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace SCXML {
namespace Parsing {

/**
 * Namespace information structure for SCXML parsing
 */
struct NamespaceInfo {
    std::string prefix;  // Namespace prefix (e.g., "my")
    std::string uri;     // Namespace URI (e.g., "http://example.com/my-extensions")
    bool isStandard;     // True if this is the standard SCXML namespace
    bool isExtension;    // True if this is a recognized extension namespace

    NamespaceInfo() = default;

    NamespaceInfo(const std::string &p, const std::string &u, bool std = false, bool ext = false)
        : prefix(p), uri(u), isStandard(std), isExtension(ext) {}
};

/**
 * Extension element information for custom namespace elements
 */
struct ExtensionElement {
    std::string namespaceUri;
    std::string localName;
    std::string qualifiedName;
    std::map<std::string, std::string> attributes;
    std::string textContent;
    std::vector<ExtensionElement> children;

    ExtensionElement() = default;

    ExtensionElement(const std::string &ns, const std::string &local, const std::string &qualified)
        : namespaceUri(ns), localName(local), qualifiedName(qualified) {}
};

/**
 * Module reference information for external SCXML inclusion
 */
struct ModuleReference {
    std::string id;    // Module identifier
    std::string src;   // Source path/URL
    std::string type;  // Module type (scxml, data, etc.)
    std::map<std::string, std::string> parameters;
    bool isInline;              // True if module content is inline
    std::string inlineContent;  // Inline SCXML content if applicable

    ModuleReference() : isInline(false) {}

    ModuleReference(const std::string &moduleId, const std::string &sourcePath, const std::string &moduleType = "scxml")
        : id(moduleId), src(sourcePath), type(moduleType), isInline(false) {}
};

/**
 * Advanced XML parser with namespace awareness and modularity support
 * Extends standard SCXML parsing to handle namespaces and external modules
 */
class NamespaceAwareParser {
public:
    // Standard SCXML namespace URI
    static const std::string SCXML_NAMESPACE;

    // Common extension namespace URIs
    static const std::string SCXML_DATAMODEL_NAMESPACE;
    static const std::string SCXML_EXECUTABLE_NAMESPACE;

    NamespaceAwareParser();
    ~NamespaceAwareParser();

    /**
     * Register a namespace prefix and URI
     */
    SCXML::Common::Result<void> registerNamespace(const std::string &prefix, const std::string &uri,
                                                  bool isExtension = false);

    /**
     * Parse SCXML document with namespace awareness
     */
    SCXML::Common::Result<void> parseDocument(const std::string &scxmlContent);

    /**
     * Parse SCXML document from file with namespace awareness
     */
    SCXML::Common::Result<void> parseDocumentFromFile(const std::string &filePath);

    /**
     * Get all registered namespaces
     */
    const std::map<std::string, NamespaceInfo> &getNamespaces() const;

    /**
     * Get all discovered module references
     */
    const std::vector<ModuleReference> &getModuleReferences() const;

    /**
     * Get all extension elements found during parsing
     */
    const std::vector<ExtensionElement> &getExtensionElements() const;

    /**
     * Check if an element belongs to standard SCXML namespace
     */
    bool isStandardElement(const xmlpp::Element *element) const;

    /**
     * Check if an element is from an extension namespace
     */
    bool isExtensionElement(const xmlpp::Element *element) const;

    /**
     * Get namespace info for an element
     */
    SCXML::Common::Result<NamespaceInfo> getElementNamespace(const xmlpp::Element *element) const;

    /**
     * Resolve namespace prefix to full URI
     */
    SCXML::Common::Result<std::string> resolveNamespacePrefix(const std::string &prefix) const;

    /**
     * Extract module references from parsed document
     */
    SCXML::Common::Result<void> extractModuleReferences();

    /**
     * Extract extension elements from parsed document
     */
    SCXML::Common::Result<void> extractExtensionElements();

    /**
     * Validate namespace declarations and usage
     */
    SCXML::Common::Result<void> validateNamespaces() const;

    /**
     * Get the root SCXML element
     */
    xmlpp::Element *getRootElement() const;

    /**
     * Clear all parsing data
     */
    void clear();

private:
    std::unique_ptr<xmlpp::DomParser> parser_;
    std::map<std::string, NamespaceInfo> namespaces_;
    std::vector<ModuleReference> moduleReferences_;
    std::vector<ExtensionElement> extensionElements_;

    /**
     * Initialize standard SCXML namespaces
     */
    void initializeStandardNamespaces();

    /**
     * Extract namespace declarations from root element
     */
    SCXML::Common::Result<void> extractNamespaceDeclarations(xmlpp::Element *rootElement);

    /**
     * Process element with namespace awareness
     */
    SCXML::Common::Result<void> processElement(xmlpp::Element *element);

    /**
     * Process state element with src attribute for module inclusion
     */
    SCXML::Common::Result<void> processStateWithSrc(xmlpp::Element *stateElement);

    /**
     * Process invoke element for external module invocation
     */
    SCXML::Common::Result<void> processInvokeElement(xmlpp::Element *invokeElement);

    /**
     * Process data element with src attribute for external data inclusion
     */
    SCXML::Common::Result<void> processDataWithSrc(xmlpp::Element *dataElement);

    /**
     * Process extension element from non-standard namespace
     */
    SCXML::Common::Result<void> processExtensionElement(xmlpp::Element *element);

    /**
     * Convert XML element to ExtensionElement structure
     */
    ExtensionElement convertToExtensionElement(xmlpp::Element *element);

    /**
     * Get qualified name for element (prefix:localname)
     */
    std::string getQualifiedName(xmlpp::Element *element) const;

    /**
     * Get namespace URI for element
     */
    std::string getElementNamespaceUri(const xmlpp::Element *element) const;

    /**
     * Recursively process all child elements
     */
    SCXML::Common::Result<void> processChildElements(xmlpp::Element *parentElement);
};

/**
 * Extension handler interface for processing custom namespace elements
 */
class IExtensionHandler {
public:
    virtual ~IExtensionHandler() = default;

    /**
     * Check if this handler can process the given namespace
     */
    virtual bool canHandle(const std::string &namespaceUri) const = 0;

    /**
     * Process extension element
     */
    virtual SCXML::Common::Result<void> processExtensionElement(const ExtensionElement &element) = 0;

    /**
     * Generate code for extension element
     */
    virtual SCXML::Common::Result<std::string> generateCode(const ExtensionElement &element) const = 0;

    /**
     * Get supported namespace URIs
     */
    virtual std::vector<std::string> getSupportedNamespaces() const = 0;
};

/**
 * Registry for extension handlers
 */
class ExtensionHandlerRegistry {
public:
    static ExtensionHandlerRegistry &getInstance();

    /**
     * Register an extension handler
     */
    void registerHandler(std::shared_ptr<IExtensionHandler> handler);

    /**
     * Get handler for namespace URI
     */
    std::shared_ptr<IExtensionHandler> getHandler(const std::string &namespaceUri) const;

    /**
     * Get all registered handlers
     */
    std::vector<std::shared_ptr<IExtensionHandler>> getAllHandlers() const;

    /**
     * Check if namespace is supported by any handler
     */
    bool isSupported(const std::string &namespaceUri) const;

private:
    ExtensionHandlerRegistry() = default;
    std::map<std::string, std::shared_ptr<IExtensionHandler>> handlers_;
    mutable std::mutex handlerMutex_;
};

}  // namespace Parsing
}  // namespace SCXML