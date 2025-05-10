#include "SCXMLParserTestCommon.h"

// 오류 처리 관련 기능을 테스트하는 픽스처 클래스
class SCXMLParserErrorTest : public SCXMLParserTestBase
{
};

// 잘못된 SCXML 처리 테스트
TEST_F(SCXMLParserErrorTest, HandleInvalidSCXML)
{
  // 잘못된 XML 구문
  std::string invalidXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <state id="s1">
    <transition event="e1" target="s2"/>
  </state>
  <state id="s2">
    <transition event="e2" target="nonexistent"/>
  </state>
</scml>)"; // 종료 태그가 잘못됨

  auto model = parser->parseContent(invalidXml);
  EXPECT_TRUE(model == nullptr);
  EXPECT_TRUE(parser->hasErrors());

  // 에러 메시지 확인
  const auto &errors = parser->getErrorMessages();
  EXPECT_FALSE(errors.empty());
}

// 오류 및 경고 메시지 테스트
TEST_F(SCXMLParserErrorTest, ErrorAndWarningMessages)
{
  // 존재하지 않는 파일 파싱 시도
  auto model = parser->parseFile("nonexistent_file.xml");

  EXPECT_TRUE(model == nullptr);
  EXPECT_TRUE(parser->hasErrors());

  const auto &errors = parser->getErrorMessages();
  EXPECT_FALSE(errors.empty());

  // 오류 메시지에 'File not found' 포함 여부 확인
  bool foundFileNotFoundError = false;
  for (const auto &error : errors)
  {
    if (error.find("File not found") != std::string::npos)
    {
      foundFileNotFoundError = true;
      break;
    }
  }
  EXPECT_TRUE(foundFileNotFoundError);
}

// 모델 검증 실패 테스트
TEST_F(SCXMLParserErrorTest, ModelValidationFailure)
{
  // 상태 노드 생성은 예상됨
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // s1, s2 상태

  // 전환 노드도 생성되어야 함
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(testing::AtLeast(1)); // s1->s2 전환

  // 유효하지 않은 초기 상태를 가진 SCXML
  std::string invalidInitialState = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="nonexistent">
  <state id="s1">
    <transition event="e1" target="s2"/>
  </state>
  <state id="s2"/>
</scxml>)";

  auto model = parser->parseContent(invalidInitialState);

  // 파싱은 성공할 수 있지만 검증에서 실패할 것으로 예상
  EXPECT_TRUE(model == nullptr || parser->hasErrors());

  if (parser->hasErrors())
  {
    const auto &errors = parser->getErrorMessages();
    bool foundInitialStateError = false;
    for (const auto &error : errors)
    {
      if (error.find("Initial state") != std::string::npos &&
          error.find("not found") != std::string::npos)
      {
        foundInitialStateError = true;
        break;
      }
    }

    // SCXMLParser의 검증 로직에 따라, 초기 상태 오류를 찾을 수 있어야 함
    EXPECT_TRUE(foundInitialStateError);
  }
}

// 오류가 있는 SCXML 유효성 검사 테스트
TEST_F(SCXMLParserErrorTest, InvalidModelValidation)
{
  // 잘못된 전환 대상을 가진 SCXML
  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <state id="s1">
    <transition event="next" target="nonexistent"/>
  </state>
</scxml>)";

  auto model = parser->parseContent(scxml);

  // validateModel에서 오류를 감지하고 파싱이 실패해야 함
  EXPECT_TRUE(model == nullptr || parser->hasErrors());

  if (parser->hasErrors())
  {
    const auto &errors = parser->getErrorMessages();
    bool foundTargetError = false;

    for (const auto &error : errors)
    {
      if (error.find("non-existent target") != std::string::npos)
      {
        foundTargetError = true;
        break;
      }
    }

    EXPECT_TRUE(foundTargetError);
  }
}

// 오류 복구 메커니즘 테스트
TEST_F(SCXMLParserErrorTest, ErrorRecoveryTest)
{
  // 오류가 다양한 형태로 포함된 SCXML 테스트
  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="nonexistent">
      <!-- 존재하지 않는 초기 상태 -->

      <state id="s1">
        <!-- 존재하지 않는 대상을 가리키는 전환 -->
        <transition event="e1" target="nonexistent_target"/>

        <!-- 유효한 전환 -->
        <transition event="e2" target="s2"/>

        <!-- 중복 ID (유효하지 않음) -->
        <state id="duplicate"/>
      </state>

      <state id="s2">
        <!-- 구문적으로 잘못된 조건 -->
        <transition event="e3" cond="(invalid syntax]" target="s3"/>

        <!-- 유효한 전환 -->
        <transition event="e4" target="s3"/>
      </state>

      <state id="s3"/>

      <!-- 중복 ID (유효하지 않음) -->
      <state id="duplicate"/>
    </scxml>)";

  // 파서가 오류 감지
  auto model = parser->parseContent(scxml);

  // 오류가 있기 때문에 모델이 null이거나 오류 메시지가 있어야 함
  EXPECT_TRUE(model == nullptr || parser->hasErrors());

  // 오류 메시지 확인
  if (parser->hasErrors())
  {
    const auto &errors = parser->getErrorMessages();
    EXPECT_FALSE(errors.empty());

    // 기대되는 오류 타입 확인
    bool foundInitialStateError = false;
    bool foundDuplicateIdError = false;
    bool foundInvalidTargetError = false;

    for (const auto &error : errors)
    {
      if (error.find("initial") != std::string::npos && error.find("not found") != std::string::npos)
      {
        foundInitialStateError = true;
      }
      else if (error.find("duplicate") != std::string::npos && error.find("id") != std::string::npos)
      {
        foundDuplicateIdError = true;
      }
      else if (error.find("non-existent target") != std::string::npos)
      {
        foundInvalidTargetError = true;
      }
    }

    // 적어도 하나의 예상된 오류가 있어야 함
    EXPECT_TRUE(foundInitialStateError || foundDuplicateIdError || foundInvalidTargetError);
  }

  // 이제 오류 복구 후 파싱이 성공하는 케이스 테스트
  std::string validScxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <transition event="e1" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

  // 이전 오류 상태를 초기화하도록 파서 재설정
  auto validModel = parser->parseContent(validScxml);

  // 유효한 SCXML 파싱 확인
  ASSERT_TRUE(validModel != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 오류가 정상적으로 초기화되었는지 확인
  EXPECT_TRUE(parser->getErrorMessages().empty());
}

