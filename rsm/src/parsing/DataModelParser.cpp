#include "parsing/DataModelParser.h"
#include "common/Logger.h"
#include <algorithm>
using namespace RSM;


DataModelParser::DataModelParser(std::shared_ptr<INodeFactory> nodeFactory)
    : nodeFactory_(nodeFactory)
{
    Logger::debug("DataModelParser::Constructor - Creating data model parser");
}


DataModelParser::~DataModelParser()
{
    Logger::debug("DataModelParser::Destructor - Destroying data model parser");
}


std::vector<std::shared_ptr<IDataModelItem>> DataModelParser::parseDataModelNode(const xmlpp::Element *datamodelNode, const SCXMLContext &context)
{
    std::vector<std::shared_ptr<IDataModelItem>> items;


    if (!datamodelNode)
    {
        Logger::warning("DataModelParser::parseDataModelNode() - Null datamodel node");
        return items;
    }


    Logger::debug("DataModelParser::parseDataModelNode() - Parsing datamodel node");


    // data 노드들 파싱
    auto dataNodes = datamodelNode->get_children("data");
    for (auto *node : dataNodes)
    {
        auto *dataElement = dynamic_cast<const xmlpp::Element *>(node);
        if (dataElement)
        {
            auto dataItem = parseDataModelItem(dataElement, context);
            if (dataItem)
            {
                items.push_back(dataItem);
                Logger::debug("DataModelParser::parseDataModelNode() - Added data model item: " + dataItem->getId());
            }
        }
    }


    Logger::debug("DataModelParser::parseDataModelNode() - Parsed " + std::to_string(items.size()) + " data model items");
    return items;
}


std::shared_ptr<IDataModelItem> DataModelParser::parseDataModelItem(const xmlpp::Element *dataNode, const SCXMLContext &context)
{
    if (!dataNode)
    {
        Logger::warning("DataModelParser::parseDataModelItem() - Null data node");
        return nullptr;
    }


    auto idAttr = dataNode->get_attribute("id");
    if (!idAttr)
    {
        Logger::warning("DataModelParser::parseDataModelItem() - Data node missing id attribute");
        return nullptr;
    }


    std::string id = idAttr->get_value();
    std::string expr;


    auto exprAttr = dataNode->get_attribute("expr");
    if (exprAttr)
    {
        expr = exprAttr->get_value();
    }


    Logger::debug("DataModelParser::parseDataModelItem() - Parsing data model item: " + id);
    auto dataItem = nodeFactory_->createDataModelItem(id, expr);


    // src 속성 처리 (추가된 부분)
    auto srcAttr = dataNode->get_attribute("src");
    if (srcAttr)
    {
        std::string src = srcAttr->get_value();
        dataItem->setSrc(src);
        Logger::debug("DataModelParser::parseDataModelItem() - Source URL: " + src);


        loadExternalContent(src, dataItem);


        // SCXML 표준에 따르면 src, expr, content는 상호 배타적
        if (exprAttr)
        {
            Logger::warning("DataModelParser::parseDataModelItem() - Data element cannot have both 'src' and 'expr' attributes: " + id);
            // 오류 처리: SCXML 표준에서는 오류지만, 일단 둘 다 저장
        }


        // 내용이 있는지 확인 (자식 노드가 있는지)
        if (dataNode->get_first_child())
        {
            Logger::warning("DataModelParser::parseDataModelItem() - Data element cannot have both 'src' attribute and content: " + id);
            // 오류 처리: SCXML 표준에서는 오류지만, 일단 둘 다 저장
        }
    }


    // 기존 속성 처리
    auto typeAttr = dataNode->get_attribute("type");
    if (typeAttr)
    {
        dataItem->setType(typeAttr->get_value());
        Logger::debug("DataModelParser::parseDataModelItem() - Type: " + typeAttr->get_value());
    }
    else if (!context.getDatamodelType().empty())
    {
        // 노드에 타입이 없으면 컨텍스트의 데이터 모델 타입 사용
        dataItem->setType(context.getDatamodelType());
        Logger::debug("DataModelParser::parseDataModelItem() - Using parent datamodel type: " + context.getDatamodelType());
    }


    auto scopeAttr = dataNode->get_attribute("code:scope");
    if (!scopeAttr)
    {
        // 네임스페이스 없이 시도
        scopeAttr = dataNode->get_attribute("scope");
    }


    if (scopeAttr)
    {
        dataItem->setScope(scopeAttr->get_value());
        Logger::debug("DataModelParser::parseDataModelItem() - Scope: " + scopeAttr->get_value());
    }


    // 추가 속성 처리
    auto attributes = dataNode->get_attributes();
    for (auto *attr : attributes)
    {
        auto *xmlAttr = dynamic_cast<const xmlpp::Attribute *>(attr);
        if (xmlAttr)
        {
            std::string name = xmlAttr->get_name();
            std::string value = xmlAttr->get_value();


            // 이미 처리한 속성은 건너뜀
            if (name != "id" && name != "expr" && name != "type" && name != "code:scope" && name != "scope" && name != "src") // src 추가
            {
                dataItem->setAttribute(name, value);
                Logger::debug("DataModelParser::parseDataModelItem() - Added attribute: " + name + " = " + value);
            }
        }
    }


    // src가 있으면서 expr이나 content가 없는 경우에만 내용 처리
    if (!srcAttr || (srcAttr && !exprAttr && !dataNode->get_first_child()))
    {
        // 내용(콘텐츠) 처리
        parseDataContent(dataNode, dataItem);
    }


    Logger::debug("DataModelParser::parseDataModelItem() - Data model item parsed successfully");
    return dataItem;
}


