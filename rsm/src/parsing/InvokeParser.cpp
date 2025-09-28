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

    // id 속성 검색
    std::string id;
    auto idAttr = invokeElement->get_attribute("id");
    if (idAttr) {
        id = idAttr->get_value();
    } else {
        // id가 없으면 자동 생성
        id = "invoke_" + std::to_string(reinterpret_cast<uintptr_t>(invokeElement));
        LOG_DEBUG("Generated id: {}", id);
    }

    // InvokeNode 생성
    auto invokeNode = nodeFactory_->createInvokeNode(id);

    // type 속성 처리
    auto typeAttr = invokeElement->get_attribute("type");
    auto typeExprAttr = invokeElement->get_attribute("typeexpr");
    if (typeAttr) {
        invokeNode->setType(typeAttr->get_value());
    } else if (typeExprAttr) {
        // typeexpr 속성은 실행 시간에 평가됨 - 여기서는 표시만
        LOG_DEBUG("typeexpr attribute present: {}", typeExprAttr->get_value());
    }

    // src 속성 처리
    auto srcAttr = invokeElement->get_attribute("src");
    auto srcExprAttr = invokeElement->get_attribute("srcexpr");
    if (srcAttr) {
        invokeNode->setSrc(srcAttr->get_value());
    } else if (srcExprAttr) {
        // srcexpr 속성은 실행 시간에 평가됨 - 여기서는 표시만
        LOG_DEBUG("srcexpr attribute present: {}", srcExprAttr->get_value());
    }

    // idlocation 속성 처리
    auto idLocationAttr = invokeElement->get_attribute("idlocation");
    if (idLocationAttr) {
        invokeNode->setIdLocation(idLocationAttr->get_value());
    }

    // namelist 속성 처리
    auto namelistAttr = invokeElement->get_attribute("namelist");
    if (namelistAttr) {
        invokeNode->setNamelist(namelistAttr->get_value());
    }

    // autoforward 속성 처리
    auto autoforwardAttr = invokeElement->get_attribute("autoforward");
    if (autoforwardAttr && autoforwardAttr->get_value() == "true") {
        invokeNode->setAutoForward(true);
    }

    // param 요소 파싱
    parseParamElements(invokeElement, invokeNode);

    // content 요소 파싱
    parseContentElement(invokeElement, invokeNode);

    // finalize 요소 파싱
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

    // get_child_text() 대신 텍스트 노드를 직접 찾아야 함
    std::string finalizeContent;
    auto children = finalizeElement->get_children();
    for (auto child : children) {
        // TextNode 타입인지 확인하고 내용 추출
        if (auto textNode = dynamic_cast<const xmlpp::TextNode *>(child)) {
            finalizeContent += textNode->get_content();
        }
    }

    invokeNode->setFinalize(finalizeContent);

    LOG_DEBUG("Finalize element parsed for invoke: {}", invokeNode->getId());
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

        // 파라미터 추가 부분 제거 (invokeNode->addParam 호출 삭제)
        // 이미 parseInvokeNode에서 parseParamElements를 통해 추가되었음

        // 데이터 모델 아이템 생성
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
            content = exprAttr->get_value();
        } else {
            // 내부 XML 요소를 직렬화
            auto children = contentElement->get_children();
            for (auto child : children) {
                // XML 요소인 경우 libxml2의 직렬화 기능 사용
                if (auto childElement = dynamic_cast<const xmlpp::Element *>(child)) {
                    // libxml2 노드 가져오기 - const_cast 사용
                    _xmlNode *node = const_cast<_xmlNode *>(childElement->cobj());

                    // 버퍼 생성
                    auto buf = xmlBufferCreate();
                    if (buf) {
                        // 노드를 버퍼에 직렬화 (들여쓰기 없이 0 레벨)
                        xmlNodeDump(buf, node->doc, node, 0, 0);

                        // 버퍼에서 문자열 추출
                        content += (const char *)buf->content;

                        // 버퍼 해제
                        xmlBufferFree(buf);
                    }
                } else if (auto textNode = dynamic_cast<const xmlpp::TextNode *>(child)) {
                    // 텍스트 노드는 그대로 추가
                    content += textNode->get_content();
                }
            }
        }

        invokeNode->setContent(content);
        LOG_DEBUG("Content element parsed with serialized XML");
    }
}
