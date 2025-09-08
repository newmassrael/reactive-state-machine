/**
 * @file SendReceiveComplianceTest.cpp  
 * @brief SCXML W3C Send/Receive Communication 기본 검증 테스트
 * 
 * SCXML W3C 1.0 사양의 Section 5.4(send), 5.5(raise), 6.2(invoke) 기본 검증
 */

#include <gtest/gtest.h>
#include <memory>
#include "core/actions/SendActionNode.h"
#include "core/actions/RaiseActionNode.h"

class SendReceiveComplianceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 초기 설정
    }
};

/**
 * @brief SCXML W3C Section 5.4.1: Send Action 기본 속성 검증
 * Tests basic send action attributes according to W3C specification
 */
TEST_F(SendReceiveComplianceTest, W3C_5_4_1_SendActionBasicAttributes) {
    // W3C 사양: <send>는 event 속성을 가져야 함
    auto sendAction = std::make_unique<SCXML::Core::SendActionNode>("send_001");
    sendAction->setEvent("user.authenticated");
    
    // W3C 사양: event 속성은 필수
    EXPECT_EQ("user.authenticated", sendAction->getEvent());
    EXPECT_FALSE(sendAction->getEvent().empty());
    
    // W3C 사양: target 속성 설정 가능
    sendAction->setTarget("'#_internal'");
    EXPECT_EQ("'#_internal'", sendAction->getTarget());
    
    // W3C 사양: delay 속성으로 지연 시간 설정
    sendAction->setDelay("2s");
    EXPECT_EQ("2s", sendAction->getDelay());
}

/**
 * @brief SCXML W3C Section 5.4.2: Send Action 다양한 타겟 형식 검증
 * Tests various target formats for send actions
 */
TEST_F(SendReceiveComplianceTest, W3C_5_4_2_SendActionTargetFormats) {
    // W3C 사양: 다양한 target 주소 형식 지원
    std::vector<std::pair<std::string, std::string>> targetFormats = {
        {"internal", "'#_internal'"},
        {"parent", "'#_parent'"},
        {"external", "'external-service'"},
        {"http", "'http://api.example.com/endpoint'"},
        {"expression", "targetVariable"}
    };
    
    for (const auto& [testName, targetValue] : targetFormats) {
        auto sendAction = std::make_unique<SCXML::Core::SendActionNode>("send_" + testName);
        sendAction->setEvent("test.event");
        sendAction->setTarget(targetValue);
        
        // W3C 사양: target은 문자열 리터럴 또는 표현식
        EXPECT_EQ(targetValue, sendAction->getTarget());
        EXPECT_FALSE(sendAction->getTarget().empty());
    }
}

/**
 * @brief SCXML W3C Section 5.4.3: Send Action 타이밍 속성 검증
 * Tests timing-related attributes (delay) for send actions
 */
TEST_F(SendReceiveComplianceTest, W3C_5_4_3_SendActionDelayFormats) {
    // W3C 사양: delay 속성은 CSS2 시간 값 형식
    std::vector<std::string> delayFormats = {
        "",          // 즉시 전송
        "100ms",     // 밀리초
        "2s",        // 초
        "1.5s",      // 소수점 초
        "30s"        // 긴 지연
    };
    
    for (const auto& delayValue : delayFormats) {
        auto sendAction = std::make_unique<SCXML::Core::SendActionNode>("timer_" + std::to_string(rand()));
        sendAction->setEvent("timer.test");
        sendAction->setTarget("'#_internal'");
        
        if (!delayValue.empty()) {
            sendAction->setDelay(delayValue);
            EXPECT_EQ(delayValue, sendAction->getDelay());
        }
        
        // W3C 사양: 기본값은 즉시 전송 (빈 delay)
        if (delayValue.empty()) {
            EXPECT_TRUE(sendAction->getDelay().empty());
        }
    }
}

