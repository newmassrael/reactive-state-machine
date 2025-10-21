#include "parsing/DataModelParser.h"
#include "common/Logger.h"
#include <algorithm>
#include <libxml/tree.h>

RSM::DataModelParser::DataModelParser(std::shared_ptr<NodeFactory> nodeFactory) : nodeFactory_(nodeFactory) {
    LOG_DEBUG("Creating data model parser");
}

RSM::DataModelParser::~DataModelParser() {
    LOG_DEBUG("Destroying data model parser");
}

std::vector<std::shared_ptr<RSM::IDataModelItem>>
RSM::DataModelParser::parseDataModelNode(const xmlpp::Element *datamodelNode, const RSM::SCXMLContext &context) {
    std::vector<std::shared_ptr<IDataModelItem>> items;

    if (!datamodelNode) {
        LOG_WARN("Null datamodel node");
        return items;
    }

    LOG_DEBUG("Parsing datamodel node");

    // Parse data nodes
    auto dataNodes = datamodelNode->get_children("data");
    for (auto *node : dataNodes) {
        auto *dataElement = dynamic_cast<const xmlpp::Element *>(node);
        if (dataElement) {
            auto dataItem = parseDataModelItem(dataElement, context);
            if (dataItem) {
                items.push_back(dataItem);
                LOG_DEBUG("Added data model item: {}", dataItem->getId());
            }
        }
    }

    LOG_DEBUG("Parsed {} data model items", items.size());
    return items;
}

std::shared_ptr<RSM::IDataModelItem> RSM::DataModelParser::parseDataModelItem(const xmlpp::Element *dataNode,
                                                                              const RSM::SCXMLContext &context) {
    if (!dataNode) {
        LOG_WARN("Null data node");
        return nullptr;
    }

    auto idAttr = dataNode->get_attribute("id");
    if (!idAttr) {
        LOG_WARN("Data node missing id attribute");
        return nullptr;
    }

    std::string id = idAttr->get_value();
    std::string expr;

    auto exprAttr = dataNode->get_attribute("expr");
    if (exprAttr) {
        expr = exprAttr->get_value();
    }

    LOG_DEBUG("Parsing data model item: {}", id);
    auto dataItem = nodeFactory_->createDataModelItem(id, expr);

    // Process src attribute
    auto srcAttr = dataNode->get_attribute("src");
    if (srcAttr) {
        std::string src = srcAttr->get_value();
        dataItem->setSrc(src);
        LOG_DEBUG("Source URL: {}", src);

        loadExternalContent(src, dataItem);

        // According to SCXML standard, src, expr, content are mutually exclusive
        if (exprAttr) {
            LOG_WARN("Data element cannot have both 'src' and 'expr' attributes: {}", id);
            // Error handling: Store both even though it's an error per SCXML standard
        }

        // Check if content exists (has child nodes)
        if (dataNode->get_first_child()) {
            LOG_WARN("Data element cannot have both 'src' attribute and content: {}", id);
            // Error handling: Store both even though it's an error per SCXML standard
        }
    }

    // Process existing attributes
    auto typeAttr = dataNode->get_attribute("type");
    if (typeAttr) {
        dataItem->setType(typeAttr->get_value());
        LOG_DEBUG("Type: {}", typeAttr->get_value());
    } else if (!context.getDatamodelType().empty()) {
        // Use context datamodel type if node has no type
        dataItem->setType(context.getDatamodelType());
        LOG_DEBUG("Using parent datamodel type: {}", context.getDatamodelType());
    }

    auto scopeAttr = dataNode->get_attribute("code:scope");
    if (!scopeAttr) {
        // Try without namespace
        scopeAttr = dataNode->get_attribute("scope");
    }

    if (scopeAttr) {
        dataItem->setScope(scopeAttr->get_value());
        LOG_DEBUG("Scope: {}", scopeAttr->get_value());
    }

    // Process additional attributes
    auto attributes = dataNode->get_attributes();
    for (auto *attr : attributes) {
        auto *xmlAttr = dynamic_cast<const xmlpp::Attribute *>(attr);
        if (xmlAttr) {
            std::string name = xmlAttr->get_name();
            std::string value = xmlAttr->get_value();

            // Skip already processed attributes
            if (name != "id" && name != "expr" && name != "type" && name != "code:scope" && name != "scope" &&
                name != "src") {
                dataItem->setAttribute(name, value);
                LOG_DEBUG("Added attribute: {} = {}", name, value);
            }
        }
    }

    // Process content only if src exists without expr or content
    if (!srcAttr || (srcAttr && !exprAttr && !dataNode->get_first_child())) {
        // Process content
        parseDataContent(dataNode, dataItem);
    }

    LOG_DEBUG("Data model item parsed successfully");
    return dataItem;
}

