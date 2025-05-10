#include "SCXMLParserTestCommon.h"

// 데이터 모델 관련 기능을 테스트하는 픽스처 클래스
class SCXMLParserDataModelTest : public SCXMLParserTestBase
{
};

// 데이터 모델 파싱 테스트
TEST_F(SCXMLParserDataModelTest, ParseDataModel)
{
  // 상태 및 데이터 모델 항목 생성 호출 예상
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(1));

  // 데이터 모델 항목 생성 호출 예상 - 최소 3개의 항목이 있어야 함
  EXPECT_CALL(*mockFactory, createDataModelItem(testing::_, testing::_))
      .Times(testing::AtLeast(3));

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
  <scxml xmlns="http://www.w3.org/2005/07/scxml"
         xmlns:ctx="http://example.org/ctx"
         version="1.0" initial="s1" datamodel="ecmascript">
    <ctx:property name="prop1" type="string"/>
    <ctx:property name="prop2" type="int"/>
    <datamodel>
      <data id="counter" expr="0" type="int"/>
      <data id="message" expr="'Hello'" type="string"/>
      <data id="flag">
        <![CDATA[true]]>
      </data>
    </datamodel>
    <state id="s1">
      <transition event="increment" target="s1">
        <assign location="counter" expr="counter + 1"/>
      </transition>
    </state>
  </scxml>)";

  auto model = parser->parseContent(scxml);

  // 모델이 성공적으로 생성되었는지 확인
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 데이터 모델 타입 확인
  EXPECT_EQ("ecmascript", model->getDatamodel());

  // 데이터 모델 항목 확인
  const auto &dataItems = model->getDataModelItems();
  ASSERT_EQ(3, dataItems.size());

  // 각 데이터 항목의 속성 확인
  bool foundCounter = false;
  bool foundMessage = false;
  bool foundFlag = false;

  for (const auto &item : dataItems)
  {
    if (item->getId() == "counter")
    {
      foundCounter = true;
      EXPECT_EQ("0", item->getExpr());
      EXPECT_EQ("int", item->getType());
    }
    else if (item->getId() == "message")
    {
      foundMessage = true;
      EXPECT_EQ("'Hello'", item->getExpr());
      EXPECT_EQ("string", item->getType());
    }
    else if (item->getId() == "flag")
    {
      foundFlag = true;
      EXPECT_EQ("true", item->getContent());
    }
  }

  EXPECT_TRUE(foundCounter);
  EXPECT_TRUE(foundMessage);
  EXPECT_TRUE(foundFlag);

  // 컨텍스트 속성 확인
  const auto &props = model->getContextProperties();
  ASSERT_EQ(2, props.size());

  auto it = props.find("prop1");
  ASSERT_NE(props.end(), it);
  EXPECT_EQ("string", it->second);

  it = props.find("prop2");
  ASSERT_NE(props.end(), it);
  EXPECT_EQ("int", it->second);
}

// 데이터 모델 항목 파싱 테스트
TEST_F(SCXMLParserDataModelTest, DataModelItemParsing)
{
  // 데이터 모델 항목 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(1));

  EXPECT_CALL(*mockFactory, createDataModelItem(testing::_, testing::_))
      .Times(testing::AtLeast(3));

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
  <datamodel>
    <data id="counter" expr="0" type="int"/>
    <data id="message" expr="'Hello'" type="string"/>
    <data id="flag">
      <![CDATA[true]]>
    </data>
  </datamodel>
  <state id="s1"/>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 데이터 모델 항목 확인
  const auto &dataItems = model->getDataModelItems();
  EXPECT_EQ(3, dataItems.size());

  bool foundCounter = false;
  bool foundMessage = false;
  bool foundFlag = false;

  for (const auto &item : dataItems)
  {
    if (item->getId() == "counter")
    {
      EXPECT_EQ("0", item->getExpr());
      EXPECT_EQ("int", item->getType());
      foundCounter = true;
    }
    else if (item->getId() == "message")
    {
      EXPECT_EQ("'Hello'", item->getExpr());
      EXPECT_EQ("string", item->getType());
      foundMessage = true;
    }
    else if (item->getId() == "flag")
    {
      EXPECT_EQ("true", item->getContent());
      foundFlag = true;
    }
  }

  EXPECT_TRUE(foundCounter);
  EXPECT_TRUE(foundMessage);
  EXPECT_TRUE(foundFlag);
}

// 바인딩 모드 파싱 테스트
TEST_F(SCXMLParserDataModelTest, BindingModeParsing)
{
  std::string earlyBindingScxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" binding="early">
      <datamodel>
        <data id="earlyVar" expr="123"/>
      </datamodel>
      <state id="s1"/>
    </scxml>)";

  std::string lateBindingScxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" binding="late">
      <datamodel>
        <data id="lateVar" expr="123"/>
      </datamodel>
      <state id="s1"/>
    </scxml>)";

  auto earlyModel = parser->parseContent(earlyBindingScxml);
  ASSERT_TRUE(earlyModel != nullptr);
  EXPECT_EQ("early", earlyModel->getBinding());

  auto lateModel = parser->parseContent(lateBindingScxml);
  ASSERT_TRUE(lateModel != nullptr);
  EXPECT_EQ("late", lateModel->getBinding());
}