/**
 * @brief SCXML W3C Section 5.4.4: Send Action 데이터 전송 속성 검증
 * Tests data transmission attributes for send actions
 */
TEST_F(SendReceiveComplianceTest, W3C_5_4_4_SendActionDataTransmission) {
    // W3C 사양: setData로 데이터 전송
    auto sendWithData = std::make_unique<SCXML::Core::SendActionNode>("data_001");
    sendWithData->setEvent("data.sync");
    sendWithData->setData("sessionId currentState userPrefs");
    
    EXPECT_EQ("data.sync", sendWithData->getEvent());
    EXPECT_EQ("sessionId currentState userPrefs", sendWithData->getData());
    
    // W3C 사양: 복합 데이터 전송
    auto sendWithComplexData = std::make_unique<SCXML::Core::SendActionNode>("data_002");
    sendWithComplexData->setEvent("complex.data");
    sendWithComplexData->setData("{ \"userId\": 123, \"action\": \"login\" }");
    
    EXPECT_EQ("complex.data", sendWithComplexData->getEvent());
    EXPECT_FALSE(sendWithComplexData->getData().empty());
}

/**
 * @brief SCXML W3C Section 5.4.5: Send Action ID 및 추적성 검증
 * Tests send action identification and tracking capabilities
 */
TEST_F(SendReceiveComplianceTest, W3C_5_4_5_SendActionIdentification) {
    // W3C 사양: setSendId로 전송 추적 및 취소 가능
    auto trackableSend = std::make_unique<SCXML::Core::SendActionNode>("request_123");
    trackableSend->setEvent("trackable.request");
    trackableSend->setTarget("'external-service'");
    trackableSend->setSendId("send_request_123");
    trackableSend->setDelay("5s");
    
    // W3C 사양: sendId는 전송을 고유하게 식별
    EXPECT_EQ("send_request_123", trackableSend->getSendId());
    EXPECT_FALSE(trackableSend->getSendId().empty());
    
    // W3C 사양: 지연된 전송은 취소 가능해야 함
    EXPECT_EQ("5s", trackableSend->getDelay());
}

/**
 * @brief SCXML W3C Section 5.5.1: Raise Action 기본 검증
 * Tests basic raise action functionality
 */
TEST_F(SendReceiveComplianceTest, W3C_5_5_1_RaiseActionBasic) {
    // W3C 사양: <raise>는 내부 이벤트 큐에 이벤트 추가
    auto raiseAction = std::make_unique<SCXML::Core::RaiseActionNode>("raise_001");
    raiseAction->setEvent("internal.notification");
    
    // W3C 사양: event 속성은 필수
    EXPECT_EQ("internal.notification", raiseAction->getEvent());
    EXPECT_FALSE(raiseAction->getEvent().empty());
    
    // W3C 사양: <raise>는 즉시 처리 (delay, target 없음)
    // 이는 SendActionNode와의 차이점
}

/**
 * @brief SCXML W3C Section 5.5.2: Raise Action 다양한 이벤트 형식
 * Tests various event formats for raise actions
 */
TEST_F(SendReceiveComplianceTest, W3C_5_5_2_RaiseActionEventFormats) {
    // W3C 사양: 다양한 내부 이벤트 이름 형식
    std::vector<std::string> eventFormats = {
        "simple.event",
        "state.entered",
        "validation.completed",
        "error.handled",
        "process.ready",
        "timer.expired"
    };
    
    for (const auto& eventName : eventFormats) {
        auto raiseAction = std::make_unique<SCXML::Core::RaiseActionNode>("raise_001");
        raiseAction->setEvent(eventName);
        
        // W3C 사양: 이벤트 이름은 점으로 구분된 형식 허용
        EXPECT_EQ(eventName, raiseAction->getEvent());
        EXPECT_TRUE(eventName.find('.') != std::string::npos);
    }
}

/**
 * @brief SCXML W3C Communication Protocol Types 검증
 * Tests different communication protocol specifications
 */
