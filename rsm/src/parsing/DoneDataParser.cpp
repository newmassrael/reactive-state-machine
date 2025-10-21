#include "parsing/DoneDataParser.h"
#include "common/Logger.h"
#include "parsing/ParsingCommon.h"

RSM::DoneDataParser::DoneDataParser(std::shared_ptr<NodeFactory> factory) : factory_(factory) {
    LOG_DEBUG("Creating DoneData parser");
}

bool RSM::DoneDataParser::parseDoneData(const xmlpp::Element *doneDataElement, IStateNode *stateNode) {
    if (!doneDataElement || !stateNode) {
        LOG_ERROR("Null doneData element or state node");
        return false;
    }

    LOG_DEBUG("Parsing <donedata> for state {}", stateNode->getId());

    bool hasContent = false;
    bool hasParam = false;

    // Parse <content> element
    const xmlpp::Element *contentElement = ParsingCommon::findFirstChildElement(doneDataElement, "content");
    if (contentElement) {
        hasContent = parseContent(contentElement, stateNode);
        LOG_DEBUG("Found <content> element: {}", (hasContent ? "valid" : "invalid"));
    }

    // Parse <param> elements
    auto paramElements = ParsingCommon::findChildElements(doneDataElement, "param");
    for (auto *paramElement : paramElements) {
        if (parseParam(paramElement, stateNode)) {
            hasParam = true;
        }
    }

    LOG_DEBUG("Found {} <param> elements: {}", paramElements.size(), (hasParam ? "valid" : "invalid"));

    // <content> and <param> cannot be used together
    if (hasContent && hasParam) {
        LOG_ERROR("<content> and <param> cannot be used together in <donedata>");

        // Clear conflict to satisfy XOR condition
        // Both content and param are set, remove one
        if (hasContent) {
            // Keep content, remove param
            stateNode->clearDoneDataParams();
            hasParam = false;
        } else {
            // Keep param, remove content
            stateNode->setDoneDataContent("");
            hasContent = false;
        }

        // Return false to propagate error to SCXMLParser
        return false;
    }

    return hasContent || hasParam;
}

bool RSM::DoneDataParser::parseContent(const xmlpp::Element *contentElement, IStateNode *stateNode) {
    if (!contentElement || !stateNode) {
        LOG_ERROR("Null content element or state node");
        return false;
    }

    // Check expr attribute
    auto exprAttr = contentElement->get_attribute("expr");
    std::string exprValue;
    if (exprAttr) {
        exprValue = exprAttr->get_value();
        LOG_DEBUG("Found 'expr' attribute: {}", exprValue);
    }

    // Check content
    std::string textContent;
    const xmlpp::Node *childNode = contentElement->get_first_child();
    if (childNode) {
        // Try type conversion to TextNode
        const xmlpp::TextNode *textNode = dynamic_cast<const xmlpp::TextNode *>(childNode);
        if (textNode) {
            textContent = textNode->get_content();
            textContent = ParsingCommon::trimString(textContent);
            LOG_DEBUG("Found text content: {}",
                      (textContent.length() > 30 ? textContent.substr(0, 27) + "..." : textContent));
        }
    }

    // expr and content cannot be used together
    if (!exprValue.empty() && !textContent.empty()) {
        LOG_ERROR("<content> cannot have both 'expr' attribute and child content");
        return false;
    }

    // Set expr or content
    if (!exprValue.empty()) {
        stateNode->setDoneDataContent(exprValue);
        return true;
    } else if (!textContent.empty()) {
        stateNode->setDoneDataContent(textContent);
        return true;
    }

    // Handle empty content
    stateNode->setDoneDataContent("");
    return true;
}

bool RSM::DoneDataParser::parseParam(const xmlpp::Element *paramElement, IStateNode *stateNode) {
    if (!paramElement || !stateNode) {
        LOG_ERROR("Null param element or state node");
        return false;
    }

    // name attribute (required)
    auto nameAttr = paramElement->get_attribute("name");
    if (!nameAttr) {
        LOG_ERROR("<param> element must have 'name' attribute");
        return false;
    }

    std::string nameValue = nameAttr->get_value();

    // Check expr and location attributes (only one can be used)
    auto exprAttr = paramElement->get_attribute("expr");
    auto locationAttr = paramElement->get_attribute("location");

    if (exprAttr && locationAttr) {
        LOG_ERROR("<param> cannot have both 'expr' and 'location' attributes");
        return false;
    }

    // Process location attribute (add param to donedata)
    if (locationAttr) {
        std::string locationValue = locationAttr->get_value();
        stateNode->addDoneDataParam(nameValue, locationValue);
        LOG_DEBUG("Added param: {} with location: {}", nameValue, locationValue);
        return true;
    }

    // Process expr attribute
    if (exprAttr) {
        std::string exprValue = exprAttr->get_value();
        // Simply convert expr value to location for use
        // More complex processing can be added as needed
        stateNode->addDoneDataParam(nameValue, exprValue);
        LOG_DEBUG("Added param: {} with expr: {}", nameValue, exprValue);
        return true;
    }

    LOG_ERROR("<param> must have either 'expr' or 'location' attribute");
    return false;
}
