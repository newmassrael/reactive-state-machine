
#include "SCXMLParserTestCommon.h"

// 상태 관련 기능을 테스트하는 픽스처 클래스
class SCXMLParserInvokeTest : public SCXMLParserTestBase
{
};

// 인보크 세부 기능 테스트
TEST_F(SCXMLParserInvokeTest, InvokeDetailedTest)
{
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <invoke id="childProcess" type="http://www.w3.org/TR/scxml/" src="childMachine.scxml" autoforward="true">
          <param name="initialValue" expr="100"/>
          <finalize>
            <assign location="result" expr="_event.data.answer"/>
          </finalize>
        </invoke>
        <transition event="childProcess.done" target="s2"/>
        <transition event="error" target="error"/>
      </state>
      <state id="s2"/>
      <state id="error"/>
    </scxml>)";

    // 인보크 노드 생성 기대
    EXPECT_CALL(*mockFactory, createInvokeNode(testing::_))
        .Times(testing::AtLeast(1));

    // 파라미터 노드 생성 기대
    EXPECT_CALL(*mockFactory, createDataModelItem(testing::_, testing::_))
        .Times(testing::AtLeast(1));

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // 인보크 요소 확인
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);
    EXPECT_FALSE(s1->getInvoke().empty());

    // 인보크 정보 확인
    auto invoke = s1->getInvoke()[0];
    EXPECT_EQ("childProcess", invoke->getId());
    EXPECT_EQ("http://www.w3.org/TR/scxml/", invoke->getType());
    EXPECT_EQ("childMachine.scxml", invoke->getSrc());
    EXPECT_TRUE(invoke->isAutoForward());
}

// 자식 머신과의 통신 테스트 (invoke와 send의 상호작용)
TEST_F(SCXMLParserInvokeTest, InvokeAndSendInteractionTest)
{
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="childId" expr="''"/>
      </datamodel>
      <state id="s1">
        <invoke id="child" type="http://www.w3.org/TR/scxml/" src="child.scxml" idlocation="childId">
          <param name="startValue" expr="100"/>
        </invoke>
        <transition event="sendToChild" target="s2">
          <send targetexpr="'#_' + childId" type="http://www.w3.org/TR/scxml/" event="update">
            <param name="newValue" expr="200"/>
          </send>
        </transition>
        <transition event="done.invoke.child" target="s3"/>
      </state>
      <state id="s2">
        <transition event="done.invoke.child" target="s3"/>
      </state>
      <state id="s3"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // invoke 요소 확인
    ASSERT_FALSE(s1->getInvoke().empty());
    auto invoke = s1->getInvoke()[0];

    // 기본 속성 확인
    EXPECT_EQ("child", invoke->getId());
    EXPECT_EQ("http://www.w3.org/TR/scxml/", invoke->getType());
    EXPECT_EQ("child.scxml", invoke->getSrc());

    // idlocation 속성 확인
    EXPECT_EQ("childId", invoke->getIdLocation());

    // datamodel에 childId 데이터 항목이 있는지 확인
    bool foundChildIdData = false;
    for (const auto &dataItem : model->getDataModelItems())
    {
        if (dataItem->getId() == "childId")
        {
            foundChildIdData = true;
            EXPECT_EQ("''", dataItem->getExpr());
            break;
        }
    }
    EXPECT_TRUE(foundChildIdData);

    // param 요소 확인
    ASSERT_FALSE(invoke->getParams().empty());
    auto param = invoke->getParams()[0];
    // Use std::get to access tuple elements by index instead of trying to call methods
    EXPECT_EQ("startValue", std::get<0>(param)); // First element is name
    EXPECT_EQ("100", std::get<1>(param));        // Second element is expr
}

// 자동 이벤트 전달(Autoforwarding)과 <invoke> 상호작용 테스트
TEST_F(SCXMLParserInvokeTest, InvokeAutoforwardingTest)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(2)); // 최소 2개 상태 필요

    // invoke 노드 생성 기대
    EXPECT_CALL(*mockFactory, createInvokeNode(testing::_))
        .Times(testing::AtLeast(1)); // 최소 1개 invoke 노드

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <invoke id="childProcess" type="http://www.w3.org/TR/scxml/" src="child.scxml" autoforward="true">
          <param name="initialValue" expr="100"/>
        </invoke>
        <transition event="childProcess.done" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // invoke 요소 확인
    ASSERT_FALSE(s1->getInvoke().empty());
    auto invoke = s1->getInvoke()[0];

    // autoforward 속성 확인
    EXPECT_TRUE(invoke->isAutoForward());
    EXPECT_EQ("childProcess", invoke->getId());
    EXPECT_EQ("http://www.w3.org/TR/scxml/", invoke->getType());
    EXPECT_EQ("child.scxml", invoke->getSrc());
}

