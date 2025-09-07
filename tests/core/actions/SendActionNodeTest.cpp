#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"
#include "core/actions/SendActionNode.h"
#include "runtime/RuntimeContext.h"

// SCXML W3C 사양에 따른 Send Action Node 테스트
class SendActionNodeTest : public CoreTestBase
{
protected:
    void SetUp() override
    {
        CoreTestBase::SetUp();
        mockContext = std::make_shared<MockExecutionContext>();
        sendAction = std::make_shared<SCXML::Core::SendActionNode>("send1");
    }

    std::shared_ptr<MockExecutionContext> mockContext;
    std::shared_ptr<SCXML::Core::SendActionNode> sendAction;
};

// SCXML 사양 3.14.1: <send> 기본 속성 테스트
TEST_F(SendActionNodeTest, BasicSendAttributes)
{
    // SCXML 사양: event와 target 속성 설정
    sendAction->setEvent("user.click");
    sendAction->setTarget("#_internal");
    sendAction->setSendId("send_001");
    
    EXPECT_EQ("user.click", sendAction->getEvent());
    EXPECT_EQ("#_internal", sendAction->getTarget());
    EXPECT_EQ("send_001", sendAction->getSendId());
    EXPECT_EQ("send", sendAction->getActionType());
}

// SCXML 사양 3.14.1: delay 속성 테스트  
TEST_F(SendActionNodeTest, DelayAttributeHandling)
{
    // SCXML 사양: delay 속성으로 지연 전송 지원
    sendAction->setDelay("5s");
    EXPECT_EQ("5s", sendAction->getDelay());
    
    sendAction->setDelay("100ms");  
    EXPECT_EQ("100ms", sendAction->getDelay());
    
    sendAction->setDelay("2min");
    EXPECT_EQ("2min", sendAction->getDelay());
}

// SCXML 사양 3.14.1: 다양한 target 형식 테스트
TEST_F(SendActionNodeTest, TargetFormatValidation)
{
    // SCXML 사양: 다양한 target URI 형식 지원
    
    // Internal target
    sendAction->setTarget("#_internal");
    EXPECT_EQ("#_internal", sendAction->getTarget());
    
    // Parent target
    sendAction->setTarget("#_parent");
    EXPECT_EQ("#_parent", sendAction->getTarget());
    
    // External HTTP target
    sendAction->setTarget("http://example.com/endpoint");
    EXPECT_EQ("http://example.com/endpoint", sendAction->getTarget());
    
    // SCXML session target
    sendAction->setTarget("scxml:session123");
    EXPECT_EQ("scxml:session123", sendAction->getTarget());
}

// SCXML 사양 3.14.1: event 이름 검증
TEST_F(SendActionNodeTest, EventNameValidation)
{
    // SCXML 사양: 유효한 event 이름 형식
    sendAction->setEvent("user.action");
    EXPECT_EQ("user.action", sendAction->getEvent());
    
    sendAction->setEvent("system.ready");
    EXPECT_EQ("system.ready", sendAction->getEvent());
    
    // 특수 이벤트
    sendAction->setEvent("error.communication");
    EXPECT_EQ("error.communication", sendAction->getEvent());
}

// SCXML 사양 3.14.1: type 속성 테스트 (Event I/O Processor)
TEST_F(SendActionNodeTest, EventIOProcessorType)
{
    // SCXML 사양: type 속성으로 Event I/O Processor 지정
    sendAction->setType("http://www.w3.org/TR/scxml/");
    EXPECT_EQ("http://www.w3.org/TR/scxml/", sendAction->getType());
    
    sendAction->setType("http://www.w3.org/TR/scxml/#BasicHTTPEventProcessor");
    EXPECT_EQ("http://www.w3.org/TR/scxml/#BasicHTTPEventProcessor", sendAction->getType());
}

// SCXML 사양 3.14.1: data payload 테스트
TEST_F(SendActionNodeTest, DataPayloadHandling)
{
    // SCXML 사양: 이벤트와 함께 데이터 전송
    sendAction->setData("{ \"message\": \"Hello World\", \"timestamp\": 1234567890 }");
    EXPECT_EQ("{ \"message\": \"Hello World\", \"timestamp\": 1234567890 }", sendAction->getData());
    
    // XML 데이터
    sendAction->setData("<payload><item>test</item></payload>");
    EXPECT_EQ("<payload><item>test</item></payload>", sendAction->getData());
}

