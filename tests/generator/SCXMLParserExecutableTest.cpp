#include "SCXMLParserTestCommon.h"

// 기본 테스트 픽스처 상속
class SCXMLParserExecutableTest : public SCXMLParserTestBase
{
};

// 실행 가능 콘텐츠 파싱 테스트
TEST_F(SCXMLParserExecutableTest, ExecutableContentParsing)
{
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <onentry>
          <raise event="internal.event"/>
          <log expr="'Entering state'"/>
          <if cond="true">
            <log expr="'Condition is true'"/>
            <elseif cond="false"/>
            <log expr="'Second condition'"/>
            <else/>
            <log expr="'No condition is true'"/>
          </if>
          <foreach item="item" index="idx" array="[1,2,3]">
            <log expr="'Item: ' + item"/>
          </foreach>
        </onentry>
        <transition event="next" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);

    // OnEntry 액션에 실행 가능 콘텐츠가 올바르게 파싱되었는지 확인
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);
    EXPECT_FALSE(s1->getOnEntry().empty());
}

// 실행 콘텐츠 파싱 테스트 (foreach, if/else)
TEST_F(SCXMLParserExecutableTest, ExecutableContentTest)
{
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="items" expr="[1, 2, 3, 4, 5]"/>
        <data id="sum" expr="0"/>
        <data id="condition" expr="true"/>
      </datamodel>
      <state id="s1">
        <onentry>
          <foreach item="item" array="items" index="idx">
            <assign location="sum" expr="sum + item"/>
          </foreach>
          <if cond="sum > 10">
            <assign location="result" expr="'Greater than 10'"/>
            <elseif cond="sum == 10"/>
            <assign location="result" expr="'Equal to 10'"/>
            <else/>
            <assign location="result" expr="'Less than 10'"/>
          </if>
        </onentry>
        <transition event="next" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

    // 액션 노드 생성 기대
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(5)); // foreach, if, elseif, else, assign 등

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());
}

// 중첩 및 복합 조건 테스트
TEST_F(SCXMLParserExecutableTest, ComplexNestedConditionsTest)
{
    // 액션 노드 생성 기대
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(5)); // 최소 5개 액션 노드

    std::string scxml = R"XML(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="x" expr="5"/>
        <data id="y" expr="10"/>
        <data id="result" expr="''"/>
      </datamodel>

      <state id="s1">
        <onentry>
          <!-- 중첩된 if-elseif-else 구문 -->
          <if cond="x &lt; 0">
            <assign location="result" expr="'negative'"/>
            <elseif cond="x == 0"/>
            <assign location="result" expr="'zero'"/>
            <elseif cond="x &gt; 0 &amp;&amp; x &lt; 10"/>
            <assign location="result" expr="'small positive'"/>
            <else/>
            <assign location="result" expr="'large positive'"/>
          </if>

          <!-- 중첩된 if 구문 -->
          <if cond="x &gt; 0">
            <if cond="y &gt; 0">
              <assign location="result" expr="'both positive'"/>
            </if>
          </if>
        </onentry>

        <!-- 복합 조건식을 가진 전환 -->
        <transition event="check" cond="(x &gt; 0 &amp;&amp; y &gt; 0) || (x &lt; 0 &amp;&amp; y &lt; 0)" target="s2"/>
        <transition event="check" cond="(x &gt; 0 &amp;&amp; y &lt; 0) || (x &lt; 0 &amp;&amp; y &gt; 0)" target="s3"/>
        <transition event="check" target="s4"/>
      </state>

      <state id="s2"/>
      <state id="s3"/>
      <state id="s4"/>
    </scxml>)XML";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // onentry에 중첩된 if 요소가 파싱되었는지 확인
    EXPECT_FALSE(s1->getOnEntry().empty());

    // 복합 조건을 가진 전환 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_EQ(3, transitions.size());

    // 첫 번째 복합 조건 전환 확인
    EXPECT_EQ("check", transitions[0]->getEvent());
    EXPECT_EQ("(x > 0 && y > 0) || (x < 0 && y < 0)", transitions[0]->getGuard());
    EXPECT_EQ("s2", transitions[0]->getTargets()[0]);

    // 두 번째 복합 조건 전환 확인
    EXPECT_EQ("check", transitions[1]->getEvent());
    EXPECT_EQ("(x > 0 && y < 0) || (x < 0 && y > 0)", transitions[1]->getGuard());
    EXPECT_EQ("s3", transitions[1]->getTargets()[0]);
}