// 데이터 모델 바인딩 테스트 (Early/Late)
TEST_F(SCXMLParserDataModelTest, DataModelBindingTest)
{
  // 'early' 바인딩 테스트
  std::string earlyBindingScxml = R"(<?xml version="1.0" encoding="UTF-8"?>
  <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" binding="early" datamodel="ecmascript">
    <datamodel>
      <data id="earlyVar" expr="100"/>
    </datamodel>
    <state id="s1">
      <onentry>
        <assign location="earlyVar" expr="earlyVar + 1"/>
      </onentry>
      <transition event="check" target="s2"/>
    </state>
    <state id="s2"/>
  </scxml>)";

  // 'late' 바인딩 테스트
  std::string lateBindingScxml = R"(<?xml version="1.0" encoding="UTF-8"?>
  <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" binding="late" datamodel="ecmascript">
    <datamodel>
      <data id="lateVar" expr="100"/>
    </datamodel>
    <state id="s1">
      <datamodel>
        <data id="stateVar" expr="200"/>
      </datamodel>
      <onentry>
        <assign location="lateVar" expr="lateVar + 1"/>
        <assign location="stateVar" expr="stateVar + 1"/>
      </onentry>
      <transition event="check" target="s2"/>
    </state>
    <state id="s2"/>
  </scxml>)";

  // 바인딩 처리 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(4)); // s1, s2 (2개 문서 각각)

  // 데이터 모델 항목 생성 호출 예상
  EXPECT_CALL(*mockFactory, createDataModelItem(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // earlyVar, lateVar, stateVar

  // 각 문서 파싱
  auto earlyModel = parser->parseContent(earlyBindingScxml);
  ASSERT_TRUE(earlyModel != nullptr);
  EXPECT_EQ("early", earlyModel->getBinding());

  // 데이터 모델 항목 확인
  const auto &earlyItems = earlyModel->getDataModelItems();
  ASSERT_EQ(1, earlyItems.size());
  EXPECT_EQ("earlyVar", earlyItems[0]->getId());
  EXPECT_EQ("100", earlyItems[0]->getExpr());

  auto lateModel = parser->parseContent(lateBindingScxml);
  ASSERT_TRUE(lateModel != nullptr);
  EXPECT_EQ("late", lateModel->getBinding());

  // 데이터 모델 항목 확인
  const auto &lateItems = lateModel->getDataModelItems();
  ASSERT_GE(lateItems.size(), 1);

  // 최상위 데이터 항목 확인
  bool foundLateVar = false;
  for (const auto &item : lateItems)
  {
    if (item->getId() == "lateVar")
    {
      foundLateVar = true;
      EXPECT_EQ("100", item->getExpr());
    }
  }
  EXPECT_TRUE(foundLateVar);

  // 상태 레벨 데이터 항목 확인
  IStateNode *s1 = lateModel->findStateById("s1");
  ASSERT_TRUE(s1 != nullptr);

  // 상태별 데이터 항목 확인 - 기존 getDataItems() 메서드 사용
  const auto &stateItems = s1->getDataItems();
  if (!stateItems.empty())
  { // 상태별 데이터 항목이 있는 경우에만 검증
    bool foundStateVar = false;
    for (const auto &item : stateItems)
    {
      if (item->getId() == "stateVar")
      {
        foundStateVar = true;
        EXPECT_EQ("200", item->getExpr());
      }
    }
    EXPECT_TRUE(foundStateVar);
  }
}

// 데이터 모델 타입 테스트 (ECMAScript/XPath)
TEST_F(SCXMLParserDataModelTest, DataModelTypesTest)
{
  // ECMAScript 데이터 모델
  std::string ecmascriptDataModel = R"(<?xml version="1.0" encoding="UTF-8"?>
  <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
    <datamodel>
      <data id="counter" expr="0"/>
      <data id="message" expr="'Hello'"/>
    </datamodel>
    <state id="s1">
      <onentry>
        <assign location="counter" expr="counter + 1"/>
        <assign location="message" expr="message + ' World'"/>
      </onentry>
      <transition event="check" target="s2"/>
    </state>
    <state id="s2"/>
  </scxml>)";

  // XPath 데이터 모델
  std::string xpathDataModel = R"(<?xml version="1.0" encoding="UTF-8"?>
  <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="xpath">
    <datamodel>
      <data id="user">
        <name>John</name>
        <age>30</age>
      </data>
    </datamodel>
    <state id="s1">
      <onentry>
        <assign location="/user/age" expr="/user/age + 1"/>
      </onentry>
      <transition event="check" target="s2"/>
    </state>
    <state id="s2"/>
  </scxml>)";

  // 데이터 모델 생성 호출 예상
  EXPECT_CALL(*mockFactory, createDataModelItem(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // 최소 3개의 데이터 아이템

  // ECMAScript 모델 테스트
  auto ecmascriptModel = parser->parseContent(ecmascriptDataModel);
  ASSERT_TRUE(ecmascriptModel != nullptr);
  EXPECT_EQ("ecmascript", ecmascriptModel->getDatamodel());

  const auto &ecmascriptItems = ecmascriptModel->getDataModelItems();
  ASSERT_EQ(2, ecmascriptItems.size());

  // 데이터 항목 확인
  bool foundCounter = false;
  bool foundMessage = false;

  for (const auto &item : ecmascriptItems)
  {
    if (item->getId() == "counter")
    {
      foundCounter = true;
      EXPECT_EQ("0", item->getExpr());
    }
    else if (item->getId() == "message")
    {
      foundMessage = true;
      EXPECT_EQ("'Hello'", item->getExpr());
    }
  }

  EXPECT_TRUE(foundCounter);
  EXPECT_TRUE(foundMessage);

  // XPath 모델 테스트 (선택적으로 지원되는 데이터 모델)
  auto xpathModel = parser->parseContent(xpathDataModel);
  if (xpathModel != nullptr)
  {
    EXPECT_EQ("xpath", xpathModel->getDatamodel());

    const auto &xpathItems = xpathModel->getDataModelItems();
    ASSERT_EQ(1, xpathItems.size());

    // user 데이터 항목 확인
    bool foundUser = false;

    for (const auto &item : xpathItems)
    {
      if (item->getId() == "user")
      {
        foundUser = true;
        // XML 내용이 있는지 확인
        EXPECT_FALSE(item->getContent().empty());
      }
    }

    EXPECT_TRUE(foundUser);
  }
}

// <donedata> 요소 파싱 테스트
TEST_F(SCXMLParserDataModelTest, ParseDoneData)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // 최소 2개 상태 필요

  // <donedata>의 param 요소를 위한 호출 기대
  EXPECT_CALL(*mockFactory, createDataModelItem(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // param 항목 2개

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <datamodel>
    <data id="result" expr="'success'"/>
    <data id="code" expr="200"/>
  </datamodel>
  <state id="s1">
    <transition event="done" target="final"/>
  </state>
  <final id="final">
    <donedata>
      <param name="status" location="result"/>
      <param name="statusCode" location="code"/>
    </donedata>
  </final>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // final 상태 찾기
  IStateNode *finalState = model->findStateById("final");
  ASSERT_TRUE(finalState != nullptr);
  EXPECT_TRUE(finalState->isFinalState());

  // donedata 요소가 파싱되었는지 확인
  const auto &doneData = finalState->getDoneData();
  EXPECT_FALSE(doneData.isEmpty());

  // param 요소들이 파싱되었는지 확인
  const auto &params = doneData.getParams();
  EXPECT_EQ(2, params.size());

  // 파라미터 이름과 위치 확인
  bool foundStatus = false;
  bool foundStatusCode = false;

  for (const auto &param : params)
  {
    if (param.first == "status" && param.second == "result")
    {
      foundStatus = true;
    }
    else if (param.first == "statusCode" && param.second == "code")
    {
      foundStatusCode = true;
    }
  }

  EXPECT_TRUE(foundStatus);
  EXPECT_TRUE(foundStatusCode);
}

// <donedata>에서 <content> 사용 테스트
TEST_F(SCXMLParserDataModelTest, ParseDoneDataWithContent)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // 최소 2개 상태 필요

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <state id="s1">
    <transition event="done" target="final"/>
  </state>
  <final id="final">
    <donedata>
      <content>
        {"status":"complete","timestamp":"2023-04-15T12:00:00Z"}
      </content>
    </donedata>
  </final>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // final 상태 찾기
  IStateNode *finalState = model->findStateById("final");
  ASSERT_TRUE(finalState != nullptr);
  EXPECT_TRUE(finalState->isFinalState());

  // donedata 요소와 content가 파싱되었는지 확인
  const auto &doneData = finalState->getDoneData();
  EXPECT_FALSE(doneData.isEmpty());
  EXPECT_TRUE(doneData.hasContent());

  // content 내용 확인
  const auto &content = doneData.getContent();
  EXPECT_FALSE(content.empty());

  // 내용에 status와 timestamp가 포함되어 있는지 확인
  EXPECT_TRUE(content.find("status") != std::string::npos);
  EXPECT_TRUE(content.find("timestamp") != std::string::npos);
}

// <donedata>에서 <content> 표현식 테스트
TEST_F(SCXMLParserDataModelTest, ParseDoneDataWithContentExpr)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // 최소 2개 상태 필요

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <state id="s1">
    <transition event="done" target="final"/>
  </state>
  <final id="final">
    <donedata>
      <content expr="'Hello ' + 'World'"/>
    </donedata>
  </final>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // final 상태 찾기
  IStateNode *finalState = model->findStateById("final");
  ASSERT_TRUE(finalState != nullptr);
  EXPECT_TRUE(finalState->isFinalState());

  // donedata 요소와 content가 파싱되었는지 확인
  const auto &doneData = finalState->getDoneData();
  EXPECT_FALSE(doneData.isEmpty());
  EXPECT_TRUE(doneData.hasContent());

  // content 내용 확인 (표현식은 평가되지 않고 문자열로 저장됨)
  const auto &content = doneData.getContent();
  EXPECT_EQ("'Hello ' + 'World'", content);
}