void RSM::DataModelParser::parseDataContent(const xmlpp::Element *dataNode, std::shared_ptr<IDataModelItem> dataItem) {
    if (!dataNode || !dataItem) {
        return;
    }

    auto children = dataNode->get_children();
    if (children.empty()) {
        return;
    }

    for (auto *child : children) {
        // Process text nodes
        auto *textNode = dynamic_cast<const xmlpp::TextNode *>(child);
        if (textNode) {
            std::string content = textNode->get_content();
            if (!content.empty()) {
                // Ignore whitespace-only content
                bool onlyWhitespace = true;
                for (char c : content) {
                    if (!std::isspace(c)) {
                        onlyWhitespace = false;
                        break;
                    }
                }

                if (!onlyWhitespace) {
                    dataItem->setContent(content);
                    LOG_DEBUG("Added text content");
                }
            }
            continue;  // Continue to next node after processing text node
        }

        // Process CDATA sections
        auto *cdataNode = dynamic_cast<const xmlpp::CdataNode *>(child);
        if (cdataNode) {
            std::string content = cdataNode->get_content();
            dataItem->setContent(content);
            LOG_DEBUG("Added CDATA content: {}", content);
            continue;  // Continue to next node after processing CDATA node
        }

        // Process element nodes (XML content)
        auto *elementNode = dynamic_cast<const xmlpp::Element *>(child);
        if (elementNode) {
            // W3C SCXML B.2 test 557: Serialize full XML content for DOM parsing
            // Use libxml2 to serialize the XML node
            xmlNodePtr xmlNode = const_cast<xmlNodePtr>(elementNode->cobj());
            xmlBufferPtr buffer = xmlBufferCreate();
            xmlNodeDump(buffer, xmlNode->doc, xmlNode, 0, 1);
            std::string xmlContent = reinterpret_cast<const char *>(xmlBufferContent(buffer));
            xmlBufferFree(buffer);

            dataItem->setContent(xmlContent);
            LOG_DEBUG("Added XML content of type: {}", elementNode->get_name());
        }
    }
}

std::vector<std::shared_ptr<RSM::IDataModelItem>>
RSM::DataModelParser::parseDataModelInState(const xmlpp::Element *stateNode, const RSM::SCXMLContext &context) {
    std::vector<std::shared_ptr<IDataModelItem>> items;

    if (!stateNode) {
        LOG_WARN("Null state node");
        return items;
    }

    LOG_DEBUG("Parsing datamodel in state");

    // Find datamodel element
    auto datamodelNode = stateNode->get_first_child("datamodel");
    if (datamodelNode) {
        auto *element = dynamic_cast<const xmlpp::Element *>(datamodelNode);
        if (element) {
            // Pass context
            auto stateItems = parseDataModelNode(element, context);
            items.insert(items.end(), stateItems.begin(), stateItems.end());
        }
    }

    LOG_DEBUG("Found {} data items in state", items.size());
    return items;
}

std::string RSM::DataModelParser::extractDataModelType(const xmlpp::Element *datamodelNode) const {
    if (!datamodelNode) {
        return "";
    }

    auto typeAttr = datamodelNode->get_attribute("type");
    if (typeAttr) {
        return typeAttr->get_value();
    }

    return "";
}

bool RSM::DataModelParser::isDataModelItem(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();
    return matchNodeName(nodeName, "data");
}

bool RSM::DataModelParser::matchNodeName(const std::string &nodeName, const std::string &searchName) const {
    // Exact match
    if (nodeName == searchName) {
        return true;
    }

    // With namespace (e.g., "code:data")
    size_t colonPos = nodeName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < nodeName.length()) {
        std::string localName = nodeName.substr(colonPos + 1);
        return localName == searchName;
    }

    return false;
}

void RSM::DataModelParser::loadExternalContent(const std::string &src, std::shared_ptr<IDataModelItem> dataItem) {
    // This method handles loading data from external URLs
    // e.g., file system, HTTP requests, etc.

    LOG_DEBUG("Loading content from: {}", src);

    // Example: Load file content for file paths
    if (src.find("file://") == 0 || src.find("/") == 0 || src.find("./") == 0) {
        std::string filePath = src;
        if (src.find("file://") == 0) {
            filePath = src.substr(7);  // Remove "file://" prefix
        }

        try {
            // Load file content
            std::ifstream file(filePath);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                dataItem->setContent(buffer.str());
                LOG_DEBUG("Content loaded from file");
            } else {
                LOG_ERROR("Failed to open file: {}", filePath);
            }
        } catch (std::exception &e) {
            LOG_ERROR("Exception loading file: {}", e.what());
        }
    } else if (src.find("http://") == 0 || src.find("https://") == 0) {
        // HTTP requests require more complex implementation and external libraries are recommended
        // Implementation omitted here, log only
        LOG_WARN("HTTP loading not implemented: {}", src);
    } else {
        // Handle other protocols or relative paths
        LOG_WARN("Unsupported URL format: {}", src);
    }
}