// <foreach> 요소 파싱 테스트
TEST_F(SCXMLParserExecutableTest, ForeachElementParsing)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(1)); // 최소 1개 상태 필요

    // 액션 노드 생성 기대
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(2)); // foreach 및 내부 액션

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
  <datamodel>
    <data id="items" expr="[1, 2, 3, 4, 5]"/>
  </datamodel>

  <state id="s1">
    <onentry>
      <!-- foreach 요소 사용 -->
      <foreach item="currentItem" index="idx" array="items">
        <log expr="'Processing item ' + currentItem + ' at index ' + idx"/>
      </foreach>
    </onentry>
  </state>
</scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // onentry에 foreach 요소가 파싱되었는지 확인
    EXPECT_FALSE(s1->getOnEntry().empty());
}

// Foreach 반복 테스트
TEST_F(SCXMLParserExecutableTest, DetailedForeachTest)
{
    // 액션 노드 생성 기대
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(2)); // 최소 2개 액션 노드

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="items" expr="[1, 2, 3, 4, 5]"/>
        <data id="sum" expr="0"/>
        <data id="itemStr" expr="''"/>
        <data id="objItems" expr="[{id: 'a', value: 10}, {id: 'b', value: 20}, {id: 'c', value: 30}]"/>
        <data id="objSum" expr="0"/>
      </datamodel>

      <state id="s1">
        <onentry>
          <!-- 기본 foreach - 배열 항목 합계 -->
          <foreach item="item" index="idx" array="items">
            <assign location="sum" expr="sum + item"/>
            <assign location="itemStr" expr="itemStr + (idx > 0 ? ',' : '') + item"/>
          </foreach>

          <!-- 객체 배열 foreach -->
          <foreach item="obj" array="objItems">
            <assign location="objSum" expr="objSum + obj.value"/>
          </foreach>
        </onentry>

        <!-- 합계 확인 전환 -->
        <transition event="check" cond="sum == 15 &amp;&amp; objSum == 60" target="pass"/>
        <transition event="check" target="fail"/>
      </state>

      <state id="pass"/>
      <state id="fail"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // onentry에 foreach 요소가 파싱되었는지 확인
    EXPECT_FALSE(s1->getOnEntry().empty());

    // 조건부 전환 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_EQ(2, transitions.size());

    // 첫 번째 전환 확인
    EXPECT_EQ("check", transitions[0]->getEvent());
    EXPECT_EQ("sum == 15 && objSum == 60", transitions[0]->getGuard());
    EXPECT_EQ("pass", transitions[0]->getTargets()[0]);
}

// <foreach>의 변경 불가능한 복사본 처리 테스트
TEST_F(SCXMLParserExecutableTest, ForeachImmutableCopyTest)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(1)); // 최소 1개 상태 필요

    // 액션 노드 생성 기대
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(1)); // foreach 액션

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="items" expr="[1, 2, 3, 4, 5]"/>
        <data id="sum" expr="0"/>
      </datamodel>
      <state id="s1">
        <onentry>
          <!-- 원본 배열의 얕은 복사본을 사용하는 foreach -->
          <foreach item="item" array="items" index="idx">
            <assign location="sum" expr="sum + item"/>
            <!-- 반복 중 배열 수정 시도 (영향을 주지 않아야 함) -->
            <assign location="items[idx]" expr="0"/>
          </foreach>
        </onentry>
      </state>
    </scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // onentry 핸들러에 foreach 요소가 있는지 확인
    EXPECT_FALSE(s1->getOnEntry().empty());
}

// <if>, <elseif>, <else> 요소 파싱 테스트
TEST_F(SCXMLParserExecutableTest, ConditionalElementsParsing)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(1)); // 최소 1개 상태 필요

    // 액션 노드 생성 기대
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(4)); // if, elseif, else 및 내부 액션들

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
  <datamodel>
    <data id="x" expr="10"/>
  </datamodel>

  <state id="s1">
    <onentry>
      <!-- 조건부 요소 사용 -->
      <if cond="x &lt; 5">
        <log expr="'x is less than 5'"/>
        <elseif cond="x &lt; 15"/>
        <log expr="'x is between 5 and 15'"/>
        <else/>
        <log expr="'x is 15 or greater'"/>
      </if>
    </onentry>
  </state>
</scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // onentry에 if 요소가 파싱되었는지 확인
    EXPECT_FALSE(s1->getOnEntry().empty());
}

