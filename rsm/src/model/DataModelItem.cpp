#include "DataModelItem.h"
#include "common/Logger.h"
#include <libxml++/parsers/domparser.h>
#include <sstream>

RSM::DataModelItem::DataModelItem(const std::string &id,
                                  const std::string &expr)
    : id_(id), expr_(expr), scope_("global") {
  Logger::debug("RSM::DataModelItem::Constructor - Creating data model item: " +
                id);
}

RSM::DataModelItem::~DataModelItem() {
  Logger::debug(
      "RSM::DataModelItem::Destructor - Destroying data model item: " + id_);
  delete xmlContent_;
  xmlContent_ = nullptr;
}

const std::string &RSM::DataModelItem::getId() const { return id_; }

void RSM::DataModelItem::setExpr(const std::string &expr) {
  Logger::debug("RSM::DataModelItem::setExpr() - Setting expression for " +
                id_ + ": " + expr);
  expr_ = expr;
}

const std::string &RSM::DataModelItem::getExpr() const { return expr_; }

void RSM::DataModelItem::setType(const std::string &type) {
  Logger::debug("RSM::DataModelItem::setType() - Setting type for " + id_ +
                ": " + type);
  type_ = type;
}

const std::string &RSM::DataModelItem::getType() const { return type_; }

void RSM::DataModelItem::setScope(const std::string &scope) {
  Logger::debug("RSM::DataModelItem::setScope() - Setting scope for " + id_ +
                ": " + scope);
  scope_ = scope;
}

const std::string &RSM::DataModelItem::getScope() const { return scope_; }

void RSM::DataModelItem::setContent(const std::string &content) {
  Logger::debug("RSM::DataModelItem::setContent() - Setting content for " +
                id_);

  // 데이터 모델이 xpath 또는 xml 유형인 경우 XML 파싱 시도
  if (type_ == "xpath" || type_ == "xml") {
    setXmlContent(content);
  } else {
    // 다른 유형은 일반 문자열로 처리
    content_ = content;

    // XML 콘텐츠가 있었다면 제거
    delete xmlContent_;
    xmlContent_ = nullptr;
  }

  // 모든 경우에 contentItems_에 추가
  contentItems_.push_back(content);
}

void RSM::DataModelItem::addContent(const std::string &content) {
  Logger::debug("RSM::DataModelItem::addContent() - Adding content for " + id_);

  // contentItems_에 항상 추가
  contentItems_.push_back(content);

  // XML 타입이면 DOM에 추가 시도
  if (type_ == "xpath" || type_ == "xml") {
    if (xmlContent_ != nullptr) {
      try {
        // 임시 XML 문서로 파싱
        xmlpp::DomParser parser;
        parser.parse_memory(content);
        xmlpp::Document *tempDoc = parser.get_document();

        if (tempDoc && tempDoc->get_root_node()) {
          // 루트 노드 가져오기
          xmlpp::Node *root = xmlContent_->get_root_node();
          if (root) {
            // 새 콘텐츠를 기존 트리에 추가
            xmlpp::Node *importedNode = tempDoc->get_root_node();
            if (importedNode) {
              root->import_node(importedNode);
            }
          }
        }
      } catch (const std::exception &ex) {
        Logger::error(
            "RSM::DataModelItem::addContent() - Failed to parse XML content: " +
            std::string(ex.what()));
      }
    } else {
      // xmlContent_가 없으면 새로 생성
      setXmlContent(content);
    }
  } else {
    // XML 타입이 아니면 문자열에 추가
    if (!content_.empty()) {
      content_ += content;
    } else {
      content_ = content;
    }
  }
}

const std::string &RSM::DataModelItem::getContent() const {
  // XML 콘텐츠가 있고 content_가 비어있으면 XML을 문자열로 직렬화
  if (xmlContent_ && content_.empty()) {
    static std::string serialized;
    serialized.clear();

    try {
      // XML 문서를 문자열로 직렬화
      xmlContent_->write_to_string(serialized);
    } catch (const std::exception &ex) {
      Logger::error(
          "RSM::DataModelItem::getContent() - Failed to serialize XML: " +
          std::string(ex.what()));
    }

    return serialized;
  }

  return content_;
}

void RSM::DataModelItem::setSrc(const std::string &src) {
  Logger::debug("RSM::DataModelItem::setSrc() - Setting source URL for " + id_ +
                ": " + src);
  src_ = src;
}

