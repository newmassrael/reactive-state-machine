#pragma once

#include "factory/INodeFactory.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "parsing/ActionParser.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief 전환 요소 파싱을 담당하는 클래스
 *
 * 이 클래스는 SCXML 문서의 전환 관련 요소(transition)를 파싱하는
 * 기능을 제공합니다. 상태 간 전환 관계를 구축하고 전환 시 실행할
 * 액션과 가드 조건을 처리합니다.
 */

namespace RSM {

class TransitionParser {
public:
    /**
     * @brief 생성자
     * @param nodeFactory 노드 생성을 위한 팩토리 인스턴스
     */
    explicit TransitionParser(std::shared_ptr<INodeFactory> nodeFactory);

    /**
     * @brief 소멸자
     */
    ~TransitionParser();

    /**
     * @brief 액션 파서 설정
     * @param actionParser 액션 파싱을 위한 파서
     */
    void setActionParser(std::shared_ptr<ActionParser> actionParser);

    /**
     * @brief 전환 노드 파싱
     * @param transElement XML 전환 요소
     * @param stateNode 소유자 상태 노드
     * @return 생성된 전환 노드
     */
    std::shared_ptr<ITransitionNode> parseTransitionNode(const xmlpp::Element *transElement, IStateNode *stateNode);

    /**
     * @brief 초기 전환 파싱
     * @param initialElement XML initial 요소
     * @return 생성된 전환 노드
     */
    std::shared_ptr<ITransitionNode> parseInitialTransition(const xmlpp::Element *initialElement);

    /**
     * @brief 상태 내의 모든 전환 파싱
     * @param stateElement 상태 요소
     * @param stateNode 상태 노드
     * @return 파싱된 전환 노드 목록
     */
    std::vector<std::shared_ptr<ITransitionNode>> parseTransitionsInState(const xmlpp::Element *stateElement,
                                                                          IStateNode *stateNode);

    /**
     * @brief 요소가 전환 노드인지 확인
     * @param element XML 요소
     * @return 전환 노드 여부
     */
    bool isTransitionNode(const xmlpp::Element *element) const;

private:
    /**
     * @brief 전환의 액션 파싱
     * @param transElement 전환 요소
     * @param transition 전환 노드
     */
    void parseActions(const xmlpp::Element *transElement, std::shared_ptr<ITransitionNode> transition);

    /**
     * @brief 이벤트 목록 파싱
     * @param eventStr 이벤트 문자열 (공백으로 구분된 목록)
     * @return 개별 이벤트 목록
     */
    std::vector<std::string> parseEventList(const std::string &eventStr) const;

    /**
     * @brief 네임스페이스 문제 처리
     * @param nodeName 노드 이름
     * @param searchName 검색할 이름
     * @return 노드 이름이 검색 이름과 일치하는지 여부
     */
    bool matchNodeName(const std::string &nodeName, const std::string &searchName) const;

    std::vector<std::string> parseTargetList(const std::string &targetStr) const;

    std::shared_ptr<INodeFactory> nodeFactory_;
    std::shared_ptr<ActionParser> actionParser_;
};

}  // namespace RSM