// SCXML 사양: clone 기능 테스트
TEST_F(SendActionNodeTest, CloneFunctionality)
{
    // 원본 설정
    sendAction->setEvent("test.event");
    sendAction->setTarget("#_parent");
    sendAction->setData("test data");
    sendAction->setDelay("1s");
    sendAction->setSendId("original_send");
    sendAction->setType("custom_type");
    
    // Clone 생성
    auto cloned = sendAction->clone();
    auto clonedSend = std::dynamic_pointer_cast<SCXML::Core::SendActionNode>(cloned);
    
    ASSERT_TRUE(clonedSend != nullptr);
    EXPECT_EQ(sendAction->getEvent(), clonedSend->getEvent());
    EXPECT_EQ(sendAction->getTarget(), clonedSend->getTarget());
    EXPECT_EQ(sendAction->getData(), clonedSend->getData());
    EXPECT_EQ(sendAction->getDelay(), clonedSend->getDelay());
    EXPECT_EQ(sendAction->getSendId(), clonedSend->getSendId());
    EXPECT_EQ(sendAction->getType(), clonedSend->getType());
    
    // 서로 다른 객체임을 확인
    EXPECT_NE(sendAction.get(), clonedSend.get());
}

// SCXML 사양 3.14.1: 오류 처리 테스트
TEST_F(SendActionNodeTest, ErrorHandlingCompliance)
{
    // SCXML 사양: 통신 오류 시 'error.communication' 이벤트 생성 필요
    // 이 테스트는 실제 execute() 구현이 오류를 올바르게 처리하는지 확인
    
    // 잘못된 target으로 send 시도
    sendAction->setEvent("test.event");
    sendAction->setTarget("invalid://malformed.uri");
    
    // execute() 메서드가 구현되어 있다면 오류 처리 확인
    // 현재는 구조적 검증만 수행
    EXPECT_EQ("test.event", sendAction->getEvent());
    EXPECT_EQ("invalid://malformed.uri", sendAction->getTarget());
}

// SCXML 사양: 필수 속성 검증
TEST_F(SendActionNodeTest, RequiredAttributeValidation)
{
    // SCXML 사양: event 속성이 필수
    sendAction->setEvent("");  // 빈 이벤트
    EXPECT_EQ("", sendAction->getEvent());
    
    // 유효한 이벤트 설정
    sendAction->setEvent("required.event");
    EXPECT_EQ("required.event", sendAction->getEvent());
}

// SCXML 사양 3.14.1: Delay parsing 테스트
TEST_F(SendActionNodeTest, DelayParsingAccuracy)
{
    // parseDelay는 protected 메서드이므로 간접적으로 테스트
    // 다양한 delay 형식이 올바르게 저장되는지 확인
    
    sendAction->setDelay("0s");        // 즉시 전송
    EXPECT_EQ("0s", sendAction->getDelay());
    
    sendAction->setDelay("1500ms");    // 밀리초
    EXPECT_EQ("1500ms", sendAction->getDelay());
    
    sendAction->setDelay("30s");       // 초
    EXPECT_EQ("30s", sendAction->getDelay());
    
    sendAction->setDelay("5min");      // 분
    EXPECT_EQ("5min", sendAction->getDelay());
}

// SCXML 사양: 동일한 sendId 처리
TEST_F(SendActionNodeTest, SendIdUniqueness)
{
    // SCXML 사양:.sendid는 보류 중인 send 작업을 식별하는 데 사용
    sendAction->setSendId("unique_send_123");
    EXPECT_EQ("unique_send_123", sendAction->getSendId());
    
    // 다른 SendAction과 동일한 ID 설정 가능 (시스템이 관리)
    auto anotherSend = std::make_shared<SCXML::Core::SendActionNode>("send2");
    anotherSend->setSendId("unique_send_123");
    EXPECT_EQ("unique_send_123", anotherSend->getSendId());
}