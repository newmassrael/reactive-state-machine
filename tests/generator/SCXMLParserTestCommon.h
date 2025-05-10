#pragma once

#include <gtest/gtest.h>
#include "parsing/SCXMLParser.h"
#include "mocks/MockNodeFactory.h"
#include "mocks/MockStateNode.h"
#include "mocks/MockTransitionNode.h"
#include "mocks/MockGuardNode.h"
#include "mocks/MockActionNode.h"
#include "mocks/MockDataModelItem.h"
#include "mocks/MockXIncludeProcessor.h"
#include "mocks/MockInvokeNode.h"
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

// 모든 테스트에서 공유할 기본 테스트 픽스처 클래스
class SCXMLParserTestBase : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockFactory = std::make_shared<MockNodeFactory>();
        parser = std::make_shared<SCXMLParser>(mockFactory);

        ::testing::FLAGS_gmock_verbose = "error";

        // 기본 반환값 설정
        SetupDefaultMockBehavior();

        // 관련 파서들을 연결
        parser->getStateNodeParser()->setRelatedParsers(
            parser->getTransitionParser(),
            parser->getActionParser(),
            parser->getDataModelParser(), parser->getInvokeParser(), parser->getDoneDataParser());

        auto actionParser = std::make_shared<ActionParser>(mockFactory);
        parser->getTransitionParser()->setActionParser(actionParser);
    }

    void TearDown() override
    {
        // Mock 객체가 누수되지 않도록 정리
        testing::Mock::AllowLeak(mockFactory.get());

        // 리소스 정리
        parser.reset();
        mockFactory.reset();
    }

    void SetupDefaultMockBehavior()
    {
        // StateNode Mock 설정
        auto setupMockStateNode = [this](const std::string &id, const Type type)
        {
            auto mockState = std::make_shared<MockStateNode>();
            mockState->id_ = id;
            mockState->type_ = type;

            // 기본 동작 설정 - 이제 MockStateNode의 SetupDefaultBehavior 메서드 사용
            mockState->SetupDefaultBehavior();

            return mockState;
        };

        // TransitionNode Mock 설정
        auto setupMockTransitionNode = [this](const std::string &event, const std::string &target)
        {
            auto mockTransition = std::make_shared<MockTransitionNode>();
            mockTransition->event_ = event;
            mockTransition->targets_ = {target}; // Changed target_ to targets_ and store as vector

            // 기본 동작 설정
            mockTransition->SetupDefaultBehavior();

            return mockTransition;
        };

        // GuardNode Mock 설정
        auto setupMockGuardNode = [this](const std::string &id, const std::string &target)
        {
            auto mockGuard = std::make_shared<MockGuardNode>();
            mockGuard->id_ = id;
            mockGuard->target_ = target;
            mockGuard->SetupDefaultBehavior();

            return mockGuard;
        };

        // ActionNode Mock 설정
        auto setupMockActionNode = [this](const std::string &id)
        {
            auto mockAction = std::make_shared<MockActionNode>();
            mockAction->id_ = id;

            // 기본 동작 설정 메서드 호출
            mockAction->SetupDefaultBehavior();

            return mockAction;
        };

        // DataModelItem Mock 설정
        auto setupMockDataModelItem = [this](const std::string &id, const std::string &expr)
        {
            auto mockDataItem = std::make_shared<MockDataModelItem>();

            // ID와 표현식 설정
            mockDataItem->id_ = id;
            mockDataItem->expr_ = expr;

            // CDATA 콘텐츠 문제 해결을 위한 특별 처리
            if (id == "flag")
            {
                mockDataItem->content_ = "true";
            }

            // 기본 동작 설정 메서드 호출
            mockDataItem->SetupDefaultBehavior();

            return mockDataItem;
        };

        // InvokeNode Mock 설정
        auto setupMockInvokeNode = [this](const std::string &id)
        {
            auto mockInvoke = std::make_shared<MockInvokeNode>();
            mockInvoke->id_ = id;
            mockInvoke->type_ = "http://www.w3.org/TR/scxml/";
            mockInvoke->src_ = "";
            mockInvoke->autoForward_ = false;

            // 기본 동작 설정 메서드 호출
            mockInvoke->SetupDefaultBehavior();

            return mockInvoke;
        };

        // 팩토리 Mock 설정
        ON_CALL(*mockFactory, createStateNode(testing::_, testing::_))
            .WillByDefault(testing::Invoke(setupMockStateNode));
        ON_CALL(*mockFactory, createTransitionNode(testing::_, testing::_))
            .WillByDefault(testing::Invoke(setupMockTransitionNode));
        ON_CALL(*mockFactory, createGuardNode(testing::_, testing::_))
            .WillByDefault(testing::Invoke(setupMockGuardNode));
        ON_CALL(*mockFactory, createActionNode(testing::_))
            .WillByDefault(testing::Invoke(setupMockActionNode));
        ON_CALL(*mockFactory, createDataModelItem(testing::_, testing::_))
            .WillByDefault(testing::Invoke(setupMockDataModelItem));
        ON_CALL(*mockFactory, createInvokeNode(testing::_))
            .WillByDefault(testing::Invoke(setupMockInvokeNode));
    }

    // 테스트용 SCXML 생성
    std::string createBasicTestSCXML()
    {
        return R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <state id="s1">
    <transition event="e1" target="s2"/>
    <onentry>
      <log expr="'Entering S1'"/>
    </onentry>
    <onexit>
      <log expr="'Exiting S1'"/>
    </onexit>
  </state>
  <state id="s2">
    <transition event="e2" target="s3"/>
  </state>
  <state id="s3">
    <transition event="e3" target="s1"/>
  </state>
</scxml>)";
    }

    // 테스트용 SCXML 파일 생성
    std::string createTestSCXMLFile(const std::string &content)
    {
        std::string filename = "test_scxml_" + std::to_string(rand()) + ".xml";
        std::ofstream file(filename);
        file << content;
        file.close();
        return filename;
    }

    std::shared_ptr<MockNodeFactory> mockFactory;
    std::shared_ptr<SCXMLParser> parser;
};
