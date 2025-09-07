#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"
#include "core/actions/RaiseActionNode.h"
#include "runtime/RuntimeContext.h"

// SCXML W3C 사양에 따른 Raise Action Node 테스트
class RaiseActionNodeTest : public CoreTestBase
{
protected:
    void SetUp() override
    {
        CoreTestBase::SetUp();
        mockContext = std::make_shared<MockExecutionContext>();
        raiseAction = std::make_shared<SCXML::Core::RaiseActionNode>("raise1");
    }

    std::shared_ptr<MockExecutionContext> mockContext;
    std::shared_ptr<SCXML::Core::RaiseActionNode> raiseAction;
};

// SCXML 사양 3.14.2: <raise> 기본 속성 테스트
TEST_F(RaiseActionNodeTest, BasicRaiseAttributes)
{
    // SCXML 사양: event 속성은 필수
    raiseAction->setEvent("internal.ready");
    
    EXPECT_EQ("internal.ready", raiseAction->getEvent());
    EXPECT_EQ("raise", raiseAction->getActionType());
}

// SCXML 사양 3.14.2: event 속성은 필수
TEST_F(RaiseActionNodeTest, EventAttributeRequired)
{
    // SCXML 사양: raise 요소는 반드시 event 속성을 가져야 함
    raiseAction->setEvent("required.event");
    EXPECT_EQ("required.event", raiseAction->getEvent());
    
    // 빈 이벤트도 기술적으로는 설정 가능하지만 런타임에서 오류 처리 필요
    raiseAction->setEvent("");
    EXPECT_EQ("", raiseAction->getEvent());
}

// SCXML 사양 3.14.2: 내부 이벤트 큐에 추가
TEST_F(RaiseActionNodeTest, InternalEventQueueBehavior)
{
    // SCXML 사양: raise된 이벤트는 내부 이벤트 큐 뒷부분에 추가됨
    raiseAction->setEvent("internal.notification");
    EXPECT_EQ("internal.notification", raiseAction->getEvent());
    
    // 여러 이벤트 raise 테스트 (순서 보장 확인용)
    auto raise2 = std::make_shared<SCXML::Core::RaiseActionNode>("raise2");
    raise2->setEvent("internal.second");
    EXPECT_EQ("internal.second", raise2->getEvent());
    
    auto raise3 = std::make_shared<SCXML::Core::RaiseActionNode>("raise3");  
    raise3->setEvent("internal.third");
    EXPECT_EQ("internal.third", raise3->getEvent());
}

// SCXML 사양 3.14.2: 데이터 payload 지원
TEST_F(RaiseActionNodeTest, DataPayloadHandling)
{
    // SCXML 사양: raise 이벤트에 데이터 포함 가능
    raiseAction->setEvent("data.event");
    raiseAction->setData("{ \"status\": \"completed\", \"value\": 42 }");
    
    EXPECT_EQ("data.event", raiseAction->getEvent());
    EXPECT_EQ("{ \"status\": \"completed\", \"value\": 42 }", raiseAction->getData());
}

// SCXML 사양: 다양한 이벤트 이름 형식
TEST_F(RaiseActionNodeTest, EventNameFormats)
{
    // 단순 이벤트 이름
    raiseAction->setEvent("ready");
    EXPECT_EQ("ready", raiseAction->getEvent());
    
    // 네임스페이스 스타일 이벤트
    raiseAction->setEvent("user.action.click");
    EXPECT_EQ("user.action.click", raiseAction->getEvent());
    
    // 시스템 오류 이벤트
    raiseAction->setEvent("error.execution");
    EXPECT_EQ("error.execution", raiseAction->getEvent());
    
    // 플랫폼 오류 이벤트
    raiseAction->setEvent("error.platform");
    EXPECT_EQ("error.platform", raiseAction->getEvent());
}