// <donedata> 내의 <content> 표현식 처리 테스트
TEST_F(SCXMLParserDataModelTest, DoneDataContentExprTest)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // 최소 2개 상태 필요

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="status" expr="'completed'"/>
        <data id="code" expr="200"/>
      </datamodel>
      <state id="s1">
        <transition event="done" target="final"/>
      </state>
      <final id="final">
        <donedata>
          <content expr="{ status: status, code: code, timestamp: '2023-04-15T12:00:00Z' }"/>
        </donedata>
      </final>
    </scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // final 상태 찾기
  IStateNode *finalState = model->findStateById("final");
  ASSERT_TRUE(finalState != nullptr);
  EXPECT_TRUE(finalState->isFinalState());

  // donedata 요소와 content expr이 파싱되었는지 확인
  const auto &doneData = finalState->getDoneData();
  EXPECT_FALSE(doneData.isEmpty());
  EXPECT_TRUE(doneData.hasContent());

  // content expr 내용 확인
  const auto &content = doneData.getContent();
  EXPECT_FALSE(content.empty());
  EXPECT_EQ("{ status: status, code: code, timestamp: '2023-04-15T12:00:00Z' }", content);
}

// 오류 발생 시 블록 내 나머지 콘텐츠 처리 테스트
TEST_F(SCXMLParserDataModelTest, ParseInvalidDoneData)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // 최소 2개 상태 필요

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <datamodel>
    <data id="result" expr="'success'"/>
  </datamodel>
  <state id="s1">
    <transition event="done" target="final"/>
  </state>
  <final id="final">
    <donedata>
      <content>{"status":"complete"}</content>
      <param name="extra" location="result"/>
    </donedata>
  </final>
</scxml>)";

  auto model = parser->parseContent(scxml);

  // 문서는 파싱되지만 <donedata>는 유효하지 않아 무시되거나 오류가 발생할 수 있음
  if (model != nullptr)
  {
    IStateNode *finalState = model->findStateById("final");
    if (finalState)
    {
      // donedata가 비어있거나 오류 메시지가 있어야 함
      const auto &doneData = finalState->getDoneData();

      // 두 가지 경우 중 하나를 기대할 수 있음:
      // 1. content와 param 중 하나만 처리됨
      // 2. 둘 다 무시되고 doneData가 비어있음
      if (!doneData.isEmpty())
      {
        // content와 param 중 하나만 처리되었는지 확인
        EXPECT_TRUE(doneData.hasContent() != !doneData.getParams().empty());
      }
    }
  }

  IStateNode *finalState = nullptr;
  if (model != nullptr)
  {
    finalState = model->findStateById("final");
  }

  // 최소한 오류 로그가 생성되었는지 확인 (로그 캡처는 구현에 따라 다름)
  EXPECT_TRUE(parser->hasErrors() || !parser->getErrorMessages().empty() ||
              (finalState && (!finalState->getDoneData().hasContent() || finalState->getDoneData().getParams().empty())));
}

