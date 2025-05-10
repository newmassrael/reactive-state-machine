#pragma once

#include <memory>
#include <string>
#include <libxml++/libxml++.h>
#include "factory/INodeFactory.h"
#include "model/IStateNode.h"

/**
 * @brief <donedata> 요소를 파싱하는 클래스
 *
 * 이 클래스는 SCXML의 <donedata> 요소와 그 자식 요소들(<content>, <param>)을 파싱합니다.
 * <donedata>는 <final> 상태가 진입될 때 반환될 데이터를 정의합니다.
 */
class DoneDataParser
{
public:
    /**
     * @brief 생성자
     * @param factory 노드 생성을 위한 팩토리 인스턴스
     */
    explicit DoneDataParser(std::shared_ptr<INodeFactory> factory);

    /**
     * @brief 소멸자
     */
    ~DoneDataParser() = default;

    /**
     * @brief <donedata> 요소 파싱
     * @param doneDataElement <donedata> XML 요소
     * @param stateNode 대상 상태 노드
     * @return 파싱 성공 여부
     */
    bool parseDoneData(const xmlpp::Element *doneDataElement, IStateNode *stateNode);

private:
    /**
     * @brief <content> 요소 파싱
     * @param contentElement <content> XML 요소
     * @param stateNode 대상 상태 노드
     * @return 파싱 성공 여부
     */
    bool parseContent(const xmlpp::Element *contentElement, IStateNode *stateNode);

    /**
     * @brief <param> 요소 파싱
     * @param paramElement <param> XML 요소
     * @param stateNode 대상 상태 노드
     * @return 파싱 성공 여부
     */
    bool parseParam(const xmlpp::Element *paramElement, IStateNode *stateNode);

    std::shared_ptr<INodeFactory> factory_; // 노드 생성 팩토리
};