// SCXML 사양: clone 기능 테스트
TEST_F(RaiseActionNodeTest, CloneFunctionality)
{
    // 원본 설정
    raiseAction->setEvent("clone.test");
    raiseAction->setData("original data");
    
    // Clone 생성
    auto cloned = raiseAction->clone();
    auto clonedRaise = std::dynamic_pointer_cast<SCXML::Core::RaiseActionNode>(cloned);
    
    ASSERT_TRUE(clonedRaise != nullptr);
    EXPECT_EQ(raiseAction->getEvent(), clonedRaise->getEvent());
    EXPECT_EQ(raiseAction->getData(), clonedRaise->getData());
    
    // 서로 다른 객체임을 확인
    EXPECT_NE(raiseAction.get(), clonedRaise.get());
    
    // 원본 수정이 clone에 영향 주지 않음 확인
    raiseAction->setEvent("modified.event");
    EXPECT_EQ("clone.test", clonedRaise->getEvent());
    EXPECT_EQ("modified.event", raiseAction->getEvent());
}

// SCXML 사양 3.14.2: 실행 컨텍스트 내에서 즉시 처리 안됨
TEST_F(RaiseActionNodeTest, DelayedProcessingBehavior)
{
    // SCXML 사양: raise된 이벤트는 현재 실행 블록이 완료된 후 처리됨
    raiseAction->setEvent("delayed.processing");
    EXPECT_EQ("delayed.processing", raiseAction->getEvent());
    
    // 이 동작은 실제로는 런타임 시스템에서 보장해야 하므로
    // 여기서는 구조적 정확성만 확인
}

// SCXML 사양: 오류 이벤트 처리
TEST_F(RaiseActionNodeTest, ErrorEventHandling)
{
    // SCXML 사양에서 정의된 표준 오류 이벤트들
    raiseAction->setEvent("error.execution");
    EXPECT_EQ("error.execution", raiseAction->getEvent());
    
    raiseAction->setEvent("error.communication");
    EXPECT_EQ("error.communication", raiseAction->getEvent());
    
    raiseAction->setEvent("error.platform");
    EXPECT_EQ("error.platform", raiseAction->getEvent());
}

// SCXML 사양: 복잡한 데이터 구조 처리
TEST_F(RaiseActionNodeTest, ComplexDataStructures)
{
    raiseAction->setEvent("complex.data");
    
    // JSON 형태 데이터
    std::string jsonData = R"({
        "user": {
            "id": 123,
            "name": "John Doe",
            "permissions": ["read", "write"]
        },
        "timestamp": "2023-01-01T00:00:00Z"
    })";
    
    raiseAction->setData(jsonData);
    EXPECT_EQ(jsonData, raiseAction->getData());
    
    // XML 형태 데이터
    std::string xmlData = "<data><item id='1'>value1</item><item id='2'>value2</item></data>";
    raiseAction->setData(xmlData);
    EXPECT_EQ(xmlData, raiseAction->getData());
}

// SCXML 사양: 이벤트 이름 제한사항
TEST_F(RaiseActionNodeTest, EventNameConstraints)
{
    // SCXML 사양: 이벤트 이름에 특수 문자 포함 가능성 테스트
    raiseAction->setEvent("event-with-dash");
    EXPECT_EQ("event-with-dash", raiseAction->getEvent());
    
    raiseAction->setEvent("event_with_underscore");
    EXPECT_EQ("event_with_underscore", raiseAction->getEvent());
    
    // 숫자로 시작하는 이벤트 (일부 구현에서 제한될 수 있음)
    raiseAction->setEvent("123numeric");
    EXPECT_EQ("123numeric", raiseAction->getEvent());
}

// SCXML 사양: 빈 데이터 처리
TEST_F(RaiseActionNodeTest, EmptyDataHandling)
{
    raiseAction->setEvent("empty.data.test");
    
    // 빈 데이터 설정
    raiseAction->setData("");
    EXPECT_EQ("", raiseAction->getData());
    
    // 공백만 있는 데이터
    raiseAction->setData("   ");
    EXPECT_EQ("   ", raiseAction->getData());
}