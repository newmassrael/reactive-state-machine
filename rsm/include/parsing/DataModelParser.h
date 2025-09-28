#pragma once

#include "factory/NodeFactory.h"
#include "model/IDataModelItem.h"
#include "model/SCXMLContext.h"
#include <fstream>
#include <libxml++/libxml++.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief 데이터 모델 요소 파싱을 담당하는 클래스
 *
 * 이 클래스는 SCXML 문서의 데이터 모델 관련 요소를 파싱하는
 * 기능을 제공합니다. datamodel 요소와 그 안의 data 요소들을
 * 파싱하여 IDataModelItem 객체로 변환합니다.
 */

namespace RSM {

class DataModelParser {
public:
    /**
     * @brief 생성자
     * @param nodeFactory 노드 생성을 위한 팩토리 인스턴스
     */
    explicit DataModelParser(std::shared_ptr<NodeFactory> nodeFactory);

    /**
     * @brief 소멸자
     */
    ~DataModelParser();

    /**
     * @brief datamodel 요소 파싱
     * @param datamodelNode XML datamodel 노드
     * @return 파싱된 데이터 모델 항목 목록
     */
    std::vector<std::shared_ptr<IDataModelItem>> parseDataModelNode(const xmlpp::Element *datamodelNode,
                                                                    const SCXMLContext &context);

    /**
     * @brief 개별 data 요소 파싱
     * @param dataNode XML data 노드
     * @return 생성된 데이터 모델 항목
     */
    std::shared_ptr<IDataModelItem> parseDataModelItem(const xmlpp::Element *dataNode, const SCXMLContext &context);

    /**
     * @brief 상태 노드 내의 모든 데이터 모델 요소 파싱
     * @param stateNode 상태 노드
     * @return 파싱된 데이터 모델 항목 목록
     */
    std::vector<std::shared_ptr<IDataModelItem>> parseDataModelInState(const xmlpp::Element *stateNode,
                                                                       const SCXMLContext &context);

    /**
     * @brief 요소가 데이터 모델 항목인지 확인
     * @param element XML 요소
     * @return 데이터 모델 항목 여부
     */
    bool isDataModelItem(const xmlpp::Element *element) const;

    /**
     * @brief 데이터 모델 타입 추출
     * @param datamodelNode XML datamodel 노드
     * @return 데이터 모델 타입 (기본값: "")
     */
    std::string extractDataModelType(const xmlpp::Element *datamodelNode) const;

private:
    /**
     * @brief 데이터 모델 항목의 콘텐츠 파싱
     * @param dataNode XML data 노드
     * @param dataItem 데이터 모델 항목
     */
    void parseDataContent(const xmlpp::Element *dataNode, std::shared_ptr<IDataModelItem> dataItem);

    /**
     * @brief 네임스페이스 문제 처리
     * @param nodeName 노드 이름
     * @param searchName 검색할 이름
     * @return 노드 이름이 검색 이름과 일치하는지 여부 (네임스페이스 고려)
     */
    bool matchNodeName(const std::string &nodeName, const std::string &searchName) const;

    /**
     * @brief 외부 데이터 소스에서 콘텐츠 로드
     * @param src 외부 데이터 소스 URL
     * @param dataItem 데이터를 저장할 항목
     */
    void loadExternalContent(const std::string &src, std::shared_ptr<IDataModelItem> dataItem);

    std::shared_ptr<NodeFactory> nodeFactory_;
};

}  // namespace RSM