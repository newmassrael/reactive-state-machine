#include "parsing/ParsingCommon.h"
#include <filesystem>
#include <algorithm>

// 상수 정의
const std::string ParsingCommon::Constants::SCXML_NAMESPACE = "http://www.w3.org/2005/07/scxml";
const std::string ParsingCommon::Constants::CODE_NAMESPACE = "http://www.example.org/code-extensions";
const std::string ParsingCommon::Constants::CTX_NAMESPACE = "http://www.example.org/context-extensions";
const std::string ParsingCommon::Constants::DI_NAMESPACE = "http://www.example.org/dependency-injection";

bool ParsingCommon::matchNodeName(const std::string &nodeName, const std::string &baseName)
{
    // 정확히 일치하는 경우
    if (nodeName == baseName)
    {
        return true;
    }

    // 네임스페이스가 있는 경우 (예: "code:action")
    size_t colonPos = nodeName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < nodeName.length())
    {
        std::string localName = nodeName.substr(colonPos + 1);
        return localName == baseName;
    }

    return false;
}

std::vector<const xmlpp::Element *> ParsingCommon::findChildElements(const xmlpp::Element *element, const std::string &childName)
{
    std::vector<const xmlpp::Element *> result;

    if (!element)
    {
        Logger::warning("ParsingCommon::findChildElements() - Null parent element");
        return result;
    }

    // 정확한 이름으로 자식 요소 찾기
    auto children = element->get_children(childName);
    for (auto *child : children)
    {
        auto *childElement = dynamic_cast<const xmlpp::Element *>(child);
        if (childElement)
        {
            result.push_back(childElement);
        }
    }

    // 네임스페이스가 다른 경우를 위한 추가 검색
    if (result.empty())
    {
        auto allChildren = element->get_children();
        for (auto *child : allChildren)
        {
            auto *childElement = dynamic_cast<const xmlpp::Element *>(child);
            if (childElement)
            {
                std::string nodeName = childElement->get_name();
                if (matchNodeName(nodeName, childName))
                {
                    result.push_back(childElement);
                }
            }
        }
    }

    return result;
}

const xmlpp::Element *ParsingCommon::findFirstChildElement(const xmlpp::Element *element, const std::string &childName)
{
    if (!element)
    {
        Logger::warning("ParsingCommon::findFirstChildElement() - Null parent element");
        return nullptr;
    }

    // 정확한 이름으로 첫 번째 자식 요소 찾기
    auto child = element->get_first_child(childName);
    if (child)
    {
        return dynamic_cast<const xmlpp::Element *>(child);
    }

    // 네임스페이스가 다른 경우를 위한 추가 검색
    auto allChildren = element->get_children();
    for (auto *c : allChildren)
    {
        auto *childElement = dynamic_cast<const xmlpp::Element *>(c);
        if (childElement)
        {
            std::string nodeName = childElement->get_name();
            if (matchNodeName(nodeName, childName))
            {
                return childElement;
            }
        }
    }

    return nullptr;
}

std::string ParsingCommon::findElementId(const xmlpp::Element *element)
{
    if (!element)
    {
        return "";
    }

    // 직접 id 속성 찾기
    auto idAttr = element->get_attribute("id");
    if (idAttr)
    {
        return idAttr->get_value();
    }

    // 대체 속성 시도
    auto nameAttr = element->get_attribute("name");
    if (nameAttr)
    {
        return nameAttr->get_value();
    }

    // 부모 요소에서 id 찾기
    auto parent = element->get_parent();
    if (parent)
    {
        auto *parentElement = dynamic_cast<const xmlpp::Element *>(parent);
        if (parentElement)
        {
            return findElementId(parentElement);
        }
    }

    return "";
}

std::string ParsingCommon::getAttributeValue(const xmlpp::Element *element, const std::vector<std::string> &attrNames)
{
    if (!element)
    {
        Logger::debug("ParsingCommon::getAttributeValue() - Null element provided");
        return "";
    }

    Logger::debug("ParsingCommon::getAttributeValue() - Searching for attributes in element: " + element->get_name());

    // 주어진 속성 이름 목록에서 차례대로 시도
    for (const auto &attrName : attrNames)
    {
        Logger::debug("ParsingCommon::getAttributeValue() - Checking attribute: " + attrName);

        auto attr = element->get_attribute(attrName);
        if (attr)
        {
            Logger::debug("ParsingCommon::getAttributeValue() - Found attribute: " + attrName + " = " + attr->get_value());
            return attr->get_value();
        }

        // 네임스페이스와 분리해서 속성 찾기
        std::vector<std::string> namespaces = {"code", "ctx", "di"};
        for (const auto &ns : namespaces)
        {
            auto nsAttr = element->get_attribute(attrName, ns);
            if (nsAttr)
            {
                Logger::debug("ParsingCommon::getAttributeValue() - Found namespaced attribute: " + ns + ":" + attrName + " = " + nsAttr->get_value());
                return nsAttr->get_value();
            }
        }
    }

    Logger::debug("ParsingCommon::getAttributeValue() - No matching attribute found for element: " + element->get_name());
    return "";
}