const std::string &RSM::DataModelItem::getSrc() const { return src_; }

void RSM::DataModelItem::setAttribute(const std::string &name,
                                      const std::string &value) {
  Logger::debug("RSM::DataModelItem::setAttribute() - Setting attribute for " +
                id_ + ": " + name + " = " + value);
  attributes_[name] = value;
}

const std::string &
RSM::DataModelItem::getAttribute(const std::string &name) const {
  auto it = attributes_.find(name);
  if (it != attributes_.end()) {
    return it->second;
  }
  return emptyString_;
}

const std::unordered_map<std::string, std::string> &
RSM::DataModelItem::getAttributes() const {
  return attributes_;
}

void RSM::DataModelItem::setXmlContent(const std::string &content) {
  Logger::debug(
      "RSM::DataModelItem::setXmlContent() - Setting XML content for " + id_);

  // 기존 XML 문서가 있다면 삭제
  delete xmlContent_;
  xmlContent_ = nullptr;

  try {
    // XML 파싱
    xmlpp::DomParser parser;
    parser.parse_memory(content);

    // 새 문서 생성 후 내용 가져오기 (Document는 복사 불가)
    xmlContent_ = new xmlpp::Document();
    if (parser.get_document() && parser.get_document()->get_root_node()) {
      xmlContent_->create_root_node_by_import(
          parser.get_document()->get_root_node());
    }

    // 파싱 성공하면 content_는 비움 (필요시 getContent()에서 재생성)
    content_ = "";
  } catch (const std::exception &ex) {
    Logger::error(
        "RSM::DataModelItem::setXmlContent() - Failed to parse XML content: " +
        std::string(ex.what()));
    delete xmlContent_;
    xmlContent_ = nullptr;

    // 파싱 실패 시 일반 문자열로 저장
    content_ = content;
  }
}

xmlpp::Node *RSM::DataModelItem::getXmlContent() const {
  if (xmlContent_) {
    return xmlContent_->get_root_node();
  }
  return nullptr;
}

const std::vector<std::string> &RSM::DataModelItem::getContentItems() const {
  return contentItems_;
}

bool RSM::DataModelItem::isXmlContent() const { return xmlContent_ != nullptr; }

std::optional<std::string>
RSM::DataModelItem::queryXPath(const std::string &xpath) const {
  if (!xmlContent_ || !xmlContent_->get_root_node()) {
    return std::nullopt;
  }

  try {
    xmlpp::Node *root = xmlContent_->get_root_node();
    auto nodes = root->find(xpath);

    if (nodes.empty()) {
      return std::nullopt;
    }

    if (nodes.size() == 1) {
      // 단일 노드일 경우
      auto node = nodes[0];
      // 텍스트 노드를 찾기
      auto child = node->get_first_child();
      if (child && dynamic_cast<xmlpp::TextNode *>(child)) {
        return dynamic_cast<xmlpp::TextNode *>(child)->get_content();
      } else {
        // 텍스트 노드가 없으면 노드 경로 반환
        std::optional<xmlpp::ustring> path = node->get_path2();
        if (path) {
          return path.value();
        }
      }
    } else {
      // 여러 노드일 경우, 결과를 결합
      std::stringstream result;
      for (auto node : nodes) {
        auto child = node->get_first_child();
        if (child && dynamic_cast<xmlpp::TextNode *>(child)) {
          if (result.tellp() > 0) {
            result << " ";
          }
          result << dynamic_cast<xmlpp::TextNode *>(child)->get_content();
        }
      }
      return result.str();
    }
  } catch (const std::exception &ex) {
    Logger::error("RSM::DataModelItem::queryXPath() - XPath query failed: " +
                  std::string(ex.what()));
  }

  return std::nullopt;
}

bool RSM::DataModelItem::supportsDataModel(
    const std::string &dataModelType) const {
  // xpath와 xml 데이터 모델은 XML 처리 지원
  if (dataModelType == "xpath" || dataModelType == "xml") {
    return true;
  }

  // ecmascript 데이터 모델은 기본 문자열 처리 지원
  if (dataModelType == "ecmascript") {
    return true;
  }

  // null 데이터 모델은 제한적 지원
  if (dataModelType == "null") {
    return true;
  }

  // 그 외에는 지원하지 않음으로 판단
  return false;
}
