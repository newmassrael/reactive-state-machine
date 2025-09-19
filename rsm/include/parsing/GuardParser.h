#pragma once

#include "factory/INodeFactory.h"
#include "model/IGuardNode.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief 가드 조건 파싱을 담당하는 클래스
 *
 * 이 클래스는 SCXML 문서의 가드 조건 관련 요소를 파싱하는
 * 기능을 제공합니다. code:guards 요소 내의 code:guard 요소와
 * transition 요소의 가드 속성을 처리합니다.
 */

namespace RSM {

class GuardParser {
public:
    /**
     * @brief 생성자
     * @param nodeFactory 노드 생성을 위한 팩토리 인스턴스
     */
    explicit GuardParser(std::shared_ptr<INodeFactory> nodeFactory);

    /**
     * @brief 소멸자
     */
    ~GuardParser();

    /**
     * @brief 가드 노드 파싱
     * @param guardNode XML 가드 노드
     * @return 생성된 가드 노드
     */
    std::shared_ptr<IGuardNode> parseGuardNode(const xmlpp::Element *guardNode);

    /**
     * @brief 전환의 가드 속성 파싱
     * @param transitionNode XML 전환 노드
     * @param targetState 전환 대상 상태
     * @return 생성된 가드 노드, 가드 속성이 없으면 nullptr
     */
    std::shared_ptr<IGuardNode> parseGuardFromTransition(const xmlpp::Element *transitionNode,
                                                         const std::string &targetState);

    /**
     * @brief 반응형 가드 파싱
     * @param reactiveGuardNode XML 반응형 가드 노드
     * @return 생성된 가드 노드
     */
    std::shared_ptr<IGuardNode> parseReactiveGuard(const xmlpp::Element *reactiveGuardNode);

    /**
     * @brief guards 요소 내의 모든 가드 파싱
     * @param guardsNode code:guards 요소
     * @return 파싱된 가드 노드 목록
     */
    std::vector<std::shared_ptr<IGuardNode>> parseGuardsElement(const xmlpp::Element *guardsNode);

    /**
     * @brief SCXML 문서의 모든 가드 파싱
     * @param scxmlNode SCXML 루트 노드
     * @return 파싱된 가드 노드 목록
     */
    std::vector<std::shared_ptr<IGuardNode>> parseAllGuards(const xmlpp::Element *scxmlNode);

    /**
     * @brief 요소가 가드 노드인지 확인
     * @param element XML 요소
     * @return 가드 노드 여부
     */
    bool isGuardNode(const xmlpp::Element *element) const;

    /**
     * @brief 요소가 반응형 가드 노드인지 확인
     * @param element XML 요소
     * @return 반응형 가드 노드 여부
     */
    bool isReactiveGuardNode(const xmlpp::Element *element) const;

private:
    /**
     * @brief 의존성 목록 파싱
     * @param guardNode 가드 노드
     * @param guardObject 가드 객체
     */
    void parseDependencies(const xmlpp::Element *guardNode, std::shared_ptr<IGuardNode> guardObject);

    /**
     * @brief 외부 구현 요소 파싱
     * @param guardNode 가드 노드
     * @param guardObject 가드 객체
     */
    void parseExternalImplementation(const xmlpp::Element *guardNode, std::shared_ptr<IGuardNode> guardObject);

    /**
     * @brief 네임스페이스 문제 처리
     * @param nodeName 노드 이름
     * @param searchName 검색할 이름
     * @return 노드 이름이 검색 이름과 일치하는지 여부 (네임스페이스 고려)
     */
    bool matchNodeName(const std::string &nodeName, const std::string &searchName) const;

    /**
     * @brief 조건식과 상태 분리
     * @param guardNode 가드 노드
     * @param guardObject 가드 객체
     * @param target XML target 속성 값
     */
    void parseTargetAndCondition(const xmlpp::Element *guardNode, std::shared_ptr<IGuardNode> guardObject,
                                 const std::string &target);

    std::shared_ptr<INodeFactory> nodeFactory_;
};

}  // namespace RSM