// <script> 요소 테스트
TEST_F(SCXMLParserExecutableTest, ScriptElementTest)
{
    // script 요소가 있는 SCXML 파싱 테스트
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(1));

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <script>
        // 전역 스크립트 - 문서 로드 시 실행
        var globalCounter = 0;
        function incrementCounter() {
          globalCounter += 1;
          return globalCounter;
        }
      </script>

      <datamodel>
        <data id="testVar" expr="0"/>
      </datamodel>

      <state id="s1">
        <onentry>
          <script>
            // 상태 진입 시 실행되는 스크립트
            testVar = incrementCounter();
          </script>
        </onentry>
        <transition event="next" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태가 존재하는지 확인
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // 스크립트가 포함된 onentry 핸들러 확인
    EXPECT_FALSE(s1->getOnEntry().empty());
}
// <send> 타임아웃 및 지연 기능 테스트
TEST_F(SCXMLParserExecutableTest, SendDelayTest)
{
    // 액션 노드 생성 기대
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(3)); // 최소 3개 액션 노드

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <onentry>
          <!-- 즉시 전송 -->
          <send event="immediate" target="#_internal" id="send1"/>

          <!-- 지연 전송 (고정 시간) -->
          <send event="delayed" target="#_internal" delay="5s" id="send2"/>

          <!-- 지연 전송 (동적 시간) -->
          <send event="dynamicDelayed" target="#_internal" delayexpr="dynamicValue + 's'" id="send3"/>
        </onentry>

        <transition event="cancel" target="s2">
          <cancel sendid="send2"/>
          <cancel sendidexpr="'send3'"/>
        </transition>

        <transition event="immediate" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // onentry에 send 요소가 파싱되었는지 확인
    EXPECT_FALSE(s1->getOnEntry().empty());

    // 전환이 올바르게 파싱되었는지 확인
    const auto &transitions = s1->getTransitions();
    bool foundCancelTransition = false;
    bool foundImmediateTransition = false;

    for (const auto &t : transitions)
    {
        if (t->getEvent() == "cancel")
        {
            foundCancelTransition = true;
            // cancel 액션이 있는지 확인
            EXPECT_FALSE(t->getActions().empty());
        }
        else if (t->getEvent() == "immediate")
        {
            foundImmediateTransition = true;
        }
    }

    EXPECT_TRUE(foundCancelTransition);
    EXPECT_TRUE(foundImmediateTransition);
}

// <send> 지연 및 취소 기능 테스트
TEST_F(SCXMLParserExecutableTest, SendDelayAndCancelTest)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(3)); // 최소 3개 상태 필요

    // 액션 노드 생성 기대
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(2)); // send와 cancel 액션

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <onentry>
          <send id="delayed" event="timeout" delay="5s"/>
        </onentry>
        <transition event="cancel" target="s2">
          <cancel sendid="delayed"/>
        </transition>
        <transition event="timeout" target="s3"/>
      </state>
      <state id="s2"/>
      <state id="s3"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // 전환 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_EQ(2, transitions.size());

    // cancel 전환 확인
    bool foundCancelTransition = false;
    for (const auto &t : transitions)
    {
        if (t->getEvent() == "cancel")
        {
            foundCancelTransition = true;
            EXPECT_EQ("s2", t->getTargets()[0]);
            break;
        }
    }

    EXPECT_TRUE(foundCancelTransition) << "cancel 전환이 없습니다";
}

TEST_F(SCXMLParserExecutableTest, ActionNodeParsing)
{
    // ActionNode Mock 생성 호출 예상
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(3)); // 최소 3개 액션 파싱 예상

    // code 네임스페이스를 정의하고 사용하는 SCXML
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml"
       xmlns:code="http://www.example.org/code-extensions"
       version="1.0" initial="s1">
  <state id="s1">
    <onentry>
      <code:action name="logEntry" externalClass="Logger" type="log" param1="value1" param2="value2"/>
    </onentry>
    <onexit>
      <code:action name="logExit" externalFactory="ActionFactory" type="notification"/>
    </onexit>
    <transition event="next" target="s2">
      <code:action name="customAction" type="special" customParam="customValue"/>
    </transition>
  </state>
  <state id="s2"/>
</scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // 상태 확인
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    EXPECT_EQ("logEntry", s1->getOnEntry());
    EXPECT_EQ("logExit", s1->getOnExit());

    // 전환 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_FALSE(transitions.empty());
    ASSERT_EQ(1, transitions.size());

    auto transition = transitions[0];
    EXPECT_EQ("next", transition->getEvent());
    ASSERT_FALSE(transition->getTargets().empty());
    EXPECT_EQ("s2", transition->getTargets()[0]);

    // 전환 액션 ID 확인
    const auto &actions = transition->getActions();
    ASSERT_FALSE(actions.empty());
    EXPECT_EQ(1, actions.size());
    EXPECT_EQ("customAction", actions[0]);
}

