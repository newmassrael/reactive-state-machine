#include "SCXMLParserTestCommon.h"

// 기본 테스트 픽스처 상속
class SCXMLParserBasicTest : public SCXMLParserTestBase
{
};

// 가장 간단한 테스트로 시작
TEST_F(SCXMLParserBasicTest, SimpleTest)
{
  // createStateNode가 최소 한 번 호출될 것으로 예상
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(1));

  // TransitionParser가 StateNodeParser에 설정되지 않았기 때문에 전환이 파싱되지 않음
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(0);

  std::string simpleSCXML = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0">
  <state id="root"/>
</scxml>)";

  auto model = parser->parseContent(simpleSCXML);
  EXPECT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());
}

// 기본 SCXML 문자열 파싱 테스트
TEST_F(SCXMLParserBasicTest, BasicParseContent)
{
  // 기대하는 호출 횟수 설정
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // s1, s2, s3 상태 생성

  // 전환 노드도 생성되어야 함
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(testing::AtLeast(3));

  std::string scxml = createBasicTestSCXML();
  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 초기 상태가 제대로 설정되었는지 확인
  EXPECT_EQ("s1", model->getInitialState());

  // 모든 상태가 제대로 파싱되었는지 확인
  const auto &allStates = model->getAllStates();
  EXPECT_EQ(3, allStates.size());
}

// 파일에서 SCXML 파싱 테스트
TEST_F(SCXMLParserBasicTest, ParseFile)
{
  // 기대하는 호출 횟수 설정
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(3));

  // 전환 노드도 생성되어야 함
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // e1->s2, e2->s3, e3->s1 전환에 대해

  std::string scxml = createBasicTestSCXML();
  std::string filename = createTestSCXMLFile(scxml);

  auto model = parser->parseFile(filename);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 초기 상태가 제대로 설정되었는지 확인
  EXPECT_EQ("s1", model->getInitialState());

  // 파싱된 상태 수 확인
  const auto &allStates = model->getAllStates();
  EXPECT_EQ(3, allStates.size());

  // 테스트 파일 삭제
  std::remove(filename.c_str());
}

// XInclude 처리 테스트
TEST_F(SCXMLParserBasicTest, ParseWithXInclude)
{
  // XInclude 처리 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // 메인 상태 및 포함된 상태

  // 메인 SCXML 생성
  std::string mainScxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml"
       xmlns:xi="http://www.w3.org/2001/XInclude"
       version="1.0" initial="main">
  <state id="main">
    <xi:include href="included_state.xml"/>
    <transition event="done" target="final"/>
  </state>
  <final id="final"/>
</scxml>)";

  // 포함될 파일 생성
  std::string includedState = R"(<?xml version="1.0" encoding="UTF-8"?>
<state id="included" xmlns="http://www.w3.org/2005/07/scxml">
  <onentry>
    <log expr="'Entering included state'"/>
  </onentry>
</state>)";

  // 파일 저장
  std::string mainFilename = createTestSCXMLFile(mainScxml);
  std::string includedFilename = "included_state.xml";
  std::ofstream includedFile(includedFilename);
  includedFile << includedState;
  includedFile.close();

  // xincludeProcessor가 실제로 호출되는지 확인하기 위한 설정
  auto mockXIncludeProcessor = std::make_shared<MockXIncludeProcessor>();

  // XIncludeProcessor의 process 메서드가 호출될 것을 기대
  EXPECT_CALL(*mockXIncludeProcessor, process(testing::_))
      .Times(1);

  // 중요: 여기에서 파서를 재생성하여 mockXIncludeProcessor를 주입
  parser = std::make_shared<SCXMLParser>(mockFactory, mockXIncludeProcessor);

  auto model = parser->parseFile(mainFilename);

  // 파싱이 성공하고 Model이 반환되었는지 확인
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 테스트 파일 정리
  std::remove(mainFilename.c_str());
  std::remove(includedFilename.c_str());
}

// 시스템 변수 및 표현식 평가 테스트
TEST_F(SCXMLParserBasicTest, SystemVariablesTest)
{
  std::string scxml = R"delim(<?xml version="1.0" encoding="UTF-8"?>
  <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript" name="TestMachine">
    <datamodel>
      <data id="sessionCheck" expr="0"/>
    </datamodel>
    <state id="s1">
      <onentry>
        <assign location="sessionCheck" expr="_sessionid != ''"/>
        <assign location="nameCheck" expr="_name == 'TestMachine'"/>
        <assign location="eventAvailable" expr="_event != null"/>
        <assign location="inStateCheck" expr="In('s1')"/>
      </onentry>
      <transition event="check" cond="sessionCheck &amp;&amp; nameCheck &amp;&amp; inStateCheck" target="s2"/>
      <transition event="check" target="error"/>
    </state>
    <state id="s2"/>
    <state id="error"/>
  </scxml>)delim";

  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // s1, s2, error

  auto model = parser->parseContent(scxml);
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 시스템 이름 확인
  EXPECT_EQ("TestMachine", model->getName());
}

