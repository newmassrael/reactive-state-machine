#pragma once

#include "IDataModelItem.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

// xmlpp 헤더 추가
#include <libxml++/document.h>
#include <libxml++/nodes/node.h>

/**
 * @brief 데이터 모델 항목의 구현 클래스
 *
 * 이 클래스는 SCXML 데이터 모델의 항목을 나타냅니다.
 * SCXML 문서의 <data> 요소에 해당합니다.
 */
class DataModelItem : public IDataModelItem
{
public:
    /**
     * @brief 생성자
     * @param id 항목 식별자
     * @param expr 표현식 (선택적)
     */
    explicit DataModelItem(const std::string &id, const std::string &expr = "");

    /**
     * @brief 소멸자
     */
    virtual ~DataModelItem();

    /**
     * @brief 항목 ID 반환
     * @return 항목 ID
     */
    virtual const std::string &getId() const override;

    /**
     * @brief 표현식 설정
     * @param expr 표현식
     */
    virtual void setExpr(const std::string &expr) override;

    /**
     * @brief 표현식 반환
     * @return 표현식
     */
    virtual const std::string &getExpr() const override;

    /**
     * @brief 타입 설정
     * @param type 데이터 타입
     */
    virtual void setType(const std::string &type) override;

    /**
     * @brief 타입 반환
     * @return 데이터 타입
     */
    virtual const std::string &getType() const override;

    /**
     * @brief 범위 설정
     * @param scope 데이터 범위 (예: "local", "global")
     */
    virtual void setScope(const std::string &scope) override;

    /**
     * @brief 범위 반환
     * @return 데이터 범위
     */
    virtual const std::string &getScope() const override;

    /**
     * @brief 내용 설정
     * @param content 항목 내용
     */
    virtual void setContent(const std::string &content) override;

    /**
     * @brief 내용 반환
     * @return 항목 내용
     */
    virtual const std::string &getContent() const override;

    /**
     * @brief 소스 URL 설정
     * @param src 외부 데이터 소스 URL
     */
    virtual void setSrc(const std::string &src) override;

    /**
     * @brief 소스 URL 반환
     * @return 외부 데이터 소스 URL
     */
    virtual const std::string &getSrc() const override;

    /**
     * @brief 추가 속성 설정
     * @param name 속성 이름
     * @param value 속성 값
     */
    virtual void setAttribute(const std::string &name, const std::string &value) override;

    /**
     * @brief 속성 값 반환
     * @param name 속성 이름
     * @return 속성 값, 없으면 빈 문자열
     */
    virtual const std::string &getAttribute(const std::string &name) const override;

    /**
     * @brief 모든 속성 반환
     * @return 모든 속성의 맵
     */
    virtual const std::unordered_map<std::string, std::string> &getAttributes() const override;

    /**
     * @brief XML 콘텐츠 추가
     * @param content 추가할 XML 콘텐츠
     * 이 메서드는 기존 XML 콘텐츠에 새로운 콘텐츠를 추가합니다.
     * 데이터 모델이 XML 기반일 때 유용하며, 기존 콘텐츠 구조를 유지합니다.
     */
    virtual void addContent(const std::string &content) override;

    /**
     * @brief 모든 콘텐츠 항목 반환
     * @return 추가된 순서대로의 모든 콘텐츠 항목 목록
     */
    virtual const std::vector<std::string> &getContentItems() const override;

    /**
     * @brief 콘텐츠가 XML 형식인지 확인
     * @return XML 형식이면 true, 아니면 false
     */
    virtual bool isXmlContent() const override;

    /**
     * @brief XPath 쿼리 실행 (XML 콘텐츠에만 적용)
     * @param xpath XPath 쿼리 문자열
     * @return 쿼리 결과 문자열, 실패 시 빈 옵셔널 반환
     */
    virtual std::optional<std::string> queryXPath(const std::string &xpath) const override;

    /**
     * @brief 데이터 모델 타입에 따른 내용 처리 가능 여부 확인
     * @param dataModelType 데이터 모델 타입 (예: "ecmascript", "xpath", "null")
     * @return 처리 가능하면 true, 아니면 false
     */
    virtual bool supportsDataModel(const std::string &dataModelType) const override;

    /**
     * @brief XML 콘텐츠 설정
     * @param content XML 콘텐츠 문자열
     */
    void setXmlContent(const std::string &content);

    /**
     * @brief XML 콘텐츠 노드 반환
     * @return XML 노드 포인터, 없으면 nullptr
     */
    xmlpp::Node *getXmlContent() const;

private:
    std::string id_;
    std::string expr_;
    std::string type_;
    std::string scope_;
    std::string content_;
    xmlpp::Document *xmlContent_ = nullptr;
    std::string src_;
    std::unordered_map<std::string, std::string> attributes_;
    std::vector<std::string> contentItems_; // 추가된 콘텐츠 항목 저장
    std::string emptyString_;
};