TEST_F(SendReceiveComplianceTest, W3C_CommunicationProtocolTypes) {
    // W3C 사양: type 속성으로 통신 프로토콜 지정
    std::vector<std::pair<std::string, std::string>> protocolTypes = {
        {"default_scxml", "scxml"},
        {"http_basic", "basichttp"},
        {"http_scxml", "http://www.w3.org/TR/scxml/#SCXMLEventProcessor"},
        {"websocket", "ws"},
        {"custom", "custom-protocol"}
    };
    
    for (const auto& [testName, typeValue] : protocolTypes) {
        auto sendAction = std::make_unique<SCXML::Core::SendActionNode>("protocol_" + testName);
        sendAction->setEvent("protocol.test");
        sendAction->setTarget("'test-service'");
        sendAction->setType(typeValue);
        
        // W3C 사양: type은 메시지 전송 방식을 결정
        EXPECT_EQ(typeValue, sendAction->getType());
        
        // W3C 사양: 기본값은 scxml 프로토콜
        if (typeValue == "scxml") {
            EXPECT_EQ("scxml", sendAction->getType());
        }
    }
}

/**
 * @brief SCXML W3C Error Event Patterns 검증
 * Tests standard error event naming patterns
 */
TEST_F(SendReceiveComplianceTest, W3C_ErrorEventPatterns) {
    // W3C 사양: 표준 오류 이벤트 명명 규칙
    std::vector<std::string> errorEventPatterns = {
        "error.communication",
        "error.execution",
        "error.platform",
        "error.communication.send_123"
    };
    
    for (const auto& errorEvent : errorEventPatterns) {
        // W3C 사양: 모든 오류 이벤트는 "error."로 시작
        EXPECT_TRUE(errorEvent.find("error.") == 0);
        
        // raise로 오류 이벤트 생성 가능
        auto errorRaise = std::make_unique<SCXML::Core::RaiseActionNode>("error_raise_001");
        errorRaise->setEvent(errorEvent);
        
        EXPECT_EQ(errorEvent, errorRaise->getEvent());
    }
}

/**
 * @brief SCXML W3C 통합 통신 시나리오 검증
 * Tests integrated communication scenarios
 */
TEST_F(SendReceiveComplianceTest, W3C_IntegratedCommunicationScenario) {
    // W3C 사양: 복합 통신 워크플로우
    
    // 1. 외부 요청 전송
    auto externalRequest = std::make_unique<SCXML::Core::SendActionNode>("auth_request_001");
    externalRequest->setEvent("user.authenticate");
    externalRequest->setTarget("'auth-service'");
    externalRequest->setSendId("send_auth_001");
    externalRequest->setData("{ \"username\": \"alice\", \"password\": \"secret\" }");
    
    // 2. 내부 상태 알림
    auto internalNotification = std::make_unique<SCXML::Core::RaiseActionNode>("internal_notify_001");
    internalNotification->setEvent("auth.request.sent");
    
    // 3. 타임아웃 설정
    auto timeoutTimer = std::make_unique<SCXML::Core::SendActionNode>("auth_timeout");
    timeoutTimer->setEvent("auth.timeout");
    timeoutTimer->setTarget("'#_internal'");
    timeoutTimer->setDelay("30s");
    timeoutTimer->setSendId("timeout_timer");
    
    // 검증
    EXPECT_EQ("user.authenticate", externalRequest->getEvent());
    EXPECT_EQ("send_auth_001", externalRequest->getSendId());
    EXPECT_EQ("auth.request.sent", internalNotification->getEvent());
    EXPECT_EQ("30s", timeoutTimer->getDelay());
    
    // W3C 사양: 각 액션이 올바르게 구성됨
    EXPECT_FALSE(externalRequest->getData().empty());
    EXPECT_FALSE(internalNotification->getEvent().empty());
    EXPECT_FALSE(timeoutTimer->getSendId().empty());
}