// 시스템 변수 및 표현식 테스트
TEST_F(SCXMLParserBasicTest, SystemVariablesTest2)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // 최소 3개 상태 필요

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript" name="TestMachine">
      <datamodel>
        <data id="sessionCheck" expr="false"/>
        <data id="nameCheck" expr="false"/>
        <data id="eventData" expr="null"/>
        <data id="ioprocessorCheck" expr="false"/>
      </datamodel>

      <state id="s1">
        <onentry>
          <!-- 시스템 변수 접근 테스트 -->
          <assign location="sessionCheck" expr="_sessionid != ''"/>
          <assign location="nameCheck" expr="_name == 'TestMachine'"/>
          <assign location="ioprocessorCheck" expr="_ioprocessors != null"/>
        </onentry>

        <!-- 시스템 변수를 조건으로 사용 -->
        <transition event="check" cond="sessionCheck &amp;&amp; nameCheck &amp;&amp; ioprocessorCheck" target="s2"/>
        <transition event="check" target="error"/>

        <!-- 이벤트 정보 접근 -->
        <transition event="data" target="s3">
          <assign location="eventData" expr="_event.data"/>
        </transition>
      </state>

      <state id="s2"/>
      <state id="s3"/>
      <state id="error"/>
    </scxml>)";

  auto model = parser->parseContent(scxml);
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 시스템 이름 확인
  EXPECT_EQ("TestMachine", model->getName());

  // s1 상태 찾기
  IStateNode *s1 = model->findStateById("s1");
  ASSERT_TRUE(s1 != nullptr);

  // onentry에서 시스템 변수 접근 코드 확인
  EXPECT_FALSE(s1->getOnEntry().empty());

  // 전환 조건에 사용된 변수 확인
  const auto &transitions = s1->getTransitions();
  bool foundCondTransition = false;

  for (const auto &t : transitions)
  {
    if (t->getEvent() == "check" && t->getGuard() == "sessionCheck && nameCheck && ioprocessorCheck")
    {
      foundCondTransition = true;
      EXPECT_EQ("s2", t->getTargets()[0]);
      break;
    }
  }

  EXPECT_TRUE(foundCondTransition);
}

// 커스텀 네임스페이스 테스트
TEST_F(SCXMLParserBasicTest, CustomNamespaces)
{
  // 다양한 커스텀 네임스페이스를 사용한 SCXML
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(1));

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml"
       xmlns:custom="http://example.org/custom"
       xmlns:di="http://example.org/di"
       xmlns:ctx="http://example.org/ctx"
       version="1.0" initial="s1">
  <custom:metadata>
    <custom:author>Test Author</custom:author>
    <custom:version>1.0.0</custom:version>
  </custom:metadata>
  <ctx:property name="counter" type="int"/>
  <di:inject-point name="logger" type="ILogger"/>
  <state id="s1">
    <custom:description>This is a test state</custom:description>
  </state>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 컨텍스트 속성 및 의존성 주입 확인
  const auto &props = model->getContextProperties();
  EXPECT_EQ(1, props.size());

  auto it = props.find("counter");
  ASSERT_NE(props.end(), it);
  EXPECT_EQ("int", it->second);

  const auto &injects = model->getInjectPoints();
  EXPECT_EQ(1, injects.size());

  auto injectIt = injects.find("logger");
  ASSERT_NE(injects.end(), injectIt);
  EXPECT_EQ("ILogger", injectIt->second);
}

