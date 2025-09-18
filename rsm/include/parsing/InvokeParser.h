// InvokeParser.h
#pragma once

#include <memory>
#include <vector>
#include <libxml++/libxml++.h>
#include "model/IInvokeNode.h"
#include "factory/INodeFactory.h"


namespace RSM {

class InvokeParser
{
public:
    InvokeParser(std::shared_ptr<INodeFactory> nodeFactory);
    ~InvokeParser();

    // invoke 요소 파싱
    std::shared_ptr<IInvokeNode> parseInvokeNode(const xmlpp::Element *invokeElement);

    // 특정 상태 내의 모든 invoke 요소 파싱
    std::vector<std::shared_ptr<IInvokeNode>> parseInvokesInState(const xmlpp::Element *stateElement);

    // param 요소를 파싱하고 생성된 DataModelItem 반환
    std::vector<std::shared_ptr<IDataModelItem>> parseParamElementsAndCreateDataItems(
        const xmlpp::Element *invokeElement,
        std::shared_ptr<IInvokeNode> invokeNode);

private:
    std::shared_ptr<INodeFactory> nodeFactory_;

    void parseFinalizeElement(const xmlpp::Element *finalizeElement, std::shared_ptr<IInvokeNode> invokeNode);
    void parseParamElements(const xmlpp::Element *invokeElement, std::shared_ptr<IInvokeNode> invokeNode);
    void parseContentElement(const xmlpp::Element *invokeElement, std::shared_ptr<IInvokeNode> invokeNode);
};


}  // namespace RSM