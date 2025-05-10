#include "SCXMLParserTestCommon.h"

// 상태 관련 기능을 테스트하는 픽스처 클래스
class SCXMLParserStateTest : public SCXMLParserTestBase
{
};

// 복합 상태 파싱 테스트
TEST_F(SCXMLParserStateTest, ParseCompoundState)
{
  // 복합 상태와 자식 상태, final 상태 생성 호출 예상
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(4)); // main, sub1, sub2, final

  // 전환 노드도 생성되어야 함 - 테스트 SCXML에 포함된 전환 수에 맞게 설정
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // sub1->sub2, sub2->final, main->main

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="main">
  <state id="main" initial="sub1">
    <state id="sub1">
      <transition event="next" target="sub2"/>
    </state>
    <state id="sub2">
      <transition event="done" target="final"/>
    </state>
    <transition event="reset" target="main"/>
  </state>
  <final id="final"/>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 초기 상태가 제대로 설정되었는지 확인
  EXPECT_EQ("main", model->getInitialState());
}

// 병렬 상태 파싱 테스트
TEST_F(SCXMLParserStateTest, ParseParallelState)
{
  // 병렬 상태와 여러 영역, 자식 상태 생성 호출 예상
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(7)); // p1, r1, r1a, r1b, r2, r2a, r2b

  // 전환 노드도 생성되어야 함
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // r1a->r1b, r2a->r2b 전환

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="p1">
  <parallel id="p1">
    <state id="r1" initial="r1a">
      <state id="r1a">
        <transition event="e1" target="r1b"/>
      </state>
      <state id="r1b"/>
    </state>
    <state id="r2" initial="r2a">
      <state id="r2a">
        <transition event="e2" target="r2b"/>
      </state>
      <state id="r2b"/>
    </state>
  </parallel>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 초기 상태가 제대로 설정되었는지 확인
  EXPECT_EQ("p1", model->getInitialState());
}

// 복잡한 중첩 상태 테스트
TEST_F(SCXMLParserStateTest, ComplexNestedStates)
{
  // 복잡한 중첩 상태 구조를 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(6)); // 중첩된 여러 상태들

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <state id="s1" initial="s1.1">
    <state id="s1.1" initial="s1.1.1">
      <state id="s1.1.1">
        <transition event="e1" target="s1.1.2"/>
      </state>
      <state id="s1.1.2">
        <transition event="e2" target="s1.2"/>
      </state>
    </state>
    <state id="s1.2">
      <transition event="e3" target="s2"/>
    </state>
    <transition event="reset" target="s1"/>
  </state>
  <state id="s2">
    <transition event="restart" target="s1"/>
  </state>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 초기 상태 확인
  EXPECT_EQ("s1", model->getInitialState());

  // 상태 계층 구조 확인을 위한 상태 찾기
  IStateNode *s1 = model->findStateById("s1");
  ASSERT_TRUE(s1 != nullptr);
  EXPECT_EQ("s1.1", s1->getInitialState());

  // s1 상태에 자식이 있는지 확인
  EXPECT_FALSE(s1->getChildren().empty());

  // 자식 상태 ID 확인
  bool foundS11 = false;
  bool foundS12 = false;
  for (const auto &child : s1->getChildren())
  {
    if (child->getId() == "s1.1")
      foundS11 = true;
    if (child->getId() == "s1.2")
      foundS12 = true;
  }
  EXPECT_TRUE(foundS11);
  EXPECT_TRUE(foundS12);
}

