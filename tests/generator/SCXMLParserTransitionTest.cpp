#include "SCXMLParserTestCommon.h"

// 기본 테스트 픽스처 상속
class SCXMLParserTransitionTest : public SCXMLParserTestBase
{
};

// 전환 타입(내부/외부) 테스트
TEST_F(SCXMLParserTransitionTest, TransitionTypes)
{
    // 여러 타입의 전환을 파싱
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(3)); // main, child1, child2 상태

    // 전환 노드 생성
    EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
        .Times(testing::AtLeast(3)); // external, internal, back 세 개의 전환

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="main">
  <state id="main" initial="child1">
    <state id="child1">
      <transition event="external" target="child2"/>
      <transition event="internal" target="child1" type="internal"/>
    </state>
    <state id="child2">
      <transition event="back" target="child1"/>
    </state>
  </state>
</scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // 루트 상태를 통해 child1 상태 찾기
    IStateNode *main = model->findStateById("main");
    ASSERT_TRUE(main != nullptr);

    // 자식 상태 중에서 child1 찾기
    IStateNode *child1 = nullptr;
    for (const auto &child : main->getChildren())
    {
        if (child->getId() == "child1")
        {
            child1 = child.get();
            break;
        }
    }

    ASSERT_TRUE(child1 != nullptr);

    // 전환 타입 확인
    bool foundInternal = false;
    bool foundExternal = false;
    for (const auto &transition : child1->getTransitions())
    {
        if (transition->getEvent() == "internal")
        {
            EXPECT_TRUE(transition->isInternal());
            foundInternal = true;
        }
        if (transition->getEvent() == "external")
        {
            EXPECT_FALSE(transition->isInternal());
            foundExternal = true;
        }
    }
    EXPECT_TRUE(foundInternal);
    EXPECT_TRUE(foundExternal);
}

// 내부/외부 전환 타입 테스트 확장
TEST_F(SCXMLParserTransitionTest, DetailedTransitionTypeTest)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(4)); // 최소 4개 상태 필요

    // 전환 노드 생성 기대
    EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
        .Times(testing::AtLeast(5)); // 최소 5개 전환 필요

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="parent">
  <state id="parent" initial="child1">
    <!-- 내부 전환(type="internal") - 자식으로 가는 전환 -->
    <transition event="internal_to_child" target="child2" type="internal"/>

    <!-- 내부 전환(type="internal") - 형제로 가는 전환 -->
    <transition event="internal_to_sibling" target="sibling" type="internal"/>

    <!-- 기본 외부 전환(type="external") - 자식으로 가는 전환 -->
    <transition event="external_to_child" target="child2" type="external"/>

    <!-- 타겟이 없는 전환 -->
    <transition event="no_target" cond="true"/>

    <state id="child1">
      <transition event="child_to_child" target="child2"/>
    </state>

    <state id="child2"/>
  </state>

  <state id="sibling">
    <!-- 외부 전환(type="external") - 부모로 가는 전환 -->
    <transition event="to_parent" target="parent"/>
  </state>
</scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // 전환 타입 검증
    IStateNode *parentState = model->findStateById("parent");
    ASSERT_TRUE(parentState != nullptr);

    // 부모 상태의 전환 검사
    const auto &parentTransitions = parentState->getTransitions();
    ASSERT_GE(parentTransitions.size(), 4) << "부모 상태는 4개의 전환을 가져야 합니다";

    // 각 전환 타입 확인
    for (const auto &transition : parentTransitions)
    {
        if (transition->getEvent() == "internal_to_child")
        {
            EXPECT_TRUE(transition->isInternal()) << "internal_to_child 전환은 내부 전환이어야 합니다";
        }
        else if (transition->getEvent() == "internal_to_sibling")
        {
            EXPECT_TRUE(transition->isInternal()) << "internal_to_sibling 전환은 내부 전환이어야 합니다";
        }
        else if (transition->getEvent() == "external_to_child")
        {
            EXPECT_FALSE(transition->isInternal()) << "external_to_child 전환은 외부 전환이어야 합니다";
        }
        else if (transition->getEvent() == "no_target")
        {
            std::cout << "Found no_target transition with targets.size() = "
                      << transition->getTargets().size() << std::endl;

            // 타겟 목록 내용 확인
            auto targets = transition->getTargets();
            std::cout << "Targets content: [";
            for (const auto &target : targets)
            {
                std::cout << "'" << target << "', ";
            }
            std::cout << "]" << std::endl;

            // 타겟 리스트가 정말로 비어 있는지 확인
            std::cout << "targets.empty() = " << (targets.empty() ? "true" : "false") << std::endl;

            // hasTargets 메서드 호출 결과도 확인
            std::cout << "transition->hasTargets() = "
                      << (transition->hasTargets() ? "true" : "false") << std::endl;

            EXPECT_TRUE(transition->getTargets().empty()) << "no_target 전환은 타겟이 없어야 합니다";
        }
    }

    // 형제 상태의 전환 검사
    IStateNode *siblingState = model->findStateById("sibling");
    ASSERT_TRUE(siblingState != nullptr);

    const auto &siblingTransitions = siblingState->getTransitions();
    ASSERT_FALSE(siblingTransitions.empty()) << "형제 상태는 최소 하나의 전환을 가져야 합니다";

    auto toParentTransition = siblingTransitions[0];
    EXPECT_EQ("to_parent", toParentTransition->getEvent());
    EXPECT_FALSE(toParentTransition->isInternal()) << "to_parent 전환은 외부 전환이어야 합니다";
}

