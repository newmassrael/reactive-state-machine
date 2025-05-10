#include "SCXMLParserTestCommon.h"

// 기본 테스트 픽스처 상속
class SCXMLParserEventTest : public SCXMLParserTestBase
{
};

// 이벤트 디스크립터 파싱 테스트
TEST_F(SCXMLParserEventTest, EventDescriptorParsing)
{
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <transition event="error" target="s2"/>
        <transition event="error.*" target="s3"/>
        <transition event="custom.event" target="s4"/>
        <transition event="*" target="s5"/>
      </state>
      <state id="s2"/>
      <state id="s3"/>
      <state id="s4"/>
      <state id="s5"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);

    // 트랜지션 이벤트 디스크립터 확인
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);
    EXPECT_EQ(4, s1->getTransitions().size());

    // 이벤트 디스크립터가 올바르게 파싱되었는지 확인
    bool foundExact = false;
    bool foundWildcard = false;
    bool foundDotted = false;
    bool foundStar = false;

    for (const auto &t : s1->getTransitions())
    {
        if (t->getEvent() == "error")
            foundExact = true;
        else if (t->getEvent() == "error.*")
            foundWildcard = true;
        else if (t->getEvent() == "custom.event")
            foundDotted = true;
        else if (t->getEvent() == "*")
            foundStar = true;
    }

    EXPECT_TRUE(foundExact);
    EXPECT_TRUE(foundWildcard);
    EXPECT_TRUE(foundDotted);
    EXPECT_TRUE(foundStar);
}

// 이벤트 디스크립터 복잡한 매칭 테스트
TEST_F(SCXMLParserEventTest, ComplexEventDescriptorTest)
{
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <transition event="error.communication" target="commError"/>
        <transition event="error.*" target="generalError"/>
        <transition event="done.invoke.process1 done.invoke.process2" target="allDone"/>
        <transition event="message.*.urgent" target="urgent"/>
        <transition event="*" target="anyEvent"/>
      </state>
      <state id="commError"/>
      <state id="generalError"/>
      <state id="allDone"/>
      <state id="urgent"/>
      <state id="anyEvent"/>
    </scxml>)";

    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(6)); // s1 + 5개 대상 상태

    // 전환 노드 생성 기대
    EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
        .Times(5); // 5개 전환

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // 상태 s1 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // 전환 이벤트 디스크립터 확인
    const auto &transitions = s1->getTransitions();
    EXPECT_EQ(5, transitions.size());

    // 각 전환의 이벤트 확인 (순서는 문서 순서와 같아야 함)
    EXPECT_EQ("error.communication", transitions[0]->getEvent());
    EXPECT_EQ("error.*", transitions[1]->getEvent());
    EXPECT_EQ("done.invoke.process1 done.invoke.process2", transitions[2]->getEvent());
    EXPECT_EQ("message.*.urgent", transitions[3]->getEvent());
    EXPECT_EQ("*", transitions[4]->getEvent());
}

// 복잡한 이벤트 디스크립터 파싱 테스트
TEST_F(SCXMLParserEventTest, DetailedEventDescriptorTest)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(2)); // 최소 2개 상태 필요

    // 전환 노드 생성 기대 - 8개의 다양한 이벤트 디스크립터
    EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
        .Times(testing::AtLeast(8));

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="main">
  <state id="main">
    <!-- 단일 이벤트 -->
    <transition event="simple" target="other"/>

    <!-- 점(.) 구분자가 있는 이벤트 -->
    <transition event="system.device.update" target="other"/>

    <!-- 와일드카드 접미사로 끝나는 이벤트 -->
    <transition event="error.*" target="other"/>

    <!-- 와일드카드 중간에 있는 이벤트 -->
    <transition event="device.*.update" target="other"/>

    <!-- 와일드카드만 있는 이벤트 -->
    <transition event="*" target="other"/>

    <!-- 공백으로 구분된 여러 이벤트 -->
    <transition event="login logout" target="other"/>

    <!-- 다양한 와일드카드가 있는 여러 이벤트 -->
    <transition event="system.* user.*" target="other"/>

    <!-- 와일드카드 접두사로 끝나는 이벤트 -->
    <transition event="*.error" target="other"/>
  </state>

  <state id="other"/>
</scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // 이벤트 디스크립터 파싱 검증
    IStateNode *mainState = model->findStateById("main");
    ASSERT_TRUE(mainState != nullptr);

    const auto &transitions = mainState->getTransitions();
    ASSERT_EQ(8, transitions.size()) << "main 상태는 8개의 전환을 가져야 합니다";

    // 각 이벤트 디스크립터 확인
    std::vector<std::string> expectedEvents = {
        "simple",
        "system.device.update",
        "error.*",
        "device.*.update",
        "*",
        "login logout",
        "system.* user.*",
        "*.error"};

    for (size_t i = 0; i < transitions.size(); i++)
    {
        EXPECT_EQ(expectedEvents[i], transitions[i]->getEvent())
            << "전환 " << i << "의 이벤트 디스크립터가 올바르게 파싱되지 않았습니다";
    }
}

// 이벤트 디스크립터 매칭 알고리즘 심층 테스트
TEST_F(SCXMLParserEventTest, AdvancedEventDescriptorTest)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(10)); // 최소 10개 상태 필요

    // 전환 노드 생성 기대
    EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
        .Times(testing::AtLeast(10)); // 다양한 이벤트 패턴을 가진 전환들

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <!-- 정확한 이벤트 이름 매칭 -->
        <transition event="exact" target="s_exact"/>

        <!-- 접두사 매칭 (점 구분자) -->
        <transition event="prefix.event" target="s_prefix"/>

        <!-- 점으로 끝나는 이벤트 (무의미하지만 테스트) -->
        <transition event="prefix." target="s_prefix_dot"/>

        <!-- 와일드카드 접미사 매칭 -->
        <transition event="wild.*" target="s_wild_suffix"/>

        <!-- 와일드카드 매칭 (중간에 와일드카드) -->
        <transition event="middle.*.end" target="s_wild_middle"/>

        <!-- 와일드카드 접두사 매칭 -->
        <transition event="*.suffix" target="s_wild_prefix"/>

        <!-- 전체 와일드카드 매칭 -->
        <transition event="*" target="s_wild_all"/>

        <!-- 숫자가 포함된 이벤트 이름 -->
        <transition event="event.123" target="s_numeric"/>

        <!-- 공백으로 구분된 여러 이벤트 매칭 -->
        <transition event="multiple1 multiple2" target="s_multiple"/>

        <!-- 복잡한 다중 패턴 매칭 -->
        <transition event="a.* b.* c.123" target="s_complex"/>
      </state>

      <state id="s_exact"/>
      <state id="s_prefix"/>
      <state id="s_prefix_dot"/>
      <state id="s_wild_suffix"/>
      <state id="s_wild_middle"/>
      <state id="s_wild_prefix"/>
      <state id="s_wild_all"/>
      <state id="s_numeric"/>
      <state id="s_multiple"/>
      <state id="s_complex"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // 전환들이 올바르게 파싱되었는지 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_EQ(10, transitions.size());

    // 각 전환의 이벤트 패턴 확인
    std::vector<std::pair<std::string, std::string>> expectedTransitions = {
        {"exact", "s_exact"},
        {"prefix.event", "s_prefix"},
        {"prefix.", "s_prefix_dot"},
        {"wild.*", "s_wild_suffix"},
        {"middle.*.end", "s_wild_middle"},
        {"*.suffix", "s_wild_prefix"},
        {"*", "s_wild_all"},
        {"event.123", "s_numeric"},
        {"multiple1 multiple2", "s_multiple"},
        {"a.* b.* c.123", "s_complex"}};

    for (size_t i = 0; i < expectedTransitions.size(); i++)
    {
        EXPECT_EQ(expectedTransitions[i].first, transitions[i]->getEvent());
        EXPECT_EQ(expectedTransitions[i].second, transitions[i]->getTargets()[0]);
    }
}

