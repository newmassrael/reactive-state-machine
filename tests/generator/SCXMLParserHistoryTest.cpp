#include "SCXMLParserTestCommon.h"

class SCXMLParserHistoryTest : public SCXMLParserTestBase
{
};

// 상세 히스토리 상태 테스트
TEST_F(SCXMLParserHistoryTest, DetailedHistoryStateTest)
{
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="off">
      <state id="off">
        <transition event="power" target="on"/>
      </state>
      <state id="on" initial="player">
        <history id="h1" type="shallow">
          <transition target="player"/>
        </history>
        <history id="h2" type="deep">
          <transition target="player.stopped"/>
        </history>
        <state id="player" initial="stopped">
          <state id="stopped">
            <transition event="play" target="playing"/>
          </state>
          <state id="playing">
            <transition event="stop" target="stopped"/>
            <transition event="pause" target="paused"/>
          </state>
          <state id="paused">
            <transition event="play" target="playing"/>
            <transition event="stop" target="stopped"/>
          </state>
        </state>
        <state id="settings">
          <transition event="back" target="h1"/>
          <transition event="deep_restore" target="h2"/>
        </state>
        <transition event="menu" target="settings"/>
        <transition event="power" target="off"/>
      </state>
    </scxml>)";

    // 컨텐츠 파싱
    auto model = parser->parseContent(scxml);

    // 모델이 성공적으로 생성되었는지 확인
    ASSERT_TRUE(model != nullptr);

    // 'on' 상태 찾기
    IStateNode *onState = model->findStateById("on");
    ASSERT_TRUE(onState != nullptr);

    // 'on' 상태의 자식들 확인
    const auto &children = onState->getChildren();
    EXPECT_GE(children.size(), 4); // player, settings, h1, h2

    // 히스토리 상태 찾기
    bool foundH1 = false;
    bool foundH2 = false;
    IStateNode *h1State = nullptr;
    IStateNode *h2State = nullptr;

    for (const auto &child : children)
    {
        if (child->getId() == "h1")
        {
            foundH1 = true;
            h1State = child.get();
            EXPECT_EQ(Type::HISTORY, child->getType()) << "h1 should be a history state";
            EXPECT_TRUE(child->isShallowHistory()) << "h1 should be a shallow history";
            EXPECT_FALSE(child->isDeepHistory()) << "h1 should not be a deep history";
        }
        else if (child->getId() == "h2")
        {
            foundH2 = true;
            h2State = child.get();
            EXPECT_EQ(Type::HISTORY, child->getType()) << "h2 should be a history state";
            EXPECT_FALSE(child->isShallowHistory()) << "h2 should not be a shallow history";
            EXPECT_TRUE(child->isDeepHistory()) << "h2 should be a deep history";
        }
    }

    EXPECT_TRUE(foundH1) << "History state h1 not found";
    EXPECT_TRUE(foundH2) << "History state h2 not found";

    // 히스토리 상태의 기본 전환 확인
    if (h1State)
    {
        ASSERT_FALSE(h1State->getTransitions().empty()) << "h1 should have default transition";
        ASSERT_FALSE(h1State->getTransitions()[0]->getTargets().empty());
        EXPECT_EQ("player", h1State->getTransitions()[0]->getTargets()[0]);
    }

    if (h2State)
    {
        ASSERT_FALSE(h2State->getTransitions().empty()) << "h2 should have default transition";
        ASSERT_FALSE(h2State->getTransitions()[0]->getTargets().empty());
        EXPECT_EQ("player.stopped", h2State->getTransitions()[0]->getTargets()[0]);
    }

    // 'settings' 상태의 전환 확인 (h1, h2를 타겟으로 하는)
    IStateNode *settingsState = model->findStateById("settings");
    ASSERT_TRUE(settingsState != nullptr);

    bool foundBackTransition = false;
    bool foundDeepRestoreTransition = false;

    for (const auto &transition : settingsState->getTransitions())
    {
        if (transition->getEvent() == "back")
        {
            foundBackTransition = true;
            ASSERT_FALSE(transition->getTargets().empty());
            EXPECT_EQ("h1", transition->getTargets()[0]);
        }
        else if (transition->getEvent() == "deep_restore")
        {
            foundDeepRestoreTransition = true;
            ASSERT_FALSE(transition->getTargets().empty());
            EXPECT_EQ("h2", transition->getTargets()[0]);
        }
    }

    EXPECT_TRUE(foundBackTransition) << "Transition from settings to h1 not found";
    EXPECT_TRUE(foundDeepRestoreTransition) << "Transition from settings to h2 not found";
}