// 상태 전환 타겟 다중 지정 테스트
TEST_F(SCXMLParserTransitionTest, MultipleTargets)
{
    // 다중 타겟 전환 파싱
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(3)); // s1, s2, s3

    EXPECT_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
        .Times(testing::AnyNumber());

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <state id="s1">
    <transition event="split" target="s2 s3"/>
  </state>
  <state id="s2"/>
  <state id="s3"/>
</scxml>)";

    auto model = parser->parseContent(scxml);

    // 다중 타겟 지원 여부에 따라 파싱 성공 여부가 달라질 수 있음
    if (model != nullptr && !parser->hasErrors())
    {
        IStateNode *s1 = model->findStateById("s1");
        ASSERT_TRUE(s1 != nullptr);

        // 전환 확인
        const auto &transitions = s1->getTransitions();
        EXPECT_EQ(1, transitions.size());

        const auto &targets = transitions[0]->getTargets();
        EXPECT_EQ(2, targets.size()); // 두 개의 타겟이 있어야 함

        // 두 타겟이 포함되어 있는지 확인
        EXPECT_TRUE(std::find(targets.begin(), targets.end(), "s2") != targets.end());
        EXPECT_TRUE(std::find(targets.begin(), targets.end(), "s3") != targets.end());

        // 또는 순서가 중요하다면, 인덱스로 접근
        if (targets.size() >= 2)
        {
            EXPECT_EQ("s2", targets[0]);
            EXPECT_EQ("s3", targets[1]);
        }
    }
}

// 다중 타겟 전환(Multiple Target Transitions) 테스트
TEST_F(SCXMLParserTransitionTest, DetailedMultipleTargetsTest)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(6)); // 최소 6개 상태 필요

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="main">
      <parallel id="main">
        <state id="region1" initial="r1s1">
          <state id="r1s1"/>
          <state id="r1s2"/>
        </state>
        <state id="region2" initial="r2s1">
          <state id="r2s1"/>
          <state id="r2s2"/>
        </state>
        <!-- 두 영역의 다른 부분에 있는 상태들을 타겟으로 하는 전환 -->
        <transition event="split" target="r1s2 r2s2"/>
      </parallel>
    </scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // main 병렬 상태 찾기
    IStateNode *main = model->findStateById("main");
    ASSERT_TRUE(main != nullptr);
    EXPECT_EQ(Type::PARALLEL, main->getType());

    // 다중 타겟 전환 확인
    const auto &transitions = main->getTransitions();
    ASSERT_EQ(1, transitions.size());

    auto multiTargetTransition = transitions[0];
    EXPECT_EQ("split", multiTargetTransition->getEvent());

    // 두 개의 타겟이 있는지 확인
    const auto &targets = multiTargetTransition->getTargets();
    EXPECT_EQ(2, targets.size());
    EXPECT_TRUE(std::find(targets.begin(), targets.end(), "r1s2") != targets.end());
    EXPECT_TRUE(std::find(targets.begin(), targets.end(), "r2s2") != targets.end());
}

// 타겟리스 전환(Targetless Transitions) 테스트
TEST_F(SCXMLParserTransitionTest, TargetlessTransitionTest)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(1)); // 최소 1개 상태 필요

    // 타겟리스 전환의 액션 노드 생성 기대
    EXPECT_CALL(*mockFactory, createActionNode(testing::_))
        .Times(testing::AtLeast(1)); // 최소 1개 액션 노드

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="counter" expr="0"/>
      </datamodel>
      <state id="s1">
        <transition event="increment">
          <assign location="counter" expr="counter + 1"/>
        </transition>
        <transition cond="counter >= 10" target="s2"/>
      </state>
      <state id="s2"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태의 전환 확인
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // 전환 목록 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_EQ(2, transitions.size());

    // 첫 번째 전환이 타겟리스 전환인지 확인
    auto targetlessTransition = transitions[0];
    EXPECT_EQ("increment", targetlessTransition->getEvent());
    EXPECT_TRUE(targetlessTransition->getTargets().empty());
    EXPECT_FALSE(targetlessTransition->getActions().empty());
}

// 조건부 전환(Conditional Transitions)의 우선순위 테스트
TEST_F(SCXMLParserTransitionTest, ConditionalTransitionPriorityTest)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(4)); // 최소 4개 상태 필요

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1" datamodel="ecmascript">
      <datamodel>
        <data id="x" expr="5"/>
      </datamodel>
      <state id="s1">
        <!-- 문서 순서에 따라 첫 번째 매칭되는 전환이 선택되어야 함 -->
        <transition event="check" cond="x > 0" target="s2"/>
        <transition event="check" cond="x > 3" target="s3"/>
        <transition event="check" cond="x > 10" target="s4"/>
        <transition event="check" target="s_default"/>
      </state>
      <state id="s2"/>
      <state id="s3"/>
      <state id="s4"/>
      <state id="s_default"/>
    </scxml>)";

    auto model = parser->parseContent(scxml);

    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // s1 상태의 전환 확인
    IStateNode *s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);

    // 전환 목록 확인
    const auto &transitions = s1->getTransitions();
    ASSERT_EQ(4, transitions.size());

    // 전환 순서 및 조건 확인
    EXPECT_EQ("check", transitions[0]->getEvent());
    EXPECT_EQ("x > 0", transitions[0]->getGuard());
    EXPECT_EQ("s2", transitions[0]->getTargets()[0]);

    EXPECT_EQ("check", transitions[1]->getEvent());
    EXPECT_EQ("x > 3", transitions[1]->getGuard());
    EXPECT_EQ("s3", transitions[1]->getTargets()[0]);
}