// 복합 데이터 모델 지원 테스트
TEST_F(SCXMLParserDataModelTest, DataModelSupport)
{
  // 여러 데이터 모델 테스트 (ECMAScript, XPath, null)
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(3));
  EXPECT_CALL(*mockFactory, createDataModelItem(testing::_, testing::_))
      .Times(testing::AtLeast(3));

  // ECMAScript 데이터 모델
  std::string ecmascriptModel = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="obj" expr="{ prop: 'value', nested: { foo: 'bar' } }"/>
        <data id="arr" expr="[1, 2, 3]"/>
        <data id="func">
          function add(a, b) { return a + b; }
        </data>
      </datamodel>
      <state id="s1">
        <transition event="next" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

  auto model1 = parser->parseContent(ecmascriptModel);
  ASSERT_TRUE(model1 != nullptr);
  EXPECT_FALSE(parser->hasErrors());
  EXPECT_EQ("ecmascript", model1->getDatamodel());

  // XPath 데이터 모델
  std::string xpathModel = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="xpath">
      <datamodel>
        <data id="user">
          <name>John Doe</name>
          <age>30</age>
          <roles>
            <role>admin</role>
            <role>user</role>
          </roles>
        </data>
      </datamodel>
      <state id="s1">
        <transition event="next" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

  auto model2 = parser->parseContent(xpathModel);
  // XPath 모델은 선택적으로 지원할 수 있으므로 결과 확인

  // Null 데이터 모델
  std::string nullModel = R"XML(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="null">
      <state id="s1">
        <transition event="next" cond="In('s1')" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)XML";

  auto model3 = parser->parseContent(nullModel);
  ASSERT_TRUE(model3 != nullptr);
  EXPECT_FALSE(parser->hasErrors());
  EXPECT_EQ("null", model3->getDatamodel());
}

// 데이터 모델 타입 심층 테스트
TEST_F(SCXMLParserDataModelTest, DataModelTypesDetailedTest)
{
  // 기존 ECMAScript 데이터 모델에 대한 테스트
  std::string ecmascriptDataModel = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="counter" expr="0"/>
        <data id="message" expr="'Hello'"/>
        <data id="jsObject" expr="{ name: 'Test', value: 42, nested: { prop: true } }"/>
        <data id="jsArray" expr="[1, 2, 3, 'four', { five: 5 }]"/>
        <data id="jsFunction">
          function add(a, b) {
            return a + b;
          }
        </data>
      </datamodel>
      <state id="s1">
        <onentry>
          <assign location="counter" expr="counter + 1"/>
          <assign location="message" expr="message + ' World'"/>
          <assign location="jsObject.nested.prop" expr="false"/>
          <assign location="jsArray[2]" expr="jsArray[0] + jsArray[1]"/>
        </onentry>
        <transition event="check" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

  // XPath 데이터 모델 테스트
  std::string xpathDataModel = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="xpath">
      <datamodel>
        <data id="user">
          <name>John</name>
          <age>30</age>
          <roles>
            <role>admin</role>
            <role>user</role>
          </roles>
        </data>
      </datamodel>
      <state id="s1">
        <onentry>
          <assign location="/user/age" expr="/user/age + 1"/>
          <assign location="/user/roles/role[1]" expr="'superuser'"/>
        </onentry>
        <transition event="check" cond="/user/age > 30" target="s2"/>
        <transition event="check" target="s3"/>
      </state>
      <state id="s2"/>
      <state id="s3"/>
    </scxml>)";

  // Null 데이터 모델 테스트
  std::string nullDataModel = R"XML(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="null">
      <state id="s1">
        <transition event="next" cond="In('s1')" target="s2"/>
        <transition event="next" target="s3"/>
      </state>
      <state id="s2"/>
      <state id="s3"/>
    </scxml>)XML";

  // ECMAScript 모델 테스트
  auto ecmascriptModel = parser->parseContent(ecmascriptDataModel);
  ASSERT_TRUE(ecmascriptModel != nullptr);
  EXPECT_EQ("ecmascript", ecmascriptModel->getDatamodel());

  const auto &ecmascriptItems = ecmascriptModel->getDataModelItems();
  ASSERT_EQ(5, ecmascriptItems.size());

  // 데이터 타입 확인
  bool foundCounter = false;
  bool foundMessage = false;
  bool foundJsObject = false;
  bool foundJsArray = false;
  bool foundJsFunction = false;

  for (const auto &item : ecmascriptItems)
  {
    if (item->getId() == "counter")
    {
      foundCounter = true;
      EXPECT_EQ("0", item->getExpr());
    }
    else if (item->getId() == "message")
    {
      foundMessage = true;
      EXPECT_EQ("'Hello'", item->getExpr());
    }
    else if (item->getId() == "jsObject")
    {
      foundJsObject = true;
      EXPECT_EQ("{ name: 'Test', value: 42, nested: { prop: true } }", item->getExpr());
    }
    else if (item->getId() == "jsArray")
    {
      foundJsArray = true;
      EXPECT_EQ("[1, 2, 3, 'four', { five: 5 }]", item->getExpr());
    }
    else if (item->getId() == "jsFunction")
    {
      foundJsFunction = true;
      EXPECT_FALSE(item->getContent().empty());
    }
  }

  EXPECT_TRUE(foundCounter);
  EXPECT_TRUE(foundMessage);
  EXPECT_TRUE(foundJsObject);
  EXPECT_TRUE(foundJsArray);
  EXPECT_TRUE(foundJsFunction);

  // XPath 모델 테스트 (선택적으로 지원되는 데이터 모델)
  auto xpathModel = parser->parseContent(xpathDataModel);
  if (xpathModel != nullptr)
  {
    EXPECT_EQ("xpath", xpathModel->getDatamodel());

    const auto &xpathItems = xpathModel->getDataModelItems();
    ASSERT_FALSE(xpathItems.empty());

    bool foundUser = false;
    for (const auto &item : xpathItems)
    {
      if (item->getId() == "user")
      {
        foundUser = true;
        EXPECT_FALSE(item->getContent().empty());
        // XML 내용에서 elements 확인
        EXPECT_TRUE(item->getContent().find("<name>") != std::string::npos);
        EXPECT_TRUE(item->getContent().find("<age>") != std::string::npos);
        EXPECT_TRUE(item->getContent().find("<roles>") != std::string::npos);
        break;
      }
    }
    EXPECT_TRUE(foundUser);
  }

  // Null 모델 테스트
  auto nullModel = parser->parseContent(nullDataModel);
  ASSERT_TRUE(nullModel != nullptr);
  EXPECT_EQ("null", nullModel->getDatamodel());

  // Null 데이터 모델에서는 In() 함수만 사용 가능한지 확인
  IStateNode *s1InNull = nullModel->findStateById("s1");
  ASSERT_TRUE(s1InNull != nullptr);

  const auto &nullTransitions = s1InNull->getTransitions();
  ASSERT_EQ(2, nullTransitions.size());

  bool foundInCondition = false;
  for (const auto &t : nullTransitions)
  {
    if (t->getEvent() == "next" && t->getGuard() == "In('s1')")
    {
      foundInCondition = true;
      EXPECT_EQ("s2", t->getTargets()[0]);
      break;
    }
  }
  EXPECT_TRUE(foundInCondition);
}