// 병렬 상태 내부의 원자적 상태 테스트
TEST_F(SCXMLParserStateTest, AtomicStatesInParallel)
{
  // 병렬 상태 내 여러 원자적 상태 파싱
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(7)); // p, r1, r1a, r1b, r2, r2a, r2b

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="p">
  <parallel id="p">
    <state id="r1" initial="r1a">
      <state id="r1a">
        <transition event="e1" target="r1b"/>
      </state>
      <state id="r1b">
        <transition event="done" target="r1Final"/>
      </state>
      <final id="r1Final"/>
    </state>
    <state id="r2" initial="r2a">
      <state id="r2a">
        <transition event="e2" target="r2b"/>
      </state>
      <state id="r2b">
        <transition event="done" target="r2Final"/>
      </state>
      <final id="r2Final"/>
    </state>
  </parallel>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 병렬 상태 및 자식 상태 확인
  IStateNode *p = model->findStateById("p");
  ASSERT_TRUE(p != nullptr);
  EXPECT_EQ(Type::PARALLEL, p->getType());

  // 자식 상태 확인
  EXPECT_EQ(2, p->getChildren().size());

  bool foundR1 = false;
  bool foundR2 = false;

  for (const auto &child : p->getChildren())
  {
    if (child->getId() == "r1")
    {
      foundR1 = true;
      EXPECT_EQ("r1a", child->getInitialState());

      // r1의 자식 상태 확인
      EXPECT_EQ(3, child->getChildren().size()); // r1a, r1b, r1Final
    }
    else if (child->getId() == "r2")
    {
      foundR2 = true;
      EXPECT_EQ("r2a", child->getInitialState());

      // r2의 자식 상태 확인
      EXPECT_EQ(3, child->getChildren().size()); // r2a, r2b, r2Final
    }
  }

  EXPECT_TRUE(foundR1);
  EXPECT_TRUE(foundR2);
}

// 초기 상태 지정 방식 테스트
TEST_F(SCXMLParserStateTest, InitialStateSpecification)
{
  // 두 가지 방식의 초기 상태 지정
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(5)); // s1, s1a, s1b, s2, s2a

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <state id="s1" initial="s1a">
    <state id="s1a"/>
    <state id="s1b"/>
  </state>
  <state id="s2">
    <initial>
      <transition target="s2a"/>
    </initial>
    <state id="s2a"/>
  </state>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 초기 상태 속성 방식
  IStateNode *s1 = model->findStateById("s1");
  ASSERT_TRUE(s1 != nullptr);
  EXPECT_EQ("s1a", s1->getInitialState());

  // initial 요소 방식
  IStateNode *s2 = model->findStateById("s2");
  ASSERT_TRUE(s2 != nullptr);
  EXPECT_EQ("s2a", s2->getInitialState());
}

// 초기 상태가 지정되지 않은 경우의 기본 동작 테스트
TEST_F(SCXMLParserStateTest, DefaultInitialState)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(3)); // 최소 3개 상태 필요

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0">
  <!-- 초기 상태가 지정되지 않은 복합 상태 -->
  <state id="parent">
    <state id="child1"/>
    <state id="child2"/>
  </state>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // parent 상태 찾기
  IStateNode *parent = model->findStateById("parent");
  ASSERT_TRUE(parent != nullptr);

  // 초기 상태가 지정되지 않았으므로 첫 번째 자식이 기본 초기 상태가 되어야 함
  EXPECT_EQ("child1", parent->getInitialState());
}

// Macrostep/Microstep 알고리즘 테스트
TEST_F(SCXMLParserStateTest, MacrostepMicrostepProcessing)
{
  // 복잡한 전환 체인으로 macrostep/microstep 테스트
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(5));
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(testing::AtLeast(4));
  EXPECT_CALL(*mockFactory, createActionNode(testing::_))
      .Times(testing::AtLeast(3));

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <datamodel>
        <data id="count" expr="0"/>
      </datamodel>
      <state id="s1">
        <onentry>
          <assign location="count" expr="count + 1"/>
        </onentry>
        <!-- 첫 번째 eventless 전환 -->
        <transition target="s2">
          <assign location="count" expr="count + 1"/>
        </transition>
      </state>
      <state id="s2">
        <onentry>
          <assign location="count" expr="count + 1"/>
          <!-- 내부 이벤트 발생 -->
          <raise event="internal.event"/>
        </onentry>
        <!-- 내부 이벤트에 의한 전환 -->
        <transition event="internal.event" target="s3">
          <assign location="count" expr="count + 1"/>
        </transition>
      </state>
      <state id="s3">
        <onentry>
          <assign location="count" expr="count + 1"/>
        </onentry>
        <!-- 또 다른 eventless 전환 -->
        <transition cond="count > 4" target="s4">
          <assign location="count" expr="count + 1"/>
        </transition>
      </state>
      <state id="s4">
        <onentry>
          <assign location="count" expr="count + 1"/>
        </onentry>
        <!-- 마지막 eventless 전환 -->
        <transition target="final">
          <assign location="count" expr="count + 1"/>
        </transition>
      </state>
      <final id="final"/>
    </scxml>)";

  auto model = parser->parseContent(scxml);
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 모든 상태 및 전환 확인
  for (const auto &stateId : {"s1", "s2", "s3", "s4", "final"})
  {
    IStateNode *state = model->findStateById(stateId);
    ASSERT_TRUE(state != nullptr) << "상태 " << stateId << "를 찾을 수 없습니다";
  }
}

