// InvokeParser.h
#pragma once

#include "factory/NodeFactory.h"
#include "model/IInvokeNode.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <vector>

namespace RSM {

class InvokeParser {
public:
    InvokeParser(std::shared_ptr<NodeFactory> nodeFactory);
    ~InvokeParser();

    // Parse invoke element
    std::shared_ptr<IInvokeNode> parseInvokeNode(const xmlpp::Element *invokeElement);

    // Parse all invoke elements within a specific state
    std::vector<std::shared_ptr<IInvokeNode>> parseInvokesInState(const xmlpp::Element *stateElement);

    // Parse param elements and return created DataModelItems
    std::vector<std::shared_ptr<IDataModelItem>>
    parseParamElementsAndCreateDataItems(const xmlpp::Element *invokeElement, std::shared_ptr<IInvokeNode> invokeNode);

private:
    std::shared_ptr<NodeFactory> nodeFactory_;

    void parseFinalizeElement(const xmlpp::Element *finalizeElement, std::shared_ptr<IInvokeNode> invokeNode);
    void parseParamElements(const xmlpp::Element *invokeElement, std::shared_ptr<IInvokeNode> invokeNode);
    void parseContentElement(const xmlpp::Element *invokeElement, std::shared_ptr<IInvokeNode> invokeNode);
};

}  // namespace RSM