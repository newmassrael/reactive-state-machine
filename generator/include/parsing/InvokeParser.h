// InvokeParser.h
#pragma once

#include "model/INodeFactory.h"
#include "model/IInvokeNode.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <vector>

using SCXML::Model::IDataModelItem;


namespace SCXML {

namespace Model {
class IInvokeNode;
class IDataModelItem;
}

namespace Parsing {

class InvokeParser {
public:
    InvokeParser(::std::shared_ptr<::SCXML::Model::INodeFactory> nodeFactory);
    ~InvokeParser();

    // Parse invoke element
    ::std::shared_ptr<::SCXML::Model::IInvokeNode> parseInvokeNode(const xmlpp::Element *invokeElement);

    // Parse all invoke elements within a specific state
    ::std::vector<::std::shared_ptr<::SCXML::Model::IInvokeNode>>
    parseInvokesInState(const xmlpp::Element *stateElement);

    // Parse param elements and return generated DataModelItems
    ::std::vector<::std::shared_ptr<::SCXML::Model::IDataModelItem>>
    parseParamElementsAndCreateDataItems(const xmlpp::Element *invokeElement,
                                         ::std::shared_ptr<::SCXML::Model::IInvokeNode> invokeNode);

private:
    ::std::shared_ptr<::SCXML::Model::INodeFactory> nodeFactory_;

    void parseFinalizeElement(const xmlpp::Element *finalizeElement,
                              ::std::shared_ptr<::SCXML::Model::IInvokeNode> invokeNode);
    void parseParamElements(const xmlpp::Element *invokeElement,
                            ::std::shared_ptr<::SCXML::Model::IInvokeNode> invokeNode);
    void parseContentElement(const xmlpp::Element *invokeElement,
                             ::std::shared_ptr<::SCXML::Model::IInvokeNode> invokeNode);
};

}  // namespace Parsing
}  // namespace SCXML
