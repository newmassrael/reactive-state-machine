#include "parsing/NamespaceAwareParser.h"
#include "common/Logger.h"
#include <algorithm>
#include <fstream>
#include <mutex>
#include <sstream>

namespace SCXML {
namespace Parsing {

// Standard namespace URIs
const std::string NamespaceAwareParser::SCXML_NAMESPACE = "http://www.w3.org/2005/07/scxml";
const std::string NamespaceAwareParser::SCXML_DATAMODEL_NAMESPACE = "http://www.w3.org/2005/07/scxml/datamodel";
const std::string NamespaceAwareParser::SCXML_EXECUTABLE_NAMESPACE = "http://www.w3.org/2005/07/scxml/executable";

NamespaceAwareParser::NamespaceAwareParser() : parser_(std::make_unique<xmlpp::DomParser>()) {
    initializeStandardNamespaces();
}

NamespaceAwareParser::~NamespaceAwareParser() = default;

void NamespaceAwareParser::initializeStandardNamespaces() {
    // Register standard SCXML namespace
    namespaces_[""] = NamespaceInfo("", SCXML_NAMESPACE, true, false);
    namespaces_["scxml"] = NamespaceInfo("scxml", SCXML_NAMESPACE, true, false);

    // Register standard extension namespaces
    namespaces_["datamodel"] = NamespaceInfo("datamodel", SCXML_DATAMODEL_NAMESPACE, false, true);
    namespaces_["exec"] = NamespaceInfo("exec", SCXML_EXECUTABLE_NAMESPACE, false, true);
}

SCXML::Common::Result<void> NamespaceAwareParser::registerNamespace(const std::string &prefix, const std::string &uri,
                                                                    bool isExtension) {
    if (uri.empty()) {
        return SCXML::Common::Result<void>::failure("Namespace URI cannot be empty");
    }

    try {
        bool isStandard = (uri == SCXML_NAMESPACE);
        namespaces_[prefix] = NamespaceInfo(prefix, uri, isStandard, isExtension);

        SCXML::Common::Logger::info("NamespaceAwareParser: Registered namespace: prefix='" + prefix + "', uri='" + uri + "'");

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to register namespace: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> NamespaceAwareParser::parseDocument(const std::string &scxmlContent) {
    if (scxmlContent.empty()) {
        return SCXML::Common::Result<void>::failure("SCXML content is empty");
    }

    try {
        // Clear previous parsing data
        clear();

        // Parse XML content
        parser_->parse_memory(scxmlContent);

        if (!parser_->get_document()) {
            return SCXML::Common::Result<void>::failure("Failed to create XML document");
        }

        // Get root element
        xmlpp::Element *rootElement = parser_->get_document()->get_root_node();
        if (!rootElement) {
            return SCXML::Common::Result<void>::failure("No root element found");
        }

        // Extract namespace declarations
        auto nsResult = extractNamespaceDeclarations(rootElement);
        if (!nsResult.isSuccess()) {
            return nsResult;
        }

        // Process all elements with namespace awareness
        auto processResult = processElement(rootElement);
        if (!processResult.isSuccess()) {
            return processResult;
        }

        // Extract module references and extension elements
        auto moduleResult = extractModuleReferences();
        if (!moduleResult.isSuccess()) {
            return moduleResult;
        }

        auto extensionResult = extractExtensionElements();
        if (!extensionResult.isSuccess()) {
            return extensionResult;
        }

        // Validate namespace usage
        auto validateResult = validateNamespaces();
        if (!validateResult.isSuccess()) {
            return validateResult;
        }

        SCXML::Common::Logger::info("NamespaceAwareParser: Document parsed successfully with namespace awareness");
        return SCXML::Common::Result<void>::success();

    } catch (const xmlpp::exception &e) {
        return SCXML::Common::Result<void>::failure("XML parsing error: " + std::string(e.what()));
    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Parsing error: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> NamespaceAwareParser::parseDocumentFromFile(const std::string &filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return SCXML::Common::Result<void>::failure("Cannot open file: " + filePath);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        return parseDocument(buffer.str());

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to read file: " + std::string(e.what()));
    }
}

const std::map<std::string, NamespaceInfo> &NamespaceAwareParser::getNamespaces() const {
    return namespaces_;
}

const std::vector<ModuleReference> &NamespaceAwareParser::getModuleReferences() const {
    return moduleReferences_;
}

const std::vector<ExtensionElement> &NamespaceAwareParser::getExtensionElements() const {
    return extensionElements_;
}

bool NamespaceAwareParser::isStandardElement(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string namespaceUri = getElementNamespaceUri(element);
    return namespaceUri == SCXML_NAMESPACE || namespaceUri.empty();
}

bool NamespaceAwareParser::isExtensionElement(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string namespaceUri = getElementNamespaceUri(element);

    // Check if namespace is registered as extension
    for (const auto &pair : namespaces_) {
        if (pair.second.uri == namespaceUri && pair.second.isExtension) {
            return true;
        }
    }

    // Non-standard namespace is considered extension
    return !namespaceUri.empty() && namespaceUri != SCXML_NAMESPACE;
}

SCXML::Common::Result<NamespaceInfo> NamespaceAwareParser::getElementNamespace(const xmlpp::Element *element) const {
    if (!element) {
        return SCXML::Common::Result<NamespaceInfo>::failure("Element is null");
    }

    std::string namespaceUri = getElementNamespaceUri(element);

    for (const auto &pair : namespaces_) {
        if (pair.second.uri == namespaceUri) {
            return SCXML::Common::Result<NamespaceInfo>::success(pair.second);
        }
    }

    return SCXML::Common::Result<NamespaceInfo>::failure("Namespace not found for element");
}

SCXML::Common::Result<std::string> NamespaceAwareParser::resolveNamespacePrefix(const std::string &prefix) const {
    auto it = namespaces_.find(prefix);
    if (it != namespaces_.end()) {
        return SCXML::Common::Result<std::string>::success(it->second.uri);
    }

    return SCXML::Common::Result<std::string>::failure("Namespace prefix not found: " + prefix);
}

xmlpp::Element *NamespaceAwareParser::getRootElement() const {
    if (!parser_->get_document()) {
        return nullptr;
    }
    return parser_->get_document()->get_root_node();
}

void NamespaceAwareParser::clear() {
    moduleReferences_.clear();
    extensionElements_.clear();
    // Keep registered namespaces but clear document-specific data
}

SCXML::Common::Result<void> NamespaceAwareParser::extractNamespaceDeclarations(xmlpp::Element *rootElement) {
    if (!rootElement) {
        return SCXML::Common::Result<void>::failure("Root element is null");
    }

    try {
        // Get namespace declarations from root element
        auto attributes = rootElement->get_attributes();
        for (const auto &attr : attributes) {
            std::string attrName = attr->get_name();
            std::string attrValue = attr->get_value();

            if (attrName == "xmlns") {
                // Default namespace
                registerNamespace("", attrValue);
            } else if (attrName.substr(0, 6) == "xmlns:") {
                // Prefixed namespace
                std::string prefix = attrName.substr(6);
                registerNamespace(prefix, attrValue, true);
            }
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to extract namespace declarations: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> NamespaceAwareParser::processElement(xmlpp::Element *element) {
    if (!element) {
        return SCXML::Common::Result<void>::success();
    }

    try {
        std::string elementName = element->get_name();

        // Process based on element type and namespace
        if (isStandardElement(element)) {
            // Handle standard SCXML elements with potential src attributes
            if (elementName == "state" || elementName == "final") {
                processStateWithSrc(element);
            } else if (elementName == "invoke") {
                processInvokeElement(element);
            } else if (elementName == "data") {
                processDataWithSrc(element);
            }
        } else if (isExtensionElement(element)) {
            // Handle extension elements
            processExtensionElement(element);
        }

        // Process child elements recursively
        return processChildElements(element);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to process element: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> NamespaceAwareParser::processStateWithSrc(xmlpp::Element *stateElement) {
    try {
        auto srcAttr = stateElement->get_attribute("src");
        if (srcAttr) {
            std::string src = srcAttr->get_value();
            std::string id = "";

            auto idAttr = stateElement->get_attribute("id");
            if (idAttr) {
                id = idAttr->get_value();
            }

            ModuleReference moduleRef(id.empty() ? "state_module_" + std::to_string(moduleReferences_.size()) : id, src,
                                      "scxml");
            moduleReferences_.push_back(moduleRef);

            SCXML::Common::Logger::info("NamespaceAwareParser: Found state module reference: " + src);
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to process state with src: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> NamespaceAwareParser::processInvokeElement(xmlpp::Element *invokeElement) {
    try {
        auto srcAttr = invokeElement->get_attribute("src");
        auto typeAttr = invokeElement->get_attribute("type");
        auto idAttr = invokeElement->get_attribute("id");

        if (srcAttr) {
            std::string src = srcAttr->get_value();
            std::string type = typeAttr ? typeAttr->get_value() : "scxml";
            std::string id =
                idAttr ? idAttr->get_value() : ("invoke_module_" + std::to_string(moduleReferences_.size()));

            ModuleReference moduleRef(id, src, type);

            // Extract parameters
            auto children = invokeElement->get_children("param");
            for (const auto &child : children) {
                if (auto paramElement = dynamic_cast<xmlpp::Element *>(child)) {
                    auto nameAttr = paramElement->get_attribute("name");
                    auto exprAttr = paramElement->get_attribute("expr");

                    if (nameAttr) {
                        std::string paramName = nameAttr->get_value();
                        std::string paramValue = exprAttr ? exprAttr->get_value() : "";
                        moduleRef.parameters[paramName] = paramValue;
                    }
                }
            }

            moduleReferences_.push_back(moduleRef);

            SCXML::Common::Logger::info("NamespaceAwareParser: Found invoke module reference: " + src + " (type: " + type + ")");
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to process invoke element: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> NamespaceAwareParser::processDataWithSrc(xmlpp::Element *dataElement) {
    try {
        auto srcAttr = dataElement->get_attribute("src");
        if (srcAttr) {
            std::string src = srcAttr->get_value();
            std::string id = "";

            auto idAttr = dataElement->get_attribute("id");
            if (idAttr) {
                id = idAttr->get_value();
            }

            ModuleReference moduleRef(id.empty() ? "data_module_" + std::to_string(moduleReferences_.size()) : id, src,
                                      "data");
            moduleReferences_.push_back(moduleRef);

            SCXML::Common::Logger::info("NamespaceAwareParser: Found data module reference: " + src);
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to process data with src: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> NamespaceAwareParser::processExtensionElement(xmlpp::Element *element) {
    try {
        ExtensionElement extElement = convertToExtensionElement(element);
        extensionElements_.push_back(extElement);

        SCXML::Common::Logger::info("NamespaceAwareParser: Found extension element: " + extElement.qualifiedName + " (namespace: " + extElement.namespaceUri + ")");

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to process extension element: " + std::string(e.what()));
    }
}

ExtensionElement NamespaceAwareParser::convertToExtensionElement(xmlpp::Element *element) {
    ExtensionElement extElement(getElementNamespaceUri(element), element->get_name(), getQualifiedName(element));

    // Extract attributes
    auto attributes = element->get_attributes();
    for (const auto &attr : attributes) {
        extElement.attributes[attr->get_name()] = attr->get_value();
    }

    // Extract text content
    auto textNodes = element->get_children();
    std::stringstream textContent;
    for (const auto &node : textNodes) {
        if (auto textNode = dynamic_cast<const xmlpp::TextNode *>(node)) {
            textContent << textNode->get_content();
        }
    }
    extElement.textContent = textContent.str();

    // Extract child extension elements recursively
    for (const auto &child : textNodes) {
        if (auto childElement = dynamic_cast<xmlpp::Element *>(child)) {
            if (isExtensionElement(childElement)) {
                extElement.children.push_back(convertToExtensionElement(childElement));
            }
        }
    }

    return extElement;
}

std::string NamespaceAwareParser::getQualifiedName(xmlpp::Element *element) const {
    std::string namespaceUri = getElementNamespaceUri(element);
    std::string localName = element->get_name();

    // Find namespace prefix
    for (const auto &pair : namespaces_) {
        if (pair.second.uri == namespaceUri && !pair.second.prefix.empty()) {
            return pair.second.prefix + ":" + localName;
        }
    }

    return localName;
}

std::string NamespaceAwareParser::getElementNamespaceUri(const xmlpp::Element *element) const {
    if (!element) {
        return "";
    }

    // Get namespace URI from element
    if (element->get_namespace_uri().empty()) {
        return SCXML_NAMESPACE;  // Default to standard SCXML namespace
    }

    return element->get_namespace_uri();
}

SCXML::Common::Result<void> NamespaceAwareParser::processChildElements(xmlpp::Element *parentElement) {
    if (!parentElement) {
        return SCXML::Common::Result<void>::success();
    }

    try {
        auto children = parentElement->get_children();
        for (const auto &child : children) {
            if (auto childElement = dynamic_cast<xmlpp::Element *>(child)) {
                auto result = processElement(childElement);
                if (!result.isSuccess()) {
                    return result;
                }
            }
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to process child elements: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> NamespaceAwareParser::extractModuleReferences() {
    // Module references are extracted during element processing
    // This method can be used for additional post-processing if needed

    SCXML::Common::Logger::info("NamespaceAwareParser: Extracted " + std::to_string(moduleReferences_.size()) + " module references");

    return SCXML::Common::Result<void>::success();
}

SCXML::Common::Result<void> NamespaceAwareParser::extractExtensionElements() {
    // Extension elements are extracted during element processing
    // This method can be used for additional post-processing if needed

    SCXML::Common::Logger::info("NamespaceAwareParser: Extracted " + std::to_string(extensionElements_.size()) + " extension elements");

    return SCXML::Common::Result<void>::success();
}

SCXML::Common::Result<void> NamespaceAwareParser::validateNamespaces() const {
    try {
        // Check for undefined namespace prefixes
        // This is a basic validation - can be extended

        bool hasStandardNamespace = false;
        for (const auto &pair : namespaces_) {
            if (pair.second.isStandard) {
                hasStandardNamespace = true;
                break;
            }
        }

        if (!hasStandardNamespace) {
            return SCXML::Common::Result<void>::failure("No standard SCXML namespace found");
        }

        SCXML::Common::Logger::info("NamespaceAwareParser: Namespace validation completed successfully");
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Namespace validation failed: " + std::string(e.what()));
    }
}

// ExtensionHandlerRegistry implementation
ExtensionHandlerRegistry &ExtensionHandlerRegistry::getInstance() {
    static ExtensionHandlerRegistry instance;
    return instance;
}

void ExtensionHandlerRegistry::registerHandler(std::shared_ptr<IExtensionHandler> handler) {
    if (!handler) {
        return;
    }

    std::lock_guard<std::mutex> lock(handlerMutex_);

    auto supportedNamespaces = handler->getSupportedNamespaces();
    for (const auto &namespaceUri : supportedNamespaces) {
        handlers_[namespaceUri] = handler;
    }
}

std::shared_ptr<IExtensionHandler> ExtensionHandlerRegistry::getHandler(const std::string &namespaceUri) const {
    std::lock_guard<std::mutex> lock(handlerMutex_);

    auto it = handlers_.find(namespaceUri);
    if (it != handlers_.end()) {
        return it->second;
    }

    return nullptr;
}

std::vector<std::shared_ptr<IExtensionHandler>> ExtensionHandlerRegistry::getAllHandlers() const {
    std::lock_guard<std::mutex> lock(handlerMutex_);

    std::vector<std::shared_ptr<IExtensionHandler>> result;
    std::set<std::shared_ptr<IExtensionHandler>> uniqueHandlers;

    for (const auto &pair : handlers_) {
        uniqueHandlers.insert(pair.second);
    }

    for (const auto &handler : uniqueHandlers) {
        result.push_back(handler);
    }

    return result;
}

bool ExtensionHandlerRegistry::isSupported(const std::string &namespaceUri) const {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    return handlers_.find(namespaceUri) != handlers_.end();
}

}  // namespace Parsing
}  // namespace SCXML