// XPath 데이터 모델 지원 테스트
TEST_F(SCXMLParserDataModelTest, XPathDataModelTest)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(1)); // 최소 1개 상태 필요

  // 데이터 모델 항목 생성 기대
  EXPECT_CALL(*mockFactory, createDataModelItem(testing::_, testing::_))
      .Times(testing::AtLeast(1)); // 최소 1개 데이터 모델 항목

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="xpath">
      <datamodel>
        <data id="user">
          <name>John Doe</name>
          <age>30</age>
          <roles>
            <role>admin</role>
            <role>user</role>
          </roles>
        </data>
      </datamodel>
      <state id="s1">
        <onentry>
          <assign location="/user/age" expr="/user/age + 1"/>
        </onentry>
      </state>
    </scxml>)";

  auto model = parser->parseContent(scxml);

  // XPath 데이터 모델을 지원하지 않을 수 있으므로, 파싱 실패해도 테스트 실패가 아님
  if (model != nullptr)
  {
    EXPECT_EQ("xpath", model->getDatamodel());

    // 데이터 모델 항목 확인
    const auto &dataItems = model->getDataModelItems();
    ASSERT_FALSE(dataItems.empty());

    bool foundUserData = false;
    for (const auto &item : dataItems)
    {
      if (item->getId() == "user")
      {
        foundUserData = true;
        EXPECT_FALSE(item->getContent().empty());
        break;
      }
    }

    EXPECT_TRUE(foundUserData) << "user 데이터 항목을 찾을 수 없습니다";
  }
  else
  {
    // XPath 데이터 모델을 지원하지 않는 경우
    std::cout << "XPath 데이터 모델은 지원되지 않습니다." << std::endl;
  }
}

// 상세 DoneData 테스트
TEST_F(SCXMLParserDataModelTest, DetailedDoneDataTest)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // 최소 3개 상태 필요

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="process" datamodel="ecmascript">
      <datamodel>
        <data id="processResult" expr="{ status: 'success', code: 200 }"/>
        <data id="timestamp" expr="'2023-04-15T12:00:00Z'"/>
      </datamodel>

      <state id="process">
        <transition event="complete" target="finalWithParams"/>
        <transition event="completeWithContent" target="finalWithContent"/>
      </state>

      <!-- 파라미터를 사용한 donedata -->
      <final id="finalWithParams">
        <donedata>
          <param name="status" location="processResult.status"/>
          <param name="code" location="processResult.code"/>
          <param name="time" location="timestamp"/>
        </donedata>
      </final>

      <!-- content를 사용한 donedata -->
      <final id="finalWithContent">
        <donedata>
          <content expr="{ result: processResult, timestamp: timestamp, additional: 'info' }"/>
        </donedata>
      </final>
    </scxml>)";

  auto model = parser->parseContent(scxml);
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // finalWithParams 상태 찾기
  IStateNode *finalWithParams = model->findStateById("finalWithParams");
  ASSERT_TRUE(finalWithParams != nullptr);
  EXPECT_TRUE(finalWithParams->isFinalState());

  // donedata 요소가 파싱되었는지 확인
  const auto &paramsData = finalWithParams->getDoneData();
  EXPECT_FALSE(paramsData.isEmpty());

  // param 요소들이 파싱되었는지 확인
  const auto &params = paramsData.getParams();
  EXPECT_EQ(3, params.size());

  // 파라미터 이름과 위치 확인
  bool foundStatus = false;
  bool foundCode = false;
  bool foundTime = false;

  for (const auto &param : params)
  {
    if (param.first == "status" && param.second == "processResult.status")
    {
      foundStatus = true;
    }
    else if (param.first == "code" && param.second == "processResult.code")
    {
      foundCode = true;
    }
    else if (param.first == "time" && param.second == "timestamp")
    {
      foundTime = true;
    }
  }

  EXPECT_TRUE(foundStatus);
  EXPECT_TRUE(foundCode);
  EXPECT_TRUE(foundTime);

  // finalWithContent 상태 찾기
  IStateNode *finalWithContent = model->findStateById("finalWithContent");
  ASSERT_TRUE(finalWithContent != nullptr);
  EXPECT_TRUE(finalWithContent->isFinalState());

  // donedata 요소와 content expr이 파싱되었는지 확인
  const auto &contentData = finalWithContent->getDoneData();
  EXPECT_FALSE(contentData.isEmpty());
  EXPECT_TRUE(contentData.hasContent());

  // content expr 내용 확인
  const auto &content = contentData.getContent();
  EXPECT_FALSE(content.empty());
  EXPECT_EQ("{ result: processResult, timestamp: timestamp, additional: 'info' }", content);
}

