#include "SCXMLParserTestCommon.h"

class SCXMLParserCommunicationTest : public SCXMLParserTestBase
{
};

// 외부 통신 요소 테스트 (send/cancel)
TEST_F(SCXMLParserCommunicationTest, CommunicationElementsTest)
{
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <onentry>
          <send id="timer" event="timeout" delay="5s"/>
          <send id="msg" event="message" target="#_internal">
            <content>Internal message content</content>
          </send>
        </onentry>
        <transition event="cancel" target="s2">
          <cancel sendid="timer"/>
        </transition>
        <transition event="timeout" target="s3"/>
        <transition event="message" target="s4"/>
      </state>
      <state id="s2"/>
      <state id="s3"/>
      <state id="s4"/>
    </scxml>)";

    // 통신 관련 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(4)); // s1, s2, s3, s4

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // 상태 및 전환 수 확인
    const auto &allStates = model->getAllStates();
    EXPECT_EQ(4, allStates.size());
}

// 외부 통신 요소 파싱 테스트
TEST_F(SCXMLParserCommunicationTest, ExternalCommunicationParsing)
{
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <onentry>
          <send id="send1" event="external.event" target="http://example.org" type="http"/>
          <send id="send2" eventexpr="'dynamic.event'" delay="1s">
            <content>Hello World</content>
          </send>
        </onentry>
        <transition event="response" target="s2">
          <cancel sendid="send1"/>
        </transition>
      </state>
      <state id="s2">
        <invoke id="inv1" type="http://www.w3.org/TR/scxml/" src="child.scxml" autoforward="true">
          <finalize>
            <log expr="'Finalizing invoke'"/>
          </finalize>
        </invoke>
      </state>
    </scxml>)";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);

    // send, cancel, invoke, finalize 요소가 올바르게 파싱되었는지 확인
    IStateNode *s2 = model->findStateById("s2");
    ASSERT_TRUE(s2 != nullptr);
    ASSERT_FALSE(s2->getInvoke().empty());
    EXPECT_EQ("inv1", s2->getInvoke()[0]->getId());
    EXPECT_EQ("http://www.w3.org/TR/scxml/", s2->getInvoke()[0]->getType());
    EXPECT_EQ("child.scxml", s2->getInvoke()[0]->getSrc());
    EXPECT_TRUE(s2->getInvoke()[0]->isAutoForward());
}

// SCXML 이벤트 I/O 프로세서 파싱 테스트
TEST_F(SCXMLParserCommunicationTest, SCXMLEventIOProcessor)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(2)); // 최소 2개 상태 필요

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <datamodel>
    <data id="targetId" expr="'#_internal'"/>
  </datamodel>

  <state id="s1">
    <onentry>
      <!-- 내부 이벤트 발생 (특수 타겟 #_internal) -->
      <send event="internal.event" target="#_internal"/>

      <!-- 동적 타겟 표현식 사용 -->
      <send event="dynamic.event" targetexpr="targetId"/>

      <!-- 특수 타겟 #_parent (부모 세션으로 이벤트 전송) -->
      <send event="parent.event" target="#_parent"/>
    </onentry>
    <transition event="internal.event" target="s2"/>
  </state>
  <state id="s2"/>
</scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // onentry에 send 요소들이 파싱되었는지 확인
    EXPECT_FALSE(s1->getOnEntry().empty());

    // 전환 이벤트 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_EQ(1, transitions.size());
    EXPECT_EQ("internal.event", transitions[0]->getEvent());
    EXPECT_EQ("s2", transitions[0]->getTargets()[0]);
}

// SCXML Event I/O Processor 통합 테스트
TEST_F(SCXMLParserCommunicationTest, SCXMLEventIOProcessorIntegration)
{
    // SCXML Event I/O 프로세서 테스트
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(2));
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(4));

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <datamodel>
        <data id="targetSession" expr="'#_scxml_session123'"/>
        <data id="payload" expr="{ status: 'ready', data: [1, 2, 3] }"/>
      </datamodel>
      <state id="s1">
        <onentry>
          <!-- 내부 이벤트 발생 -->
          <send target="#_internal" event="internal.notification" namelist="payload"/>

          <!-- 특정 세션에 이벤트 전송 -->
          <send targetexpr="targetSession" event="external.update" namelist="payload"/>

          <!-- 부모 세션에 이벤트 전송 -->
          <send target="#_parent" event="child.response">
            <content expr="payload"/>
          </send>

          <!-- 생성된 하위 세션에 이벤트 전송 -->
          <send target="#_invoke1" event="control.pause"/>
        </onentry>
        <transition event="next" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // 이벤트 I/O 프로세서 타겟 처리 확인
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);
    EXPECT_FALSE(s1->getOnEntry().empty());
}

// HTTP Event I/O Processor 테스트
TEST_F(SCXMLParserCommunicationTest, HTTPEventIOProcessorIntegration)
{
    // HTTP Event I/O 프로세서 테스트
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(2));
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(2));

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <datamodel>
        <data id="apiEndpoint" expr="'https://api.example.com/events'"/>
        <data id="userData" expr="{ userId: 'user123', action: 'login' }"/>
      </datamodel>
      <state id="s1">
        <onentry>
          <!-- HTTP POST 요청 전송 -->
          <send target="https://api.example.com/webhook"
                type="http://www.w3.org/TR/scxml/#BasicHTTPEventProcessor"
                event="api.notification"
                namelist="userData"/>

          <!-- 동적 대상 HTTP 요청 -->
          <send targetexpr="apiEndpoint"
                type="http://www.w3.org/TR/scxml/#BasicHTTPEventProcessor"
                event="api.update">
            <content expr="userData"/>
          </send>
        </onentry>
        <transition event="next" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // HTTP 이벤트 I/O 프로세서 구성 확인
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);
    EXPECT_FALSE(s1->getOnEntry().empty());
}

// <raise> 요소 파싱 테스트
TEST_F(SCXMLParserCommunicationTest, RaiseElementParsing)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(2)); // 최소 2개 상태 필요

    // 액션 노드 생성 기대
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(1)); // raise 액션

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <state id="s1">
    <onentry>
      <!-- 내부 이벤트 발생 (raise 요소 사용) -->
      <raise event="internal.raised.event"/>
    </onentry>
    <transition event="internal.raised.event" target="s2"/>
  </state>
  <state id="s2"/>
</scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // onentry에 raise 요소가 파싱되었는지 확인
    EXPECT_FALSE(s1->getOnEntry().empty());

    // 전환 이벤트 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_EQ(1, transitions.size());
    EXPECT_EQ("internal.raised.event", transitions[0]->getEvent());
    EXPECT_EQ("s2", transitions[0]->getTargets()[0]);
}