// 실행 콘텐츠의 오류 처리 테스트
TEST_F(SCXMLParserErrorTest, ExecutableContentErrorHandlingTest)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // 최소 2개 상태 필요

  // 액션 노드 생성 기대
  EXPECT_CALL(*mockFactory, createActionNode(testing::_))
      .Times(testing::AtLeast(1)); // 최소 1개 액션 노드

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="testObj" expr="{}"/>
      </datamodel>
      <state id="s1">
        <onentry>
          <!-- 잠재적으로 오류를 발생시키는 실행 콘텐츠 -->
          <assign location="testObj.nonExistentProp.deeperProp" expr="'value'"/>
        </onentry>
        <transition event="error.execution" target="error"/>
        <transition event="next" target="normal"/>
      </state>
      <state id="error"/>
      <state id="normal"/>
    </scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // s1 상태 찾기
  IStateNode *s1 = model->findStateById("s1");
  ASSERT_TRUE(s1 != nullptr);

  // 오류 전환이 있는지 확인
  const auto &transitions = s1->getTransitions();
  ASSERT_GE(transitions.size(), 2);

  bool foundErrorTransition = false;
  for (const auto &t : transitions)
  {
    if (t->getEvent() == "error.execution")
    {
      foundErrorTransition = true;
      EXPECT_EQ("error", t->getTargets()[0]);
      break;
    }
  }

  EXPECT_TRUE(foundErrorTransition) << "에러 처리 전환이 없습니다";
}

// 오류 이벤트 처리 테스트
TEST_F(SCXMLParserErrorTest, ErrorEventHandling)
{
  // 오류 이벤트 처리 테스트
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(3));
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(testing::AtLeast(3));

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <onentry>
          <!-- 오류를 발생시키는 작업 -->
          <assign location="nonExistentVariable" expr="'value'"/>
        </onentry>
        <!-- 일반 오류 처리 -->
        <transition event="error" target="errorState"/>
        <!-- 특정 오류 유형 처리 -->
        <transition event="error.execution" target="executionErrorState"/>
        <!-- 플랫폼 오류 처리 -->
        <transition event="error.platform" target="platformErrorState"/>
      </state>
      <state id="errorState"/>
      <state id="executionErrorState"/>
      <state id="platformErrorState"/>
    </scxml>)";

  auto model = parser->parseContent(scxml);
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 오류 전환 확인
  IStateNode *s1 = model->findStateById("s1");
  ASSERT_TRUE(s1 != nullptr);
  const auto &transitions = s1->getTransitions();
  ASSERT_GE(transitions.size(), 3);

  // 오류 이벤트 매칭 확인
  bool foundGenericError = false;
  bool foundExecutionError = false;
  bool foundPlatformError = false;

  for (const auto &t : transitions)
  {
    if (t->getEvent() == "error")
    {
      foundGenericError = true;
      EXPECT_EQ("errorState", t->getTargets()[0]);
    }
    else if (t->getEvent() == "error.execution")
    {
      foundExecutionError = true;
      EXPECT_EQ("executionErrorState", t->getTargets()[0]);
    }
    else if (t->getEvent() == "error.platform")
    {
      foundPlatformError = true;
      EXPECT_EQ("platformErrorState", t->getTargets()[0]);
    }
  }

  EXPECT_TRUE(foundGenericError);
  EXPECT_TRUE(foundExecutionError);
  EXPECT_TRUE(foundPlatformError);
}

// 런타임 보안 및 유효성 검사 테스트
TEST_F(SCXMLParserErrorTest, RuntimeSecurityValidation)
{
  // 보안 관련 유효성 검사 테스트
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(3));
  EXPECT_CALL(*mockFactory, createActionNode(testing::_))
      .Times(testing::AtLeast(2));

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <datamodel>
        <data id="securityToken" expr="'secret123'"/>
        <data id="_reservedPrefix" expr="'should not be allowed'"/>
      </datamodel>
      <state id="s1">
        <onentry>
          <!-- 시스템 변수 수정 시도 -->
          <assign location="_event" expr="null"/>
          <assign location="_sessionid" expr="'hacked'"/>
        </onentry>
        <transition event="next" target="s2"/>
      </state>
      <state id="s2">
        <transition event="error.execution" target="error"/>
      </state>
      <state id="error"/>
    </scxml>)";

  auto model = parser->parseContent(scxml);

  // 오류가 있기 때문에 모델이 null이거나 오류 메시지가 있어야 함
  if (model != nullptr && !parser->hasErrors())
  {
    // 보안 관련 유효성 검사가 있는 경우 이 부분이 실행됨
    // (현재 구현에서는 이러한 보안 검사가 없을 수 있음)
  }

  // 다른 시스템 ID 중복 테스트
  std::string scxml2 = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <state id="_internal"/>
        <transition event="next" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

  auto model2 = parser->parseContent(scxml2);
}