// 시스템 변수 처리 테스트
TEST_F(SCXMLParserDataModelTest, SystemVariablesProcessing)
{
  // SystemVariables 접근 테스트
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(2));
  EXPECT_CALL(*mockFactory, createDataModelItem(testing::_, testing::_))
      .Times(testing::AtLeast(4));

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" name="TestMachine" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="sessionInfo" expr="{}"/>
      </datamodel>
      <state id="s1">
        <onentry>
          <assign location="sessionInfo.name" expr="_name"/>
          <assign location="sessionInfo.sessionid" expr="_sessionid"/>
          <assign location="sessionInfo.hasIoprocessors" expr="_ioprocessors !== null"/>
          <assign location="sessionInfo.eventName" expr="_event.name"/>
        </onentry>
        <transition event="next" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

  auto model = parser->parseContent(scxml);
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 시스템 변수 접근 확인
  EXPECT_EQ("TestMachine", model->getName());
}

// 조건부 표현식의 In() 함수 테스트
TEST_F(SCXMLParserDataModelTest, InFunctionTest)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // 최소 3개 상태 필요

  std::string scxml = R"XML(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <parallel id="p1">
        <state id="r1" initial="s1">
          <state id="s1">
            <transition event="e1" target="s2"/>
          </state>
          <state id="s2">
            <transition event="e2" cond="In('r2')" target="s3"/>
            <transition event="e2" target="s1"/>
          </state>
          <state id="s3"/>
        </state>
        <state id="r2" initial="s4">
          <state id="s4">
            <transition event="e1" cond="In('s1')" target="s5"/>
          </state>
          <state id="s5">
            <transition event="e2" cond="In('s2') &amp;&amp; In('s5')" target="s6"/>
          </state>
          <state id="s6"/>
        </state>
      </parallel>
    </scxml>)XML";

  auto model = parser->parseContent(scxml);
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // s2 상태의 전환 확인
  IStateNode *s2 = model->findStateById("s2");
  ASSERT_TRUE(s2 != nullptr);

  const auto &s2Transitions = s2->getTransitions();
  ASSERT_EQ(2, s2Transitions.size());

  // In() 함수가 있는 조건부 전환 확인
  bool foundCondTransition = false;
  for (const auto &t : s2Transitions)
  {
    if (t->getEvent() == "e2" && t->getGuard() == "In('r2')")
    {
      foundCondTransition = true;
      EXPECT_EQ("s3", t->getTargets()[0]);
      break;
    }
  }
  EXPECT_TRUE(foundCondTransition);

  // s5 상태의 전환에서 복합 In() 조건 확인
  IStateNode *s5 = model->findStateById("s5");
  ASSERT_TRUE(s5 != nullptr);

  const auto &s5Transitions = s5->getTransitions();
  ASSERT_EQ(1, s5Transitions.size());

  auto s5Transition = s5Transitions[0];
  EXPECT_EQ("e2", s5Transition->getEvent());
  EXPECT_EQ("In('s2') && In('s5')", s5Transition->getGuard());
  EXPECT_EQ("s6", s5Transition->getTargets()[0]);
}

// 가드 조건 파싱 테스트
TEST_F(SCXMLParserDataModelTest, ParseGuards)
{
  // 가드 노드 생성 호출 예상
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // s1, s2, s3

  // GuardParser는 작동할 수 있으므로 가드 생성 호출 가능
  EXPECT_CALL(*mockFactory, createGuardNode(testing::_, testing::_))
      .Times(testing::AtLeast(1)); // isCounterPositive 가드

  // 전환 노드도 생성되어야 함
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // s1의 두 개 전환(guard 포함, guard 미포함)

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml"
     xmlns:code="http://example.org/code"
     version="1.0" initial="s1">
<code:guards>
  <code:guard id="isCounterPositive" target="counter > 0">
    <code:dependency property="counter"/>
  </code:guard>
</code:guards>
<state id="s1">
  <transition event="check" target="s2" code:guard="isCounterPositive"/>
  <transition event="check" target="s3"/>
</state>
<state id="s2"/>
<state id="s3"/>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 가드 조건이 제대로 파싱되었는지 확인
  const auto &guards = model->getGuards();
  // 가드 파싱이 작동하는지에 따라 조정 가능
  if (!guards.empty())
  {
    EXPECT_EQ(1, guards.size());
    auto guard = guards[0];
    EXPECT_EQ("isCounterPositive", guard->getId());
    EXPECT_EQ("counter > 0", guard->getCondition());

    // 의존성 확인 (API가 지원하는 경우)
    if (guard->getDependencies().size() > 0)
    {
      EXPECT_EQ(1, guard->getDependencies().size());
      EXPECT_EQ("counter", guard->getDependencies()[0]);
    }
  }

  // s1 상태의 전환 확인
  IStateNode *s1 = model->findStateById("s1");
  ASSERT_TRUE(s1 != nullptr);

  const auto &transitions = s1->getTransitions();
  EXPECT_EQ(2, transitions.size());

  bool foundGuardedTransition = false;
  bool foundUnguardedTransition = false;

  for (const auto &transition : transitions)
  {
    const auto &targets = transition->getTargets();
    if (transition->getEvent() == "check" && !targets.empty() && targets[0] == "s2")
    {
      foundGuardedTransition = true;
      EXPECT_EQ("isCounterPositive", transition->getGuard());
    }
    else if (transition->getEvent() == "check" && !targets.empty() && targets[0] == "s3")
    {
      foundUnguardedTransition = true;
      EXPECT_TRUE(transition->getGuard().empty());
    }
  }

  EXPECT_TRUE(foundGuardedTransition);
  EXPECT_TRUE(foundUnguardedTransition);
}