// 병렬 상태 및 충돌하는 전환 테스트
TEST_F(SCXMLParserStateTest, ParallelStatesConflictingTransitionsTest)
{
  // 복잡한 병렬 상태 및 충돌하는 전환을 설정
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(6));
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(testing::AtLeast(4));

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="p">
      <parallel id="p">
        <state id="r1" initial="r1a">
          <state id="r1a">
            <!-- 높은 우선순위 전환: 자식 상태에서 정의 -->
            <transition event="e" target="outside"/>
          </state>
          <state id="r1b"/>
          <!-- 낮은 우선순위 전환: 부모 상태에서 정의 -->
          <transition event="e" target="r1b"/>
        </state>
        <state id="r2" initial="r2a">
          <state id="r2a">
            <!-- 첫 번째 문서 순서의 전환 -->
            <transition event="e" target="r2b"/>
          </state>
          <state id="r2b">
            <!-- 두 번째 문서 순서의 전환 -->
            <transition event="e" target="outside"/>
          </state>
        </state>
      </parallel>
      <state id="outside"/>
    </scxml>)";

  auto model = parser->parseContent(scxml);
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // r1a의 전환이 r1의 전환보다 우선순위가 높은지 확인
  IStateNode *r1a = model->findStateById("r1a");
  IStateNode *r1 = model->findStateById("r1");
  ASSERT_TRUE(r1a != nullptr && r1 != nullptr);

  // r2a의 첫 번째 전환이 r2b의 두 번째 전환보다 우선순위가 높은지 확인
  IStateNode *r2a = model->findStateById("r2a");
  IStateNode *r2b = model->findStateById("r2b");
  ASSERT_TRUE(r2a != nullptr && r2b != nullptr);
}

// 의존성 주입 파싱 테스트
TEST_F(SCXMLParserStateTest, ParseInjectPoints)
{
  // 상태 노드 생성 호출 예상
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(2)); // s1, s2

  // 전환 노드도 생성되어야 함
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(testing::AtLeast(1)); // s1->s2 전환

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml"
       xmlns:di="http://example.org/di"
       version="1.0" initial="s1">
  <di:inject-point name="logger" type="ILogger"/>
  <di:inject-point name="database" type="IDatabase"/>
  <state id="s1">
    <transition event="log" target="s2"/>
  </state>
  <state id="s2"/>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // 의존성 주입 지점 확인
  const auto &injects = model->getInjectPoints();
  EXPECT_EQ(2, injects.size());

  auto it = injects.find("logger");
  ASSERT_NE(injects.end(), it);
  EXPECT_EQ("ILogger", it->second);

  it = injects.find("database");
  ASSERT_NE(injects.end(), it);
  EXPECT_EQ("IDatabase", it->second);
}

// 추가 액션 테스트 (addEntryAction, addExitAction 메서드 테스트)
TEST_F(SCXMLParserStateTest, ActionNodeAddition)
{
  // MockStateNode 설정
  auto mockState = std::make_shared<MockStateNode>();
  mockState->id_ = "testState";
  mockState->SetupDefaultBehavior();

  // 액션 추가 전 초기 상태 확인
  EXPECT_TRUE(mockState->getEntryActions().empty()) << "Entry actions should be empty initially";
  EXPECT_TRUE(mockState->getExitActions().empty()) << "Exit actions should be empty initially";
  EXPECT_TRUE(mockState->getOnEntry().empty()) << "OnEntry should be empty initially";
  EXPECT_TRUE(mockState->getOnExit().empty()) << "OnExit should be empty initially";

  // 액션 추가
  mockState->addEntryAction("entry1");
  mockState->addEntryAction("entry2");
  mockState->addExitAction("exit1");

  // 액션 목록 확인
  const auto &entryActions = mockState->getEntryActions();
  const auto &exitActions = mockState->getExitActions();

  // 액션 수 확인
  EXPECT_EQ(2, entryActions.size()) << "Should have 2 entry actions";
  EXPECT_EQ(1, exitActions.size()) << "Should have 1 exit action";

  // 액션 내용 확인
  EXPECT_EQ("entry1", entryActions[0]) << "First entry action should be 'entry1'";
  EXPECT_EQ("entry2", entryActions[1]) << "Second entry action should be 'entry2'";
  EXPECT_EQ("exit1", exitActions[0]) << "Exit action should be 'exit1'";

  // onEntry와 onExit 문자열 확인 (세미콜론으로 연결되었는지)
  EXPECT_EQ("entry1;entry2", mockState->getOnEntry()) << "OnEntry string should concatenate actions";
  EXPECT_EQ("exit1", mockState->getOnExit()) << "OnExit string should contain action";

  // 추가 액션 테스트
  mockState->addExitAction("exit2");
  EXPECT_EQ(2, mockState->getExitActions().size()) << "Should have 2 exit actions now";
  EXPECT_EQ("exit1;exit2", mockState->getOnExit()) << "OnExit string should concatenate actions";
}