void DataModelParser::parseDataContent(const xmlpp::Element *dataNode, std::shared_ptr<IDataModelItem> dataItem)
{
    if (!dataNode || !dataItem)
    {
        return;
    }


    auto children = dataNode->get_children();
    if (children.empty())
    {
        return;
    }


    for (auto *child : children)
    {
        // 텍스트 노드 처리
        auto *textNode = dynamic_cast<const xmlpp::TextNode *>(child);
        if (textNode)
        {
            std::string content = textNode->get_content();
            if (!content.empty())
            {
                // 공백만 있는 내용은 무시
                bool onlyWhitespace = true;
                for (char c : content)
                {
                    if (!std::isspace(c))
                    {
                        onlyWhitespace = false;
                        break;
                    }
                }


                if (!onlyWhitespace)
                {
                    dataItem->setContent(content);
                    Logger::debug("DataModelParser::parseDataContent() - Added text content");
                }
            }
            continue; // 텍스트 노드 처리 후 다음 노드로
        }


        // CDATA 섹션 처리 (추가된 부분)
        auto *cdataNode = dynamic_cast<const xmlpp::CdataNode *>(child);
        if (cdataNode)
        {
            std::string content = cdataNode->get_content();
            dataItem->setContent(content);
            Logger::debug("DataModelParser::parseDataContent() - Added CDATA content: " + content);
            continue; // CDATA 노드 처리 후 다음 노드로
        }


        // 요소 노드 처리 (XML 콘텐츠)
        auto *elementNode = dynamic_cast<const xmlpp::Element *>(child);
        if (elementNode)
        {
            // XML 콘텐츠를 문자열로 직렬화하는 방법이 필요함
            // 간단한 구현을 위해 요소 이름만 기록
            dataItem->setContent("<" + elementNode->get_name() + ">...</" + elementNode->get_name() + ">");
            Logger::debug("DataModelParser::parseDataContent() - Added XML content of type: " + elementNode->get_name());
        }
    }
}


std::vector<std::shared_ptr<IDataModelItem>> DataModelParser::parseDataModelInState(
    const xmlpp::Element *stateNode,
    const SCXMLContext &context)
{
    std::vector<std::shared_ptr<IDataModelItem>> items;


    if (!stateNode)
    {
        Logger::warning("DataModelParser::parseDataModelInState() - Null state node");
        return items;
    }


    Logger::debug("DataModelParser::parseDataModelInState() - Parsing datamodel in state");


    // datamodel 요소 찾기
    auto datamodelNode = stateNode->get_first_child("datamodel");
    if (datamodelNode)
    {
        auto *element = dynamic_cast<const xmlpp::Element *>(datamodelNode);
        if (element)
        {
            // context 전달
            auto stateItems = parseDataModelNode(element, context);
            items.insert(items.end(), stateItems.begin(), stateItems.end());
        }
    }


    Logger::debug("DataModelParser::parseDataModelInState() - Found " + std::to_string(items.size()) + " data items in state");
    return items;
}


std::string DataModelParser::extractDataModelType(const xmlpp::Element *datamodelNode) const
{
    if (!datamodelNode)
    {
        return "";
    }


    auto typeAttr = datamodelNode->get_attribute("type");
    if (typeAttr)
    {
        return typeAttr->get_value();
    }


    return "";
}


bool DataModelParser::isDataModelItem(const xmlpp::Element *element) const
{
    if (!element)
    {
        return false;
    }


    std::string nodeName = element->get_name();
    return matchNodeName(nodeName, "data");
}


bool DataModelParser::matchNodeName(const std::string &nodeName, const std::string &searchName) const
{
    // 정확히 일치하는 경우
    if (nodeName == searchName)
    {
        return true;
    }


    // 네임스페이스가 있는 경우 (예: "code:data")
    size_t colonPos = nodeName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < nodeName.length())
    {
        std::string localName = nodeName.substr(colonPos + 1);
        return localName == searchName;
    }


    return false;
}


void DataModelParser::loadExternalContent(const std::string &src, std::shared_ptr<IDataModelItem> dataItem)
{
    // 이 메서드는 실제 구현에서 외부 URL에서 데이터를 로드하는 작업을 담당
    // 예: 파일 시스템, HTTP 요청 등


    Logger::debug("DataModelParser::loadExternalContent() - Loading content from: " + src);


    // 예시: 파일 경로인 경우 파일 내용 로드
    if (src.find("file://") == 0 || src.find("/") == 0 || src.find("./") == 0)
    {
        std::string filePath = src;
        if (src.find("file://") == 0)
        {
            filePath = src.substr(7); // "file://" 접두사 제거
        }


        try
        {
            // 파일 내용 로드
            std::ifstream file(filePath);
            if (file.is_open())
            {
                std::stringstream buffer;
                buffer << file.rdbuf();
                dataItem->setContent(buffer.str());
                Logger::debug("DataModelParser::loadExternalContent() - Content loaded from file");
            }
            else
            {
                Logger::error("DataModelParser::loadExternalContent() - Failed to open file: " + filePath);
            }
        }
        catch (std::exception &e)
        {
            Logger::error("DataModelParser::loadExternalContent() - Exception loading file: " + std::string(e.what()));
        }
    }
    else if (src.find("http://") == 0 || src.find("https://") == 0)
    {
        // HTTP 요청은 더 복잡한 구현이 필요하며, 외부 라이브러리 사용이 권장됨
        // 여기서는 구현을 생략하고 로그만 남김
        Logger::warning("DataModelParser::loadExternalContent() - HTTP loading not implemented: " + src);
    }
    else
    {
        // 기타 프로토콜이나 상대 경로 등 처리
        Logger::warning("DataModelParser::loadExternalContent() - Unsupported URL format: " + src);
    }
}