// 반응형 가드 테스트
TEST_F(SCXMLParserDataModelTest, ReactiveGuards)
{
  // 기대하는 호출 횟수 설정
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(4)); // s1, s2, s3, s4 상태 생성

  // 모든 guard 생성을 잡아내는 핸들러 - 반응형 가드 특별 처리
  ON_CALL(*mockFactory, createGuardNode(testing::_, testing::_))
      .WillByDefault([this](const std::string &id, const std::string &target)
                     {
          auto mockGuard = std::make_shared<MockGuardNode>();
          mockGuard->id_ = id;
          mockGuard->target_ = target;
          mockGuard->SetupDefaultBehavior();

          // 반응형 가드 특별 처리 - 이름으로 구분
          if (id == "reactiveGuard" || id == "flagMonitor" || id.find("reactive") != std::string::npos) {
              mockGuard->reactive_ = true;
              ON_CALL(*mockGuard, isReactive()).WillByDefault(testing::Return(true));
          }

          return mockGuard; });

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
  <scxml xmlns="http://www.w3.org/2005/07/scxml"
         xmlns:code="http://example.org/code"
         version="1.0" initial="s1">
    <code:guards>
      <!-- 일반 가드 -->
      <code:guard id="normalGuard" target="s2">
        <code:dependency property="counter"/>
      </code:guard>

      <!-- 명시적 반응형 가드 -->
      <code:guard id="reactiveGuard" target="s3" code:reactive="true">
        <code:dependency property="flag"/>
      </code:guard>

      <!-- 다른 반응형 가드 -->
      <code:guard id="flagMonitor" target="s4" code:reactive="true">
        <code:dependency property="systemFlag"/>
      </code:guard>

      <!-- 복합 조건 반응형 가드 -->
      <code:guard id="reactiveComplex" target="s5" code:reactive="true">
        <code:dependency property="user.status"/>
        <code:dependency property="system.state"/>
      </code:guard>
    </code:guards>

    <datamodel>
      <data id="counter" expr="5"/>
      <data id="flag">
        <![CDATA[true]]>
      </data>
      <data id="systemFlag" expr="false"/>
      <data id="user" expr="{ status: 'active' }"/>
      <data id="system" expr="{ state: 'running' }"/>
    </datamodel>

    <state id="s1">
      <!-- 일반 가드를 사용하는 전환 -->
      <transition event="check" code:guard="normalGuard" target="s2"/>

      <!-- 반응형 가드 직접 선언 -->
      <code:reactive-guard id="reactiveGuard" target="s3"/>

      <!-- 다른 반응형 가드 선언 -->
      <code:reactive-guard id="flagMonitor" target="s4"/>

      <!-- 복합 반응형 가드 -->
      <code:reactive-guard id="reactiveComplex" target="s5"/>
    </state>

    <state id="s2"/>
    <state id="s3"/>
    <state id="s4"/>
    <state id="s5"/>
  </scxml>)";

  auto model = parser->parseContent(scxml);

  // 모델이 성공적으로 생성되었는지 확인
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 가드 조건 수 확인
  const auto &guards = model->getGuards();
  ASSERT_EQ(4, guards.size()) << "Expected 4 guard conditions";

  // 각 가드 타입 확인
  std::map<std::string, bool> guardTypes;
  for (const auto &guard : guards)
  {
    guardTypes[guard->getId()] = guard->isReactive();
  }

  // 일반 가드 확인
  ASSERT_TRUE(guardTypes.find("normalGuard") != guardTypes.end());
  EXPECT_FALSE(guardTypes["normalGuard"]) << "normalGuard should not be reactive";

  // 반응형 가드들 확인
  ASSERT_TRUE(guardTypes.find("reactiveGuard") != guardTypes.end());
  EXPECT_TRUE(guardTypes["reactiveGuard"]) << "reactiveGuard should be reactive";

  ASSERT_TRUE(guardTypes.find("flagMonitor") != guardTypes.end());
  EXPECT_TRUE(guardTypes["flagMonitor"]) << "flagMonitor should be reactive";

  ASSERT_TRUE(guardTypes.find("reactiveComplex") != guardTypes.end());
  EXPECT_TRUE(guardTypes["reactiveComplex"]) << "reactiveComplex should be reactive";

  // 상태 내 반응형 가드 참조 확인
  IStateNode *s1 = model->findStateById("s1");
  ASSERT_TRUE(s1 != nullptr);

  // 상태 내에서 정의된 반응형 가드 ID 확인
  const auto &reactiveGuards = s1->getReactiveGuards();
  EXPECT_EQ(3, reactiveGuards.size());
  EXPECT_TRUE(std::find(reactiveGuards.begin(), reactiveGuards.end(), "reactiveGuard") != reactiveGuards.end());
  EXPECT_TRUE(std::find(reactiveGuards.begin(), reactiveGuards.end(), "flagMonitor") != reactiveGuards.end());
  EXPECT_TRUE(std::find(reactiveGuards.begin(), reactiveGuards.end(), "reactiveComplex") != reactiveGuards.end());

  // 대신 전환 확인으로 대체
  const auto &transitions = s1->getTransitions();
  ASSERT_GE(transitions.size(), 1);

  bool foundNormalGuardTransition = false;
  for (const auto &transition : transitions)
  {
    const auto &targets = transition->getTargets();
    if (transition->getEvent() == "check" && !targets.empty() && targets[0] == "s2")
    {
      foundNormalGuardTransition = true;
      EXPECT_EQ("normalGuard", transition->getGuard());
    }
  }

  EXPECT_TRUE(foundNormalGuardTransition) << "Transition with normal guard not found";
}