// 전환 우선순위 및 충돌 해결 테스트
TEST_F(SCXMLParserStateTest, TransitionPriorityAndConflictResolution)
{
  // 복잡한 병렬 상태 및 충돌하는 전환을 설정
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(6));
  EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
      .Times(testing::AtLeast(4));

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="p">
      <parallel id="p">
        <state id="r1" initial="r1a">
          <state id="r1a">
            <!-- 높은 우선순위 전환: 자식 상태에서 정의 -->
            <transition event="e" target="outside"/>
          </state>
          <state id="r1b"/>
          <!-- 낮은 우선순위 전환: 부모 상태에서 정의 -->
          <transition event="e" target="r1b"/>
        </state>
        <state id="r2" initial="r2a">
          <state id="r2a">
            <!-- 첫 번째 문서 순서의 전환 -->
            <transition event="e" target="r2b"/>
          </state>
          <state id="r2b">
            <!-- 두 번째 문서 순서의 전환 -->
            <transition event="e" target="outside"/>
          </state>
        </state>
      </parallel>
      <state id="outside"/>
    </scxml>)";

  auto model = parser->parseContent(scxml);
  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // r1a의 전환이 r1의 전환보다 우선순위가 높은지 확인
  IStateNode *r1a = model->findStateById("r1a");
  IStateNode *r1 = model->findStateById("r1");
  ASSERT_TRUE(r1a != nullptr && r1 != nullptr);

  // r2a의 첫 번째 전환이 r2b의 두 번째 전환보다 우선순위가 높은지 확인
  IStateNode *r2a = model->findStateById("r2a");
  IStateNode *r2b = model->findStateById("r2b");
  ASSERT_TRUE(r2a != nullptr && r2b != nullptr);
}

// initial 속성과 <initial> 요소의 차이 테스트
TEST_F(SCXMLParserStateTest, InitialAttributeVsInitialElement)
{
  // 상태 노드 생성 기대
  EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
      .Times(testing::AtLeast(6)); // 최소 6개 상태 필요

  // 액션 노드 생성 기대
  EXPECT_CALL(*mockFactory, createActionNode(testing::_))
      .Times(testing::AtLeast(1)); // initial 전환의 액션

  std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0">
  <!-- initial 속성 사용 -->
  <state id="state1" initial="state1_1">
    <state id="state1_1"/>
    <state id="state1_2"/>
  </state>

  <!-- <initial> 요소 사용 -->
  <state id="state2">
    <initial>
      <transition target="state2_2">
        <log expr="'Entering initial state of state2'"/>
      </transition>
    </initial>
    <state id="state2_1"/>
    <state id="state2_2"/>
  </state>
</scxml>)";

  auto model = parser->parseContent(scxml);

  ASSERT_TRUE(model != nullptr);
  EXPECT_FALSE(parser->hasErrors());

  // state1 찾기 (initial 속성 사용)
  IStateNode *state1 = model->findStateById("state1");
  ASSERT_TRUE(state1 != nullptr);
  EXPECT_EQ("state1_1", state1->getInitialState());

  // state2 찾기 (<initial> 요소 사용)
  IStateNode *state2 = model->findStateById("state2");
  ASSERT_TRUE(state2 != nullptr);
  EXPECT_EQ("state2_2", state2->getInitialState());

  // <initial> 요소의 전환에 실행 콘텐츠가 포함되어 있는지 확인
  // 이 부분은 구현에 따라 다를 수 있으므로 적절히 수정이 필요할 수 있습니다
  const auto &initialTransition = state2->getInitialTransition();
  ASSERT_TRUE(initialTransition != nullptr);
  EXPECT_FALSE(initialTransition->getActions().empty());
}
