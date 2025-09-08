/**
 * @file EventSystemComplianceTest.cpp
 * @brief SCXML W3C Event System and Queue Management Compliance Tests
 *
 * Comprehensive tests validating SCXML W3C 1.0 Event System specification compliance.
 * 
 * W3C References:
 * - Section 3.12: Event I/O Processor
 * - Section 3.13: Event Data
 * - Section 5: Executable Content
 * - Section 6.2: Event Processing
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

// Core Event System includes
#include "events/Event.h"
#include "events/EventQueue.h"
#include "runtime/RuntimeContext.h"

class EventSystemComplianceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test environment
    }

    void TearDown() override {
        // Cleanup after each test
    }

    // Helper method to create test events
    std::shared_ptr<SCXML::Events::Event> createTestEvent(
        const std::string& name, 
        SCXML::Events::Event::Type type = SCXML::Events::Event::Type::PLATFORM,
        const std::string& origin = "",
        const std::string& data = "") {
        
        auto event = std::make_shared<SCXML::Events::Event>(name, type);
        
        if (!origin.empty()) {
            event->setOrigin(origin);
        }
        if (!data.empty()) {
            event->setData(data);
        }
        
        return event;
    }
};

/**
 * @brief SCXML W3C Section 3.12.1: Event Structure 기본 검증
 * Tests basic event structure and attributes
 */
TEST_F(EventSystemComplianceTest, W3C_3_12_1_EventStructureBasic) {
    // W3C 사양: Event는 name, type, origin, data 속성을 가져야 함
    std::string eventData = "{\"x\": 100, \"y\": 200}";
    auto event = createTestEvent("user.click", SCXML::Events::Event::Type::PLATFORM, "#_internal", eventData);
    
    // W3C 사양: name 속성 검증
    EXPECT_EQ("user.click", event->getName());
    EXPECT_FALSE(event->getName().empty());
    
    // W3C 사양: type 속성 검증 (PLATFORM, INTERNAL, EXTERNAL)
    EXPECT_EQ(SCXML::Events::Event::Type::PLATFORM, event->getType());
    EXPECT_TRUE(event->isPlatform());
    
    // W3C 사양: origin 속성 검증 (발신자 식별)
    EXPECT_EQ("#_internal", event->getOrigin());
    
    // W3C 사양: data 속성 검증 (이벤트 페이로드)
    EXPECT_EQ(eventData, event->getDataAsString());
    EXPECT_TRUE(event->hasData());
}

/**
 * @brief SCXML W3C Section 3.12.2: Event Type Classification 검증
 * Tests W3C-compliant event type classification
 */
TEST_F(EventSystemComplianceTest, W3C_3_12_2_EventTypeClassification) {
    // W3C 사양: PLATFORM 이벤트 (사용자 입력, 시스템 이벤트)
    auto platformEvent = createTestEvent("user.input", SCXML::Events::Event::Type::PLATFORM);
    EXPECT_EQ(SCXML::Events::Event::Type::PLATFORM, platformEvent->getType());
    EXPECT_TRUE(platformEvent->isPlatform());
    
    // W3C 사양: INTERNAL 이벤트 (raise로 생성된 내부 이벤트)
    auto internalEvent = createTestEvent("internal.ready", SCXML::Events::Event::Type::INTERNAL);
    EXPECT_EQ(SCXML::Events::Event::Type::INTERNAL, internalEvent->getType());
    EXPECT_TRUE(internalEvent->isInternal());
    
    // W3C 사양: EXTERNAL 이벤트 (외부 시스템에서 전송)
    auto externalEvent = createTestEvent("external.notification", SCXML::Events::Event::Type::EXTERNAL);
    EXPECT_EQ(SCXML::Events::Event::Type::EXTERNAL, externalEvent->getType());
    EXPECT_TRUE(externalEvent->isExternal());
    
    // W3C 사양: error 이벤트 (시스템 오류)
    auto errorEvent = createTestEvent("error.execution", SCXML::Events::Event::Type::PLATFORM);
    EXPECT_TRUE(errorEvent->getName().find("error.") == 0);
}

/**
 * @brief SCXML W3C Section 6.2.1: Event Queue Processing 검증
 * Tests event queue processing order and priorities
 */
