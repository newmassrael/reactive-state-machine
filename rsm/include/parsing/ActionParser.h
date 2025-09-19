#pragma once

#include "factory/INodeFactory.h"
#include "model/IActionNode.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief 액션 요소 파싱을 담당하는 클래스
 *
 * 이 클래스는 SCXML 문서의 액션 관련 요소를 파싱하는
 * 기능을 제공합니다. 일반 액션(<code:action>)과 외부 실행
 * 액션(<code:external-action>) 모두 처리합니다.
 */

namespace RSM {

class ActionParser {
public:
    /**
     * @brief 생성자
     * @param nodeFactory 노드 생성을 위한 팩토리 인스턴스
     */
    explicit ActionParser(std::shared_ptr<INodeFactory> nodeFactory);

    /**
     * @brief 소멸자
     */
    ~ActionParser();

    /**
     * @brief 액션 노드 파싱
     * @param actionNode XML 액션 노드
     * @return 생성된 액션 노드
     */
    std::shared_ptr<IActionNode> parseActionNode(const xmlpp::Element *actionNode);

    /**
     * @brief 외부 실행 액션 노드 파싱
     * @param externalActionNode XML 외부 실행 액션 노드
     * @return 생성된 액션 노드
     */
    std::shared_ptr<IActionNode> parseExternalActionNode(const xmlpp::Element *externalActionNode);

    /**
     * @brief onentry/onexit 요소 내의 액션 파싱
     * @param parentElement 부모 요소 (onentry 또는 onexit)
     * @return 파싱된 액션 목록
     */
    std::vector<std::shared_ptr<IActionNode>> parseActionsInElement(const xmlpp::Element *parentElement);

    /**
     * @brief 요소가 액션 노드인지 확인
     * @param element XML 요소
     * @return 액션 노드 여부
     */
    bool isActionNode(const xmlpp::Element *element) const;

    /**
     * @brief 요소가 외부 실행 액션 노드인지 확인
     * @param element XML 요소
     * @return 외부 실행 액션 노드 여부
     */
    bool isExternalActionNode(const xmlpp::Element *element) const;

    /**
     * @brief 요소가 특수 처리가 필요한 실행 가능 콘텐츠인지 확인
     * @param element XML 요소
     * @return 특수 실행 가능 콘텐츠 여부
     */
    bool isSpecialExecutableContent(const xmlpp::Element *element) const;

private:
    /**
     * @brief 외부 구현 요소 파싱
     * @param element XML 요소
     * @param actionNode 액션 노드
     */
    void parseExternalImplementation(const xmlpp::Element *element, std::shared_ptr<IActionNode> actionNode);

    /**
     * @brief 특수 실행 가능 콘텐츠 파싱
     * @param element XML 요소
     * @param actions 파싱된 액션 목록 (수정됨)
     */
    void parseSpecialExecutableContent(const xmlpp::Element *element,
                                       std::vector<std::shared_ptr<IActionNode>> &actions);

    /**
     * @brief 네임스페이스 문제 처리
     * @param nodeName 노드 이름
     * @param searchName 검색할 이름
     * @return 노드 이름이 검색 이름과 일치하는지 여부 (네임스페이스 고려)
     */
    bool matchNodeName(const std::string &nodeName, const std::string &searchName) const;

    /**
     * @brief 노드 이름에서 로컬 이름 추출 (네임스페이스 제거)
     * @param nodeName 노드 이름
     * @return 로컬 노드 이름
     */
    std::string getLocalName(const std::string &nodeName) const;

    std::shared_ptr<INodeFactory> nodeFactory_;
};

}  // namespace RSM