std::unordered_map<std::string, std::string> ParsingCommon::collectAttributes(
    const xmlpp::Element *element,
    const std::vector<std::string> &excludeAttrs)
{
    std::unordered_map<std::string, std::string> result;

    if (!element)
    {
        return result;
    }

    auto attributes = element->get_attributes();
    for (auto *attr : attributes)
    {
        auto *xmlAttr = dynamic_cast<const xmlpp::Attribute *>(attr);
        if (xmlAttr)
        {
            std::string name = xmlAttr->get_name();

            // 제외할 속성인지 확인
            bool excluded = false;
            for (const auto &excludeName : excludeAttrs)
            {
                if (matchNodeName(name, excludeName))
                {
                    excluded = true;
                    break;
                }
            }

            if (!excluded)
            {
                // 네임스페이스 제거 옵션
                size_t colonPos = name.find(':');
                if (colonPos != std::string::npos && colonPos + 1 < name.length())
                {
                    std::string localName = name.substr(colonPos + 1);
                    result[localName] = xmlAttr->get_value();
                }
                else
                {
                    result[name] = xmlAttr->get_value();
                }
            }
        }
    }

    return result;
}

std::string ParsingCommon::resolveRelativePath(const std::string &basePath, const std::string &relativePath)
{
    // 상대 경로가 절대 경로이면 그대로 반환
    if (std::filesystem::path(relativePath).is_absolute())
    {
        return relativePath;
    }

    // 기준 경로의 디렉토리 부분 추출
    std::filesystem::path baseDir = std::filesystem::path(basePath).parent_path();

    // 상대 경로 해석
    std::filesystem::path resolvedPath = baseDir / relativePath;

    // 정규화
    return std::filesystem::canonical(resolvedPath).string();
}

std::string ParsingCommon::extractTextContent(const xmlpp::Element *element, bool trimWhitespace)
{
    if (!element)
    {
        return "";
    }

    std::string result;
    auto children = element->get_children();

    for (auto *child : children)
    {
        // 일반 텍스트 노드 처리
        if (auto *textNode = dynamic_cast<const xmlpp::TextNode *>(child))
        {
            result += textNode->get_content();
            continue;
        }

        // CDATA 섹션 처리
        if (auto *cdataNode = dynamic_cast<const xmlpp::CdataNode *>(child))
        {
            result += cdataNode->get_content();
            continue;
        }
    }

    if (trimWhitespace)
    {
        // 앞뒤 공백 제거
        auto start = result.find_first_not_of(" \t\n\r\f\v");
        if (start == std::string::npos)
        {
            return ""; // 공백만 있는 경우
        }

        auto end = result.find_last_not_of(" \t\n\r\f\v");
        result = result.substr(start, end - start + 1);
    }

    return result;
}

std::string ParsingCommon::getLocalName(const xmlpp::Element *element)
{
    if (!element)
    {
        return "";
    }

    std::string fullName = element->get_name();
    size_t colonPos = fullName.find(':');

    if (colonPos != std::string::npos && colonPos + 1 < fullName.length())
    {
        return fullName.substr(colonPos + 1);
    }

    return fullName;
}

std::vector<const xmlpp::Element *> ParsingCommon::findChildElementsWithNamespace(
    const xmlpp::Element *parent,
    const std::string &elementName,
    const std::string &namespaceURI)
{
    std::vector<const xmlpp::Element *> result;
    if (!parent)
    {
        return result;
    }

    auto children = parent->get_children();
    for (auto child : children)
    {
        auto element = dynamic_cast<const xmlpp::Element *>(child);
        if (element)
        {
            std::string fullName = element->get_name();
            std::string ns = element->get_namespace_uri();

            // 네임스페이스 URI 확인 및 로컬 이름 추출
            if (ns == namespaceURI)
            {
                size_t colonPos = fullName.find(':');
                std::string localName = (colonPos != std::string::npos) ? fullName.substr(colonPos + 1) : fullName;

                if (localName == elementName)
                {
                    result.push_back(element);
                }
            }
        }
    }
    return result;
}

std::string ParsingCommon::trimString(const std::string &str)
{
    // 앞뒤 공백 제거
    auto start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos)
    {
        return ""; // 공백만 있는 경우
    }

    auto end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}