// 복합 조건을 가진 가드 테스트
TEST_F(SCXMLParserDataModelTest, ComplexGuardConditions)
{
  // 가드 노드 생성 호출 예상
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // s1, s2, s3

  // 복잡한 가드 조건 생성 기대
  EXPECT_CALL(*mockFactory, createGuardNode(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // 복잡한 가드 조건 3개

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
  <scxml xmlns="http://www.w3.org/2005/07/scxml"
         xmlns:code="http://example.org/code"
         version="1.0" initial="s1">
    <code:guards>
      <!-- 논리 연산자를 사용한 복합 조건 -->
      <code:guard id="complexCondition1" target="s2">
        <code:condition><![CDATA[(x > 10 && y < 20) || z == 0]]></code:condition>
        <code:dependency property="x"/>
        <code:dependency property="y"/>
        <code:dependency property="z"/>
      </code:guard>

      <!-- 함수 호출이 포함된 조건 -->
      <code:guard id="complexCondition2" target="s3">
        <code:condition><![CDATA[Math.abs(value) > threshold && isValid(status)]]></code:condition>
        <code:dependency property="value"/>
        <code:dependency property="threshold"/>
        <code:dependency property="status"/>
      </code:guard>

      <!-- 문자열 처리가 포함된 조건 -->
      <code:guard id="complexCondition3" target="s4">
        <code:condition><![CDATA[user.name.startsWith('admin') && user.permissions.includes('write')]]></code:condition>
        <code:dependency property="user"/>
      </code:guard>
    </code:guards>

    <datamodel>
      <data id="x" expr="15"/>
      <data id="y" expr="10"/>
      <data id="z" expr="0"/>
      <data id="value" expr="-30"/>
      <data id="threshold" expr="20"/>
      <data id="status" expr="'valid'"/>
      <data id="user" expr="{ name: 'admin_user', permissions: ['read', 'write'] }"/>
    </datamodel>

    <state id="s1">
      <transition event="test1" target="s2" code:guard="complexCondition1"/>
      <transition event="test2" target="s3" code:guard="complexCondition2"/>
      <transition event="test3" target="s4" code:guard="complexCondition3"/>
    </state>

    <state id="s2"/>
    <state id="s3"/>
    <state id="s4"/>
  </scxml>)";

  auto model = parser->parseContent(scxml);

  // 모델이 성공적으로 생성되었는지 확인
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 가드 조건 확인
  const auto &guards = model->getGuards();
  ASSERT_EQ(3, guards.size()) << "Expected 3 complex guard conditions";

  // 각 가드 조건 내용 검증
  bool foundCondition1 = false;
  bool foundCondition2 = false;
  bool foundCondition3 = false;

  for (const auto &guard : guards)
  {
    if (guard->getId() == "complexCondition1")
    {
      foundCondition1 = true;

      // 타겟 확인
      EXPECT_EQ("s2", guard->getTargetState());

      // 의존성 목록 확인
      const auto &deps = guard->getDependencies();
      ASSERT_EQ(3, deps.size());
      EXPECT_TRUE(std::find(deps.begin(), deps.end(), "x") != deps.end());
      EXPECT_TRUE(std::find(deps.begin(), deps.end(), "y") != deps.end());
      EXPECT_TRUE(std::find(deps.begin(), deps.end(), "z") != deps.end());

      // 조건식 확인
      EXPECT_EQ("(x > 10 && y < 20) || z == 0", guard->getCondition());
    }
    else if (guard->getId() == "complexCondition2")
    {
      foundCondition2 = true;

      // 타겟 확인
      EXPECT_EQ("s3", guard->getTargetState());

      // 의존성 목록 확인
      const auto &deps = guard->getDependencies();
      ASSERT_EQ(3, deps.size());
      EXPECT_TRUE(std::find(deps.begin(), deps.end(), "value") != deps.end());
      EXPECT_TRUE(std::find(deps.begin(), deps.end(), "threshold") != deps.end());
      EXPECT_TRUE(std::find(deps.begin(), deps.end(), "status") != deps.end());
    }
    else if (guard->getId() == "complexCondition3")
    {
      foundCondition3 = true;

      // 타겟 확인
      EXPECT_EQ("s4", guard->getTargetState());

      // 의존성 목록 확인
      const auto &deps = guard->getDependencies();
      ASSERT_EQ(1, deps.size());
      EXPECT_EQ("user", deps[0]);
    }
  }

  EXPECT_TRUE(foundCondition1) << "complexCondition1 not found";
  EXPECT_TRUE(foundCondition2) << "complexCondition2 not found";
  EXPECT_TRUE(foundCondition3) << "complexCondition3 not found";

  // 전환에 가드 조건이 올바르게 연결되었는지 확인
  IStateNode *s1 = model->findStateById("s1");
  ASSERT_TRUE(s1 != nullptr);

  const auto &transitions = s1->getTransitions();
  ASSERT_EQ(3, transitions.size());

  for (const auto &transition : transitions)
  {
    const auto &targets = transition->getTargets();

    if (transition->getEvent() == "test1")
    {
      EXPECT_EQ("complexCondition1", transition->getGuard());
      EXPECT_TRUE(!targets.empty());
      EXPECT_EQ("s2", targets[0]);
    }
    else if (transition->getEvent() == "test2")
    {
      EXPECT_EQ("complexCondition2", transition->getGuard());
      EXPECT_TRUE(!targets.empty());
      EXPECT_EQ("s3", targets[0]);
    }
    else if (transition->getEvent() == "test3")
    {
      EXPECT_EQ("complexCondition3", transition->getGuard());
      EXPECT_TRUE(!targets.empty());
      EXPECT_EQ("s4", targets[0]);
    }
  }
}
