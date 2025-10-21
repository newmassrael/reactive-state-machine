// InvokeParser.cpp
#include "InvokeParser.h"
#include "common/Logger.h"
#include "parsing/ParsingCommon.h"
#include <libxml++/nodes/textnode.h>
#include <libxml/tree.h>

RSM::InvokeParser::InvokeParser(std::shared_ptr<RSM::NodeFactory> nodeFactory) : nodeFactory_(nodeFactory) {
    LOG_DEBUG("Creating invoke parser");
}

RSM::InvokeParser::~InvokeParser() {
    LOG_DEBUG("Destroying invoke parser");
}

std::shared_ptr<RSM::IInvokeNode> RSM::InvokeParser::parseInvokeNode(const xmlpp::Element *invokeElement) {
    if (!invokeElement) {
        LOG_WARN("Null invoke element");
        return nullptr;
    }

    // W3C SCXML 6.4: Parse id attribute if present, otherwise leave empty for runtime generation
    // Runtime generation uses "stateid.platformid" format for W3C compliance (test 224)
    std::string id;
    auto idAttr = invokeElement->get_attribute("id");
    if (idAttr) {
        id = idAttr->get_value();
    }
    // If no id attribute, leave empty - InvokeExecutor will generate W3C compliant ID at runtime

    // Create InvokeNode
    auto invokeNode = nodeFactory_->createInvokeNode(id);

    // Process type attribute
    auto typeAttr = invokeElement->get_attribute("type");
    auto typeExprAttr = invokeElement->get_attribute("typeexpr");
    if (typeAttr) {
        invokeNode->setType(typeAttr->get_value());
    } else if (typeExprAttr) {
        // W3C SCXML 1.0: Handle typeexpr attribute for dynamic type evaluation
        invokeNode->setTypeExpr(typeExprAttr->get_value());
    }

    // Process src attribute
    auto srcAttr = invokeElement->get_attribute("src");
    auto srcExprAttr = invokeElement->get_attribute("srcexpr");
    if (srcAttr) {
        invokeNode->setSrc(srcAttr->get_value());
    } else if (srcExprAttr) {
        // Store srcexpr attribute for runtime evaluation
        invokeNode->setSrcExpr(srcExprAttr->get_value());
        LOG_DEBUG("srcexpr attribute set: {}", srcExprAttr->get_value());
    }

    // Process idlocation attribute
    auto idLocationAttr = invokeElement->get_attribute("idlocation");
    if (idLocationAttr) {
        invokeNode->setIdLocation(idLocationAttr->get_value());
    }

    // Process namelist attribute
    auto namelistAttr = invokeElement->get_attribute("namelist");
    if (namelistAttr) {
        invokeNode->setNamelist(namelistAttr->get_value());
    }

    // Process autoforward attribute
    auto autoforwardAttr = invokeElement->get_attribute("autoforward");
    if (autoforwardAttr && autoforwardAttr->get_value() == "true") {
        invokeNode->setAutoForward(true);
    }

    // Parse param elements
    parseParamElements(invokeElement, invokeNode);

    // Parse content element
    parseContentElement(invokeElement, invokeNode);

    // Parse finalize element
    auto finalizeElement = ParsingCommon::findFirstChildElement(invokeElement, "finalize");
    if (finalizeElement) {
        parseFinalizeElement(finalizeElement, invokeNode);
    }

    LOG_DEBUG("Invoke node parsed successfully: {}", id);
    return invokeNode;
}

std::vector<std::shared_ptr<RSM::IInvokeNode>>
RSM::InvokeParser::parseInvokesInState(const xmlpp::Element *stateElement) {
    std::vector<std::shared_ptr<IInvokeNode>> invokeNodes;

    if (!stateElement) {
        LOG_WARN("Null state element");
        return invokeNodes;
    }

    auto invokeElements = ParsingCommon::findChildElements(stateElement, "invoke");
    LOG_DEBUG("Found {} invoke elements", invokeElements.size());

    for (auto invokeElement : invokeElements) {
        auto invokeNode = parseInvokeNode(invokeElement);
        if (invokeNode) {
            invokeNodes.push_back(invokeNode);
        }
    }

    return invokeNodes;
}