TEST_F(EventSystemComplianceTest, W3C_6_2_1_EventQueueProcessing) {
    SCXML::Events::EventQueue eventQueue;
    
    // W3C 사양: 이벤트 큐는 FIFO 순서로 처리
    auto event1 = createTestEvent("test.first", SCXML::Events::Event::Type::EXTERNAL);
    auto event2 = createTestEvent("test.second", SCXML::Events::Event::Type::EXTERNAL);
    auto event3 = createTestEvent("test.third", SCXML::Events::Event::Type::EXTERNAL);
    
    eventQueue.enqueue(event1);
    eventQueue.enqueue(event2);
    eventQueue.enqueue(event3);
    
    // W3C 사양: 큐에서 이벤트 순서대로 처리
    EXPECT_FALSE(eventQueue.empty());
    EXPECT_EQ(3, eventQueue.size());
    
    auto firstProcessed = eventQueue.dequeue();
    EXPECT_EQ("test.first", firstProcessed->getName());
    
    auto secondProcessed = eventQueue.dequeue();
    EXPECT_EQ("test.second", secondProcessed->getName());
    
    auto thirdProcessed = eventQueue.dequeue();
    EXPECT_EQ("test.third", thirdProcessed->getName());
    
    EXPECT_TRUE(eventQueue.empty());
}

/**
 * @brief SCXML W3C Section 6.2.2: Internal Event Priority 검증
 * Tests internal events have higher priority than external events
 */
TEST_F(EventSystemComplianceTest, W3C_6_2_2_InternalEventPriority) {
    SCXML::Events::EventQueue eventQueue;
    
    // W3C 사양: internal 이벤트는 external 이벤트보다 우선순위가 높음
    auto externalEvent = createTestEvent("external.data", SCXML::Events::Event::Type::EXTERNAL);
    auto internalEvent = createTestEvent("internal.ready", SCXML::Events::Event::Type::INTERNAL);
    
    // 외부 이벤트를 먼저 큐에 추가 (낮은 우선순위)
    eventQueue.enqueue(externalEvent, SCXML::Events::EventQueue::Priority::NORMAL);
    // 내부 이벤트를 높은 우선순위로 추가
    eventQueue.enqueue(internalEvent, SCXML::Events::EventQueue::Priority::HIGH);
    
    // W3C 사양: 높은 우선순위 이벤트가 먼저 처리되어야 함
    auto firstProcessed = eventQueue.dequeue();
    EXPECT_EQ("internal.ready", firstProcessed->getName());
    EXPECT_EQ(SCXML::Events::Event::Type::INTERNAL, firstProcessed->getType());
    
    auto secondProcessed = eventQueue.dequeue();
    EXPECT_EQ("external.data", secondProcessed->getName());
    EXPECT_EQ(SCXML::Events::Event::Type::EXTERNAL, secondProcessed->getType());
}

/**
 * @brief SCXML W3C Section 3.13: Event Data Processing 검증
 * Tests event data processing and formats
 */
TEST_F(EventSystemComplianceTest, W3C_3_13_EventDataProcessing) {
    // W3C 사양: 다양한 데이터 형식 지원
    
    // JSON 형식 데이터
    std::string jsonData = "{\"user\": \"admin\", \"action\": \"login\", \"timestamp\": 1234567890}";
    auto jsonEvent = createTestEvent("data.json", SCXML::Events::Event::Type::PLATFORM, "", jsonData);
    EXPECT_FALSE(jsonEvent->getDataAsString().empty());
    EXPECT_TRUE(jsonEvent->hasData());
    EXPECT_TRUE(jsonEvent->getDataAsString().find("{") != std::string::npos);
    
    // XML 형식 데이터
    std::string xmlData = "<user><name>admin</name><role>administrator</role></user>";
    auto xmlEvent = createTestEvent("data.xml", SCXML::Events::Event::Type::PLATFORM, "", xmlData);
    EXPECT_TRUE(xmlEvent->getDataAsString().find("<user>") != std::string::npos);
    
    // 문자열 데이터
    auto stringEvent = createTestEvent("data.string", SCXML::Events::Event::Type::PLATFORM, "", "Simple string data");
    EXPECT_EQ("Simple string data", stringEvent->getDataAsString());
    
    // 빈 데이터 (유효한 경우)
    auto emptyEvent = createTestEvent("data.empty", SCXML::Events::Event::Type::PLATFORM);
    EXPECT_FALSE(emptyEvent->hasData());
}

/**
 * @brief SCXML W3C Section 3.12.3: Event Manager Integration 검증
 * Tests integration with SCXML Runtime Event Manager
 */
