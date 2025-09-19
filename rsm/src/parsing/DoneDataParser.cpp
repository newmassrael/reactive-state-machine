#include "parsing/DoneDataParser.h"
#include "common/Logger.h"
#include "parsing/ParsingCommon.h"

RSM::DoneDataParser::DoneDataParser(std::shared_ptr<INodeFactory> factory)
    : factory_(factory) {
  Logger::debug("RSM::DoneDataParser::Constructor - Creating DoneData parser");
}

bool RSM::DoneDataParser::parseDoneData(const xmlpp::Element *doneDataElement,
                                        IStateNode *stateNode) {
  if (!doneDataElement || !stateNode) {
    Logger::error("RSM::DoneDataParser::parseDoneData - Null doneData element "
                  "or state node");
    return false;
  }

  Logger::debug(
      "RSM::DoneDataParser::parseDoneData - Parsing <donedata> for state " +
      stateNode->getId());

  bool hasContent = false;
  bool hasParam = false;

  // <content> 요소 파싱
  const xmlpp::Element *contentElement =
      ParsingCommon::findFirstChildElement(doneDataElement, "content");
  if (contentElement) {
    hasContent = parseContent(contentElement, stateNode);
    Logger::debug(
        std::string(
            "RSM::DoneDataParser::parseDoneData - Found <content> element: ") +
        (hasContent ? "valid" : "invalid"));
  }

  // <param> 요소들 파싱
  auto paramElements =
      ParsingCommon::findChildElements(doneDataElement, "param");
  for (auto *paramElement : paramElements) {
    if (parseParam(paramElement, stateNode)) {
      hasParam = true;
    }
  }

  Logger::debug("RSM::DoneDataParser::parseDoneData - Found " +
                std::to_string(paramElements.size()) +
                " <param> elements: " + (hasParam ? "valid" : "invalid"));

  // <content>와 <param>은 함께 사용할 수 없음
  if (hasContent && hasParam) {
    Logger::error("RSM::DoneDataParser::parseDoneData - <content> and <param> "
                  "cannot be used together in <donedata>");

    // 충돌 감지 시 명확한 정리
    // content와 param이 둘 다 설정되어 있으므로 하나를 제거하여 XOR 조건 충족
    if (hasContent) {
      // content 유지하고 param 제거
      stateNode->clearDoneDataParams();
      hasParam = false;
    } else {
      // param 유지하고 content 제거
      stateNode->setDoneDataContent("");
      hasContent = false;
    }

    // SCXMLParser에 오류를 전파하려면 이 메서드에서 false를 반환해야 합니다
    return false;
  }

  return hasContent || hasParam;
}

bool RSM::DoneDataParser::parseContent(const xmlpp::Element *contentElement,
                                       IStateNode *stateNode) {
  if (!contentElement || !stateNode) {
    Logger::error("RSM::DoneDataParser::parseContent - Null content element or "
                  "state node");
    return false;
  }

  // expr 속성 확인
  auto exprAttr = contentElement->get_attribute("expr");
  std::string exprValue;
  if (exprAttr) {
    exprValue = exprAttr->get_value();
    Logger::debug(
        "RSM::DoneDataParser::parseContent - Found 'expr' attribute: " +
        exprValue);
  }

  // 내용 확인
  std::string textContent;
  const xmlpp::Node *childNode = contentElement->get_first_child();
  if (childNode) {
    // TextNode로 타입 변환 시도
    const xmlpp::TextNode *textNode =
        dynamic_cast<const xmlpp::TextNode *>(childNode);
    if (textNode) {
      textContent = textNode->get_content();
      textContent = ParsingCommon::trimString(textContent);
      Logger::debug("RSM::DoneDataParser::parseContent - Found text content: " +
                    (textContent.length() > 30
                         ? textContent.substr(0, 27) + "..."
                         : textContent));
    }
  }

  // expr과 내용은 함께 사용할 수 없음
  if (!exprValue.empty() && !textContent.empty()) {
    Logger::error("RSM::DoneDataParser::parseContent - <content> cannot have "
                  "both 'expr' attribute and child content");
    return false;
  }

  // expr 또는 내용 설정
  if (!exprValue.empty()) {
    stateNode->setDoneDataContent(exprValue);
    return true;
  } else if (!textContent.empty()) {
    stateNode->setDoneDataContent(textContent);
    return true;
  }

  // 빈 content 처리
  stateNode->setDoneDataContent("");
  return true;
}

bool RSM::DoneDataParser::parseParam(const xmlpp::Element *paramElement,
                                     IStateNode *stateNode) {
  if (!paramElement || !stateNode) {
    Logger::error(
        "RSM::DoneDataParser::parseParam - Null param element or state node");
    return false;
  }

  // name 속성 (필수)
  auto nameAttr = paramElement->get_attribute("name");
  if (!nameAttr) {
    Logger::error("RSM::DoneDataParser::parseParam - <param> element must have "
                  "'name' attribute");
    return false;
  }

  std::string nameValue = nameAttr->get_value();

  // expr과 location 속성 확인 (둘 중 하나만 사용 가능)
  auto exprAttr = paramElement->get_attribute("expr");
  auto locationAttr = paramElement->get_attribute("location");

  if (exprAttr && locationAttr) {
    Logger::error("RSM::DoneDataParser::parseParam - <param> cannot have both "
                  "'expr' and 'location' attributes");
    return false;
  }

  // location 속성 처리 (param을 donedata에 추가)
  if (locationAttr) {
    std::string locationValue = locationAttr->get_value();
    stateNode->addDoneDataParam(nameValue, locationValue);
    Logger::debug("RSM::DoneDataParser::parseParam - Added param: " +
                  nameValue + " with location: " + locationValue);
    return true;
  }

  // expr 속성 처리
  if (exprAttr) {
    std::string exprValue = exprAttr->get_value();
    // 단순히 expr 값을 location으로 변환하여 사용
    // 필요에 따라 더 복잡한 처리를 추가할 수 있음
    stateNode->addDoneDataParam(nameValue, exprValue);
    Logger::debug("RSM::DoneDataParser::parseParam - Added param: " +
                  nameValue + " with expr: " + exprValue);
    return true;
  }

  Logger::error("RSM::DoneDataParser::parseParam - <param> must have either "
                "'expr' or 'location' attribute");
  return false;
}