// 히스토리 상태 상세 테스트 추가 케이스
TEST_F(SCXMLParserHistoryTest, DetailedHistoryStateTest2)
{
    // 상태 노드 생성 기대
    EXPECT_CALL(*mockFactory, createStateNode(testing::_, testing::_))
        .Times(testing::AtLeast(7)); // 여러 상태 필요

    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="off">
      <state id="off">
        <transition event="power" target="on"/>
      </state>

      <state id="on" initial="player">
        <!-- Shallow 히스토리 - 바로 아래 자식 상태만 기억 -->
        <history id="h1" type="shallow">
          <transition target="player"/>
        </history>

        <!-- Deep 히스토리 - 모든 중첩 상태 기억 -->
        <history id="h2" type="deep">
          <transition target="player.stopped"/>
        </history>

        <state id="player" initial="stopped">
          <state id="stopped">
            <transition event="play" target="playing"/>
          </state>

          <state id="playing">
            <transition event="stop" target="stopped"/>
            <transition event="pause" target="paused"/>
          </state>

          <state id="paused">
            <transition event="play" target="playing"/>
            <transition event="stop" target="stopped"/>
          </state>
        </state>

        <state id="settings">
          <!-- 히스토리 상태로의 전환 -->
          <transition event="back" target="h1"/>
          <transition event="deep_restore" target="h2"/>
        </state>

        <transition event="menu" target="settings"/>
        <transition event="power" target="off"/>
      </state>
    </scxml>)";

    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());

    // 'on' 상태 찾기
    IStateNode *onState = model->findStateById("on");
    ASSERT_TRUE(onState != nullptr);

    // 'on' 상태의 자식들 확인
    const auto &children = onState->getChildren();
    EXPECT_GE(children.size(), 4); // player, settings, h1, h2

    // 히스토리 상태 찾기
    bool foundH1 = false;
    bool foundH2 = false;
    IStateNode *h1State = nullptr;
    IStateNode *h2State = nullptr;

    for (const auto &child : children)
    {
        if (child->getId() == "h1")
        {
            foundH1 = true;
            h1State = child.get();
            EXPECT_EQ(Type::HISTORY, child->getType());
            EXPECT_TRUE(child->isShallowHistory());
            EXPECT_FALSE(child->isDeepHistory());
        }
        else if (child->getId() == "h2")
        {
            foundH2 = true;
            h2State = child.get();
            EXPECT_EQ(Type::HISTORY, child->getType());
            EXPECT_FALSE(child->isShallowHistory());
            EXPECT_TRUE(child->isDeepHistory());
        }
    }

    EXPECT_TRUE(foundH1);
    EXPECT_TRUE(foundH2);

    // 히스토리 상태의 기본 전환 확인
    if (h1State)
    {
        ASSERT_FALSE(h1State->getTransitions().empty());
        ASSERT_FALSE(h1State->getTransitions()[0]->getTargets().empty());
        EXPECT_EQ("player", h1State->getTransitions()[0]->getTargets()[0]);
    }

    if (h2State)
    {
        ASSERT_FALSE(h2State->getTransitions().empty());
        ASSERT_FALSE(h2State->getTransitions()[0]->getTargets().empty());
        EXPECT_EQ("player.stopped", h2State->getTransitions()[0]->getTargets()[0]);
    }

    // settings 상태의 전환 확인 (h1, h2를 타겟으로 하는)
    IStateNode *settingsState = model->findStateById("settings");
    ASSERT_TRUE(settingsState != nullptr);

    bool foundBackTransition = false;
    bool foundDeepRestoreTransition = false;

    for (const auto &transition : settingsState->getTransitions())
    {
        if (transition->getEvent() == "back")
        {
            foundBackTransition = true;
            ASSERT_FALSE(transition->getTargets().empty());
            EXPECT_EQ("h1", transition->getTargets()[0]);
        }
        else if (transition->getEvent() == "deep_restore")
        {
            foundDeepRestoreTransition = true;
            ASSERT_FALSE(transition->getTargets().empty());
            EXPECT_EQ("h2", transition->getTargets()[0]);
        }
    }

    EXPECT_TRUE(foundBackTransition);
    EXPECT_TRUE(foundDeepRestoreTransition);
}