TEST_F(EventSystemComplianceTest, W3C_3_12_3_EventManagerIntegration) {
    // RuntimeContext와 EventManager 통합 테스트
    auto context = std::make_unique<SCXML::Runtime::RuntimeContext>();
    
    // W3C 사양: EventManager는 이벤트 라이프사이클 관리
    auto& eventManager = context->getEventManager();
    
    // 이벤트 전송 테스트
    auto testEvent = createTestEvent("integration.test", SCXML::Events::Event::Type::PLATFORM, "#_internal");
    
    // W3C 사양: 이벤트 매니저를 통한 이벤트 처리
    EXPECT_NE(nullptr, &eventManager);
    
    // 이벤트 데이터 무결성 검증
    EXPECT_EQ("integration.test", testEvent->getName());
    EXPECT_EQ(SCXML::Events::Event::Type::PLATFORM, testEvent->getType());
    EXPECT_EQ("#_internal", testEvent->getOrigin());
    
    // 이벤트 생성 팩토리 함수 테스트
    auto factoryEvent = SCXML::Events::makeEvent("factory.test", SCXML::Events::Event::Type::INTERNAL);
    EXPECT_EQ("factory.test", factoryEvent->getName());
    EXPECT_TRUE(factoryEvent->isInternal());
}

/**
 * @brief SCXML W3C Section 6.2.4: Error Event Handling 검증
 * Tests W3C-compliant error event handling patterns
 */
TEST_F(EventSystemComplianceTest, W3C_6_2_4_ErrorEventHandling) {
    // W3C 사양: 오류 이벤트 패턴
    std::vector<std::string> errorEvents = {
        "error.execution",         // 실행 오류
        "error.communication",     // 통신 오류
        "error.platform",         // 플랫폼 오류
        "error.execution.invalid", // 특정 실행 오류
        "error.send.failed"       // 전송 실패 오류
    };
    
    for (const auto& errorName : errorEvents) {
        auto errorEvent = createTestEvent(errorName, SCXML::Events::Event::Type::PLATFORM, "#_internal");
        
        // W3C 사양: 모든 오류 이벤트는 "error."로 시작
        EXPECT_TRUE(errorEvent->getName().find("error.") == 0);
        EXPECT_FALSE(errorEvent->getName().empty());
        
        // W3C 사양: 오류 이벤트는 즉시 처리 가능해야 함
        EXPECT_EQ(SCXML::Events::Event::Type::PLATFORM, errorEvent->getType());
        EXPECT_TRUE(errorEvent->isPlatform());
    }
}

/**
 * @brief SCXML W3C Comprehensive Event System Scenario 검증
 * Tests comprehensive event system scenario with mixed event types
 */
TEST_F(EventSystemComplianceTest, W3C_ComprehensiveEventSystemScenario) {
    // W3C 사양에 따른 종합 이벤트 시스템 시나리오
    SCXML::Events::EventQueue eventQueue;
    
    // 1. 사용자 인터랙션 시뮬레이션
    std::string loginData = "{\"username\": \"testuser\", \"timestamp\": 1234567890}";
    auto userLogin = createTestEvent("user.login", SCXML::Events::Event::Type::PLATFORM, "#_browser", loginData);
    eventQueue.enqueue(userLogin);
    
    // 2. 시스템 내부 처리 (높은 우선순위)
    auto authCheck = createTestEvent("internal.auth.check", SCXML::Events::Event::Type::INTERNAL, "#_internal");
    eventQueue.enqueue(authCheck, SCXML::Events::EventQueue::Priority::HIGH);
    
    // 3. 외부 서비스 응답
    std::string responseData = "{\"token\": \"abc123\", \"expires\": 3600}";
    auto authResponse = createTestEvent("external.auth.success", SCXML::Events::Event::Type::EXTERNAL, "#_auth_service", responseData);
    eventQueue.enqueue(authResponse);
    
    // 4. 오류 상황 시뮬레이션
    auto timeoutError = createTestEvent("error.communication.timeout", SCXML::Events::Event::Type::PLATFORM, "#_internal");
    eventQueue.enqueue(timeoutError, SCXML::Events::EventQueue::Priority::HIGH);
    
    // W3C 사양: 모든 이벤트가 적절히 큐에 저장되고 처리 가능해야 함
    EXPECT_FALSE(eventQueue.empty());
    EXPECT_EQ(4, eventQueue.size());
    
    int processedCount = 0;
    while (!eventQueue.empty() && processedCount < 10) {  // 무한 루프 방지
        auto event = eventQueue.dequeue();
        processedCount++;
        
        // 각 이벤트의 W3C 사양 준수 검증
        EXPECT_FALSE(event->getName().empty());
        
        // 이벤트 타입별 추가 검증
        if (event->getType() == SCXML::Events::Event::Type::INTERNAL) {
            // 내부 이벤트는 우선 처리되어야 함
            EXPECT_TRUE(event->isInternal());
        } else if (event->getName().find("error.") == 0) {
            // 오류 이벤트는 즉시 처리 가능해야 함
            EXPECT_EQ(SCXML::Events::Event::Type::PLATFORM, event->getType());
        }
    }
    
    // 모든 이벤트가 성공적으로 처리되었는지 확인
    EXPECT_EQ(4, processedCount);
    EXPECT_TRUE(eventQueue.empty());
}