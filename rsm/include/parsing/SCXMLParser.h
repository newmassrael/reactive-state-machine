#pragma once

#include "factory/NodeFactory.h"
#include "model/SCXMLContext.h"
#include "model/SCXMLModel.h"
#include "parsing/ActionParser.h"
#include "parsing/DataModelParser.h"
#include "parsing/DoneDataParser.h"
#include "parsing/GuardParser.h"
#include "parsing/InvokeParser.h"
#include "parsing/StateNodeParser.h"
#include "parsing/TransitionParser.h"
#include "parsing/XIncludeProcessor.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief SCXML 파싱을 총괄하는 클래스
 *
 * 이 클래스는 SCXML 문서의 파싱을 관리하고 조정하는 역할을 합니다.
 * 다양한 요소별 파서를 활용하여 문서를 완전한 객체 모델로 변환합니다.
 */

namespace RSM {

class SCXMLParser {
public:
    /**
     * @brief 생성자
     * @param nodeFactory 노드 생성을 위한 팩토리 인스턴스
     * @param xincludeProcessor XInclude 처리를 위한 프로세서 인스턴스 (선택적)
     */
    explicit SCXMLParser(std::shared_ptr<NodeFactory> nodeFactory,
                         std::shared_ptr<IXIncludeProcessor> xincludeProcessor = nullptr);

    /**
     * @brief 소멸자
     */
    ~SCXMLParser();

    /**
     * @brief SCXML 파일 파싱
     * @param filename 파싱할 파일 경로
     * @return 파싱된 SCXML 모델, 실패 시 nullptr
     */
    std::shared_ptr<SCXMLModel> parseFile(const std::string &filename);

    /**
     * @brief SCXML 문자열 파싱
     * @param content 파싱할 SCXML 문자열
     * @return 파싱된 SCXML 모델, 실패 시 nullptr
     */
    std::shared_ptr<SCXMLModel> parseContent(const std::string &content);

    /**
     * @brief 파싱 오류 확인
     * @return 오류가 발생했는지 여부
     */
    bool hasErrors() const;

    /**
     * @brief 파싱 오류 메시지 반환
     * @return 오류 메시지 목록
     */
    const std::vector<std::string> &getErrorMessages() const;

    /**
     * @brief 파싱 경고 메시지 반환
     * @return 경고 메시지 목록
     */
    const std::vector<std::string> &getWarningMessages() const;

    /**
     * @brief 상태 노드 파서 반환
     * @return 상태 노드 파서
     */
    std::shared_ptr<StateNodeParser> getStateNodeParser() const {
        return stateNodeParser_;
    }

    /**
     * @brief 전환 파서 반환
     * @return 전환 파서
     */
    std::shared_ptr<TransitionParser> getTransitionParser() const {
        return transitionParser_;
    }

    /**
     * @brief 액션 파서 반환
     * @return 액션 파서
     */
    std::shared_ptr<ActionParser> getActionParser() const {
        return actionParser_;
    }

    /**
     * @brief 가드 파서 반환
     * @return 가드 파서
     */
    std::shared_ptr<GuardParser> getGuardParser() const {
        return guardParser_;
    }

    /**
     * @brief 데이터 모델 파서 반환
     * @return 데이터 모델 파서
     */
    std::shared_ptr<DataModelParser> getDataModelParser() const {
        return dataModelParser_;
    }

    /**
     * @brief InvokeParser 반환
     * @return Invoke 파서
     */
    std::shared_ptr<InvokeParser> getInvokeParser() const {
        return invokeParser_;
    }

    /**
     * @brief DoneData 파서 반환
     * @return DoneData 파서
     */
    std::shared_ptr<DoneDataParser> getDoneDataParser() const {
        return doneDataParser_;
    }

    /**
     * @brief XInclude 프로세서 반환
     * @return XInclude 프로세서
     */
    std::shared_ptr<IXIncludeProcessor> getXIncludeProcessor() const {
        return xincludeProcessor_;
    }

private:
    /**
     * @brief XML 문서 파싱 실행
     * @param doc libxml++ 문서 객체
     * @return 파싱된 SCXML 모델, 실패 시 nullptr
     */
    std::shared_ptr<SCXMLModel> parseDocument(xmlpp::Document *doc);

    /**
     * @brief SCXML 루트 노드 파싱
     * @param scxmlNode 루트 노드
     * @param model 타겟 모델
     * @return 성공 여부
     */
    bool parseScxmlNode(const xmlpp::Element *scxmlNode, std::shared_ptr<SCXMLModel> model);

    /**
     * @brief 컨텍스트 속성 파싱
     * @param scxmlNode SCXML 노드
     * @param model 타겟 모델
     */
    void parseContextProperties(const xmlpp::Element *scxmlNode, std::shared_ptr<SCXMLModel> model);

    /**
     * @brief 의존성 주입 지점 파싱
     * @param scxmlNode SCXML 노드
     * @param model 타겟 모델
     */
    void parseInjectPoints(const xmlpp::Element *scxmlNode, std::shared_ptr<SCXMLModel> model);

    /**
     * @brief 파싱 작업 초기화
     */
    void initParsing();

    /**
     * @brief 오류 메시지 추가
     * @param message 오류 메시지
     */
    void addError(const std::string &message);

    /**
     * @brief 경고 메시지 추가
     * @param message 경고 메시지
     */
    void addWarning(const std::string &message);

    /**
     * @brief 모델 검증
     * @param model 검증할 모델
     * @return 유효한지 여부
     */
    bool validateModel(std::shared_ptr<SCXMLModel> model);

    void addSystemVariables(std::shared_ptr<SCXMLModel> model);

    std::shared_ptr<NodeFactory> nodeFactory_;
    std::shared_ptr<StateNodeParser> stateNodeParser_;
    std::shared_ptr<TransitionParser> transitionParser_;
    std::shared_ptr<ActionParser> actionParser_;
    std::shared_ptr<GuardParser> guardParser_;
    std::shared_ptr<DataModelParser> dataModelParser_;
    std::shared_ptr<InvokeParser> invokeParser_;
    std::shared_ptr<DoneDataParser> doneDataParser_;
    std::shared_ptr<IXIncludeProcessor> xincludeProcessor_;
    std::vector<std::string> errorMessages_;
    std::vector<std::string> warningMessages_;
};

}  // namespace RSM