// Invoke와 Finalize 테스트
TEST_F(SCXMLParserInvokeTest, DetailedInvokeFinalizeTest)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(3)); // 최소 3개 상태 필요

    // invoke 노드 생성 기대
    EXPECT_CALL(*mockFactory, createInvokeNode(testing::_))
        .Times(testing::AtLeast(2)); // 최소 2개 invoke 노드

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="childData" expr="{ input: 100 }"/>
        <data id="childId" expr="''"/>
        <data id="response" expr="null"/>
        <data id="processedData" expr="null"/>
      </datamodel>

      <state id="s1">
        <!-- 기본 SCXML 인보크 -->
        <invoke id="child1" type="http://www.w3.org/TR/scxml/" src="childProcess.scxml" idlocation="childId">
          <param name="initialValue" expr="childData.input"/>
          <finalize>
            <assign location="response" expr="_event.data"/>
            <script>
              // 응답 데이터 처리
              processedData = {
                result: response.result * 2,
                timestamp: new Date().toISOString()
              };
            </script>
          </finalize>
        </invoke>

        <!-- 자동 전달 설정이 있는 인보크 -->
        <invoke id="child2" type="http://www.w3.org/TR/scxml/" autoforward="true">
          <content>
            <scxml version="1.0" initial="subInitial">
              <state id="subInitial">
                <transition event="forward.event" target="subFinal"/>
              </state>
              <final id="subFinal"/>
            </scxml>
          </content>
        </invoke>

        <!-- 인보크에서 반환된 이벤트에 대한 전환 -->
        <transition event="done.invoke.child1" target="s2"/>
        <transition event="done.invoke.child2" target="s3"/>
        <transition event="error" target="error"/>
      </state>

      <state id="s2"/>
      <state id="s3"/>
      <state id="error"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // invoke 요소 확인
    ASSERT_EQ(2, s1->getInvoke().size());

    // 첫 번째 invoke 확인
    auto invoke1 = s1->getInvoke()[0];
    EXPECT_EQ("child1", invoke1->getId());
    EXPECT_EQ("http://www.w3.org/TR/scxml/", invoke1->getType());
    EXPECT_EQ("childProcess.scxml", invoke1->getSrc());
    EXPECT_EQ("childId", invoke1->getIdLocation());
    EXPECT_FALSE(invoke1->isAutoForward());

    // 파라미터 확인
    ASSERT_EQ(1, invoke1->getParams().size());
    auto param1 = invoke1->getParams()[0];
    EXPECT_EQ("initialValue", std::get<0>(param1));
    EXPECT_EQ("childData.input", std::get<1>(param1));

    // finalize 요소 확인
    EXPECT_FALSE(invoke1->getFinalize().empty());

    // 두 번째 invoke 확인
    auto invoke2 = s1->getInvoke()[1];
    EXPECT_EQ("child2", invoke2->getId());
    EXPECT_EQ("http://www.w3.org/TR/scxml/", invoke2->getType());
    EXPECT_TRUE(invoke2->isAutoForward());

    // content 요소 확인
    EXPECT_FALSE(invoke2->getContent().empty());
    EXPECT_TRUE(invoke2->getContent().find("<scxml") != std::string::npos);
    EXPECT_TRUE(invoke2->getContent().find("subInitial") != std::string::npos);

    // 전환 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_EQ(3, transitions.size());

    bool foundChild1Transition = false;
    bool foundChild2Transition = false;
    bool foundErrorTransition = false;

    for (const auto &t : transitions)
    {
        if (t->getEvent() == "done.invoke.child1")
        {
            foundChild1Transition = true;
            EXPECT_EQ("s2", t->getTargets()[0]);
        }
        else if (t->getEvent() == "done.invoke.child2")
        {
            foundChild2Transition = true;
            EXPECT_EQ("s3", t->getTargets()[0]);
        }
        else if (t->getEvent() == "error")
        {
            foundErrorTransition = true;
            EXPECT_EQ("error", t->getTargets()[0]);
        }
    }

    EXPECT_TRUE(foundChild1Transition);
    EXPECT_TRUE(foundChild2Transition);
    EXPECT_TRUE(foundErrorTransition);
}