// 액션 ID 처리 테스트
TEST_F(SCXMLParserExecutableTest, ActionNodeIds)
{
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml"
       xmlns:code="http://example.org/code"
       version="1.0" initial="s1">
  <state id="s1">
    <onentry>
      <code:action name="entry1"/>
      <code:action name="entry2"/>
    </onentry>
    <onexit>
      <code:action name="exit1"/>
    </onexit>
    <transition event="next" target="s2">
      <code:action name="transition1"/>
      <code:action name="transition2"/>
    </transition>
  </state>
  <state id="s2"/>
</scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // 상태 확인
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // 진입/종료 액션 ID 확인
    EXPECT_FALSE(s1->getOnEntry().empty());
    EXPECT_FALSE(s1->getOnExit().empty());

    // 전환 액션 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_FALSE(transitions.empty());
    ASSERT_EQ(1, transitions.size());

    auto transition = transitions[0];
    const auto &actions = transition->getActions();
    ASSERT_FALSE(actions.empty());
    ASSERT_EQ(2, actions.size());

    // 액션 ID 확인
    EXPECT_EQ("transition1", actions[0]);
    EXPECT_EQ("transition2", actions[1]);
}

// 사용자 정의 액션 테스트
TEST_F(SCXMLParserExecutableTest, CustomActions)
{
    // 액션 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(2));

    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(2));

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml"
       xmlns:code="http://example.org/code"
       version="1.0" initial="s1">
  <state id="s1">
    <onentry>
      <code:action name="logEntry" param1="value1" param2="value2"/>
    </onentry>
    <onexit>
      <code:action name="logExit"/>
    </onexit>
    <transition event="next" target="s2">
      <code:action name="logTransition"/>
    </transition>
  </state>
  <state id="s2"/>
</scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // 진입/종료 액션 확인
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // 상태의 onEntry와 onExit 확인
    EXPECT_FALSE(s1->getOnEntry().empty());
    EXPECT_FALSE(s1->getOnExit().empty());

    // 액션이 등록되었는지는 추가 메소드가 필요할 수 있음
    // 그런 메소드가 있다면 다음과 같이 테스트
    const auto &entryActions = s1->getEntryActions();
    const auto &exitActions = s1->getExitActions();

    EXPECT_EQ(1, entryActions.size());
    EXPECT_EQ("logEntry", entryActions[0]);

    EXPECT_EQ(1, exitActions.size());
    EXPECT_EQ("logExit", exitActions[0]);
}

// 복잡한 실행 가능 콘텐츠 파싱 테스트
TEST_F(SCXMLParserExecutableTest, ExecutableContentParsingTest)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(2)); // 최소 2개 상태 필요

    // 액션 노드 생성 기대 - 다양한 실행 가능 콘텐츠
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(10)); // 최소 10개 액션 노드

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="main" datamodel="ecmascript">
  <datamodel>
    <data id="count" expr="0"/>
    <data id="items" expr="[1, 2, 3, 4, 5]"/>
    <data id="condition" expr="true"/>
  </datamodel>

  <state id="main">
    <onentry>
      <!-- raise 요소 -->
      <raise event="internal.event"/>

      <!-- log 요소 -->
      <log expr="'Entering main state with count: ' + count" label="INFO"/>

      <!-- assign 요소 -->
      <assign location="count" expr="count + 1"/>

      <!-- if-elseif-else 구조 -->
      <if cond="count &lt; 5">
        <log expr="'Count is less than 5'"/>

        <elseif cond="count &lt; 10"/>
        <log expr="'Count is between 5 and 10'"/>

        <else/>
        <log expr="'Count is 10 or greater'"/>
      </if>

      <!-- 중첩된 if 구조 -->
      <if cond="condition">
        <log expr="'Outer condition is true'"/>
        <if cond="count &gt; 2">
          <log expr="'Inner condition is also true'"/>
        </if>
      </if>

      <!-- foreach 요소 -->
      <foreach item="item" index="idx" array="items">
        <log expr="'Item ' + idx + ' is: ' + item"/>
      </foreach>
    </onentry>

    <transition event="next" target="other">
      <!-- 전환 내 실행 가능 콘텐츠 -->
      <log expr="'Moving to next state'"/>
      <assign location="count" expr="count + 1"/>
    </transition>
  </state>

  <state id="other"/>
</scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // 실행 가능 콘텐츠 파싱 검증
    IStateNode *mainState = model->findStateById("main");
    ASSERT_TRUE(mainState != nullptr);

    // onentry 핸들러 확인
    EXPECT_FALSE(mainState->getOnEntry().empty()) << "main 상태의 onentry 핸들러가 비어있습니다";

    // 전환 확인
    const auto &transitions = mainState->getTransitions();
    ASSERT_EQ(1, transitions.size()) << "main 상태는 1개의 전환을 가져야 합니다";
    EXPECT_EQ("next", transitions[0]->getEvent());

    // 전환 내 액션 확인
    const auto &actions = transitions[0]->getActions();
    ASSERT_FALSE(actions.empty()) << "전환의 액션이 비어있습니다";
}