void RSM::InvokeParser::parseFinalizeElement(const xmlpp::Element *finalizeElement,
                                             std::shared_ptr<IInvokeNode> invokeNode) {
    if (!finalizeElement || !invokeNode) {
        return;
    }

    // W3C SCXML 6.4: Finalize can contain executable content (assign, script, log, etc.)
    // Serialize all child elements as SCXML for execution by ActionExecutor
    std::string finalizeContent;
    auto children = finalizeElement->get_children();
    for (auto child : children) {
        // Include both element nodes (assign, script, etc.) and text nodes
        if (auto element = dynamic_cast<const xmlpp::Element *>(child)) {
            // Serialize element node to SCXML string
            finalizeContent += "<" + element->get_name();

            // Add attributes
            auto attributes = element->get_attributes();
            if (!attributes.empty()) {
                for (auto attr : attributes) {
                    finalizeContent += " " + attr->get_name() + "=\"" + attr->get_value() + "\"";
                }
            }

            // Check if element has children
            auto elementChildren = element->get_children();
            bool hasChildren = false;
            for (auto ec : elementChildren) {
                if (dynamic_cast<const xmlpp::Element *>(ec) ||
                    (dynamic_cast<const xmlpp::TextNode *>(ec) &&
                     !dynamic_cast<const xmlpp::TextNode *>(ec)->get_content().empty())) {
                    hasChildren = true;
                    break;
                }
            }

            if (hasChildren) {
                finalizeContent += ">";
                // Recursively add children (simplified - only text for now)
                for (auto ec : elementChildren) {
                    if (auto textNode = dynamic_cast<const xmlpp::TextNode *>(ec)) {
                        finalizeContent += textNode->get_content();
                    }
                }
                finalizeContent += "</" + element->get_name() + ">";
            } else {
                finalizeContent += "/>";
            }
        } else if (auto textNode = dynamic_cast<const xmlpp::TextNode *>(child)) {
            // Keep text nodes (whitespace, etc.)
            finalizeContent += textNode->get_content();
        }
    }

    invokeNode->setFinalize(finalizeContent);

    LOG_DEBUG("Finalize element parsed for invoke: {}, content: '{}'", invokeNode->getId(), finalizeContent);
}

void RSM::InvokeParser::parseParamElements(const xmlpp::Element *invokeElement,
                                           std::shared_ptr<IInvokeNode> invokeNode) {
    if (!invokeElement || !invokeNode) {
        return;
    }

    auto paramElements = ParsingCommon::findChildElements(invokeElement, "param");
    for (auto paramElement : paramElements) {
        std::string name, expr, location;

        auto nameAttr = paramElement->get_attribute("name");
        if (nameAttr) {
            name = nameAttr->get_value();
        }

        auto exprAttr = paramElement->get_attribute("expr");
        if (exprAttr) {
            expr = exprAttr->get_value();
        }

        auto locationAttr = paramElement->get_attribute("location");
        if (locationAttr) {
            location = locationAttr->get_value();
        }

        invokeNode->addParam(name, expr, location);

        LOG_DEBUG("Param parsed: name={}", name);
    }
}

std::vector<std::shared_ptr<RSM::IDataModelItem>>
RSM::InvokeParser::parseParamElementsAndCreateDataItems(const xmlpp::Element *invokeElement,
                                                        std::shared_ptr<IInvokeNode> invokeNode) {
    std::vector<std::shared_ptr<IDataModelItem>> dataItems;

    if (!invokeElement || !invokeNode) {
        return dataItems;
    }

    auto paramElements = ParsingCommon::findChildElements(invokeElement, "param");
    for (auto paramElement : paramElements) {
        std::string name, expr, location;

        auto nameAttr = paramElement->get_attribute("name");
        if (nameAttr) {
            name = nameAttr->get_value();
        }

        auto exprAttr = paramElement->get_attribute("expr");
        if (exprAttr) {
            expr = exprAttr->get_value();
        }

        auto locationAttr = paramElement->get_attribute("location");
        if (locationAttr) {
            location = locationAttr->get_value();
        }

        // Remove parameter addition (deleted invokeNode->addParam call)
        // Already added via parseParamElements in parseInvokeNode

        // Create data model item
        if (!name.empty() && (!expr.empty() || !location.empty())) {
            auto dataItem = nodeFactory_->createDataModelItem(name, expr.empty() ? location : expr);
            if (dataItem) {
                dataItems.push_back(dataItem);
            }
        }

        LOG_DEBUG("Data item created for param: name={}", name);
    }

    return dataItems;
}

void RSM::InvokeParser::parseContentElement(const xmlpp::Element *invokeElement,
                                            std::shared_ptr<IInvokeNode> invokeNode) {
    if (!invokeElement || !invokeNode) {
        return;
    }

    auto contentElement = ParsingCommon::findFirstChildElement(invokeElement, "content");
    if (contentElement) {
        std::string content;

        auto exprAttr = contentElement->get_attribute("expr");
        if (exprAttr) {
            // W3C SCXML test 530: Store expr for dynamic evaluation during invoke execution
            std::string contentExpr = exprAttr->get_value();
            invokeNode->setContentExpr(contentExpr);
            LOG_DEBUG("Content element has expr attribute: '{}'", contentExpr);
            return;
        } else {
            // Serialize internal XML elements
            auto children = contentElement->get_children();
            for (auto child : children) {
                // Use libxml2 serialization for XML elements
                if (auto childElement = dynamic_cast<const xmlpp::Element *>(child)) {
                    // Get libxml2 node - use const_cast
                    _xmlNode *node = const_cast<_xmlNode *>(childElement->cobj());

                    // Create buffer
                    auto buf = xmlBufferCreate();
                    if (buf) {
                        // Serialize node to buffer (level 0 without indentation)
                        xmlNodeDump(buf, node->doc, node, 0, 0);

                        // Extract string from buffer
                        content += (const char *)xmlBufferContent(buf);

                        // Free buffer
                        xmlBufferFree(buf);
                    }
                } else if (auto textNode = dynamic_cast<const xmlpp::TextNode *>(child)) {
                    // Add text nodes as-is
                    content += textNode->get_content();
                }
            }
        }

        invokeNode->setContent(content);
        LOG_DEBUG("Content element parsed with serialized XML");
    }
}