// XML 네임스페이스 처리 테스트
TEST_F(SCXMLParserBasicTest, MultipleNamespacesTest)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // 최소 2개 상태 필요

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml"
           xmlns:custom="http://example.org/custom"
           xmlns:di="http://example.org/di"
           xmlns:ctx="http://example.org/ctx"
           xmlns:my="http://my.custom.namespace/"
           version="1.0" initial="s1">

      <custom:metadata>
        <custom:author>Test Author</custom:author>
        <custom:version>1.0.0</custom:version>
      </custom:metadata>

      <ctx:property name="counter" type="int"/>
      <di:inject-point name="logger" type="ILogger"/>

      <datamodel>
        <data id="config">
          <my:configuration xmlns:my="http://my.custom.namespace/">
            <my:setting id="timeout" value="30"/>
            <my:setting id="retries" value="3"/>
          </my:configuration>
        </data>
      </datamodel>

      <state id="s1">
        <custom:description>This is a test state with custom namespace elements</custom:description>
        <onentry>
          <my:customAction name="initialize" param="config"/>
        </onentry>
        <transition event="next" target="s2"/>
      </state>

      <state id="s2">
        <my:customState type="special"/>
      </state>
    </scxml>)";

  auto model = parser->parseContent(scxml);
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 컨텍스트 속성 확인
  const auto &props = model->getContextProperties();
  EXPECT_EQ(1, props.size());

  auto it = props.find("counter");
  ASSERT_NE(props.end(), it);
  EXPECT_EQ("int", it->second);

  // 의존성 주입 확인
  const auto &injects = model->getInjectPoints();
  EXPECT_EQ(1, injects.size());

  auto injectIt = injects.find("logger");
  ASSERT_NE(injects.end(), injectIt);
  EXPECT_EQ("ILogger", injectIt->second);

  // s1 상태 찾기
  IStateNode *s1 = model->findStateById("s1");
  ASSERT_TRUE(s1 != nullptr);

  // s2 상태 찾기
  IStateNode *s2 = model->findStateById("s2");
  ASSERT_TRUE(s2 != nullptr);
}

// 새로운 네임스페이스와 XML 콘텐츠 테스트
TEST_F(SCXMLParserBasicTest, NamespaceAndXMLContent)
{
  // 다양한 네임스페이스와 XML 콘텐츠 처리
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(2));
  EXPECT_CALL(*mockFactory, createDataModelItem(testing::_, testing::_))
      .Times(testing::AtLeast(1));

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml"
           xmlns:custom="http://example.org/custom"
           xmlns:data="http://example.org/data"
           xmlns:viz="http://example.org/visualization"
           version="1.0" initial="s1">
      <datamodel>
        <data id="xmlData">
          <data:record xmlns:data="http://example.org/data">
            <data:field name="id">12345</data:field>
            <data:field name="status">active</data:field>
          </data:record>
        </data>
      </datamodel>
      <state id="s1">
        <custom:metadata>
          <custom:author>Test Author</custom:author>
          <custom:version>1.0.0</custom:version>
        </custom:metadata>
        <viz:appearance color="blue" shape="rectangle"/>
        <onentry>
          <send event="custom.event">
            <content>
              <custom:message xmlns:custom="http://example.org/custom">
                <custom:header>Important Notice</custom:header>
                <custom:body>This is a test message with XML content</custom:body>
              </custom:message>
            </content>
          </send>
        </onentry>
        <transition event="next" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

  auto model = parser->parseContent(scxml);
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // XML 콘텐츠 및 네임스페이스 확인
  IStateNode *s1 = model->findStateById("s1");
  ASSERT_TRUE(s1 != nullptr);
  EXPECT_FALSE(s1->getOnEntry().empty());
}

// 안전한 종료 테스트
TEST_F(SCXMLParserBasicTest, CleanShutdown)
{
  // 안전한 종료 프로세스 테스트
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(4));
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(testing::AtLeast(3));
  EXPECT_CALL(*mockFactory, createInvokeNode(testing::_))
      .Times(testing::AtLeast(1));

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="running">
      <state id="running">
        <invoke id="subprocess" type="http://www.w3.org/TR/scxml/">
          <content>
            <!-- 간단한 하위 상태 머신 -->
            <scxml version="1.0" initial="substate">
              <state id="substate"/>
            </scxml>
          </content>
        </invoke>
        <onentry>
          <!-- 정상적인 작업 -->
        </onentry>
        <onexit>
          <!-- 종료 정리 작업 -->
        </onexit>
        <transition event="stop" target="stopping"/>
        <transition event="error" target="error"/>
      </state>
      <state id="stopping">
        <transition event="done.invoke.subprocess" target="final"/>
      </state>
      <state id="error">
        <transition target="final"/>
      </state>
      <final id="final">
        <donedata>
          <content expr="{ status: 'completed' }"/>
        </donedata>
      </final>
    </scxml>)";

  auto model = parser->parseContent(scxml);
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 최종 상태 확인
  IStateNode *finalState = model->findStateById("final");
  ASSERT_TRUE(finalState != nullptr);
  EXPECT_TRUE(finalState->isFinalState());
  EXPECT_FALSE(finalState->getDoneData().isEmpty());
}