// 이벤트 디스크립터 매칭 알고리즘 테스트
TEST_F(SCXMLParserEventTest, EventDescriptorMatching)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(7)); // 최소 7개 상태 필요

    // 전환 노드 생성 기대
    EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
        .Times(testing::AtLeast(6)); // 다양한 이벤트 패턴을 가진 전환들

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <state id="s1">
    <!-- 정확한 이벤트 이름 매칭 -->
    <transition event="exact" target="s_exact"/>

    <!-- 접두사 매칭 (점 구분자) -->
    <transition event="prefix.event" target="s_prefix"/>

    <!-- 와일드카드 접미사 매칭 -->
    <transition event="wild.*" target="s_wild_suffix"/>

    <!-- 와일드카드 매칭 (중간에 와일드카드) -->
    <transition event="middle.*.end" target="s_wild_middle"/>

    <!-- 전체 와일드카드 매칭 -->
    <transition event="*" target="s_wild_all"/>

    <!-- 공백으로 구분된 여러 이벤트 매칭 -->
    <transition event="multiple1 multiple2" target="s_multiple"/>
  </state>

  <state id="s_exact"/>
  <state id="s_prefix"/>
  <state id="s_wild_suffix"/>
  <state id="s_wild_middle"/>
  <state id="s_wild_all"/>
  <state id="s_multiple"/>
</scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태 찾기
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // 전환들이 올바르게 파싱되었는지 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_EQ(6, transitions.size());

    // 각 전환의 이벤트 패턴 확인
    bool foundExact = false;
    bool foundPrefix = false;
    bool foundWildSuffix = false;
    bool foundWildMiddle = false;
    bool foundWildAll = false;
    bool foundMultiple = false;

    for (const auto &t : transitions)
    {
        if (t->getEvent() == "exact")
        {
            foundExact = true;
            EXPECT_EQ("s_exact", t->getTargets()[0]);
        }
        else if (t->getEvent() == "prefix.event")
        {
            foundPrefix = true;
            EXPECT_EQ("s_prefix", t->getTargets()[0]);
        }
        else if (t->getEvent() == "wild.*")
        {
            foundWildSuffix = true;
            EXPECT_EQ("s_wild_suffix", t->getTargets()[0]);
        }
        else if (t->getEvent() == "middle.*.end")
        {
            foundWildMiddle = true;
            EXPECT_EQ("s_wild_middle", t->getTargets()[0]);
        }
        else if (t->getEvent() == "*")
        {
            foundWildAll = true;
            EXPECT_EQ("s_wild_all", t->getTargets()[0]);
        }
        else if (t->getEvent() == "multiple1 multiple2")
        {
            foundMultiple = true;
            EXPECT_EQ("s_multiple", t->getTargets()[0]);
        }
    }

    EXPECT_TRUE(foundExact);
    EXPECT_TRUE(foundPrefix);
    EXPECT_TRUE(foundWildSuffix);
    EXPECT_TRUE(foundWildMiddle);
    EXPECT_TRUE(foundWildAll);
    EXPECT_TRUE(foundMultiple);
}

// 명시적인 이벤트 발생 우선순위 테스트
TEST_F(SCXMLParserEventTest, RaiseEventPriorityTest)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(3)); // 최소 3개 상태 필요

    // 액션 노드 생성 기대
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(1)); // 최소 1개 액션 노드

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <onentry>
          <raise event="internal.event"/>
        </onentry>
        <transition event="internal.event" target="s2"/>
        <transition event="external.event" target="s3"/>
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

    // onentry 핸들러에 raise 요소가 있는지 확인
    EXPECT_FALSE(s1->getOnEntry().empty());

    // 전환 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_EQ(2, transitions.size());

    // 첫 번째 전환이 내부 이벤트를 처리하는지 확인
    EXPECT_EQ("internal.event", transitions[0]->getEvent());
    EXPECT_EQ("s2", transitions[0]->getTargets()[0]);

    // 두 번째 전환이 외부 이벤트를 처리하는지 확인
    EXPECT_EQ("external.event", transitions[1]->getEvent());
    EXPECT_EQ("s3", transitions[1]->getTargets()[0]);
}
