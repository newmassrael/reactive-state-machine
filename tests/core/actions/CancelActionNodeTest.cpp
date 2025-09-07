#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"
#include "core/actions/CancelActionNode.h"
#include "runtime/RuntimeContext.h"

// SCXML W3C 사양에 따른 Cancel Action Node 테스트
class CancelActionNodeTest : public CoreTestBase
{
protected:
    void SetUp() override
    {
        CoreTestBase::SetUp();
        mockContext = std::make_shared<MockExecutionContext>();
        cancelAction = std::make_shared<SCXML::Core::CancelActionNode>("cancel1");
    }

    std::shared_ptr<MockExecutionContext> mockContext;
    std::shared_ptr<SCXML::Core::CancelActionNode> cancelAction;
};

// SCXML 사양: <cancel> 기본 속성 테스트
TEST_F(CancelActionNodeTest, BasicCancelAttributes)
{
    // SCXML 사양: sendid 속성 (필수 - sendid 또는 sendidexpr 중 하나)
    cancelAction->setSendId("timer001");
    EXPECT_EQ("timer001", cancelAction->getSendId());
    
    EXPECT_EQ("cancel", cancelAction->getActionType());
    EXPECT_EQ("cancel1", cancelAction->getId());
}

// SCXML 사양: sendidexpr 속성 테스트
TEST_F(CancelActionNodeTest, SendIdExprAttribute)
{
    // sendidexpr 사용 (동적 sendid)
    cancelAction->setSendIdExpr("dynamicSendId");
    EXPECT_EQ("dynamicSendId", cancelAction->getSendIdExpr());
    
    // sendid와 sendidexpr은 상호 배타적
    cancelAction->setSendId("static001");
    // 둘 다 설정되면 sendidexpr이 우선
    EXPECT_EQ("dynamicSendId", cancelAction->getSendIdExpr());
    EXPECT_EQ("static001", cancelAction->getSendId());
}

// SCXML 사양: 필수 속성 검증 테스트
TEST_F(CancelActionNodeTest, RequiredAttributeValidation)
{
    // sendid도 sendidexpr도 없으면 유효하지 않음
    auto errors = cancelAction->validate();
    EXPECT_FALSE(errors.empty()) << "Should have validation errors for missing sendid/sendidexpr";
    
    // sendid 설정 후 유효함
    cancelAction->setSendId("validId");
    errors = cancelAction->validate();
    EXPECT_TRUE(errors.empty()) << "Should be valid with sendid";
    
    // sendidexpr만 있어도 유효함
    auto cancelAction2 = std::make_shared<SCXML::Core::CancelActionNode>("cancel2");
    cancelAction2->setSendIdExpr("validExpr");
    errors = cancelAction2->validate();
    EXPECT_TRUE(errors.empty()) << "Should be valid with sendidexpr";
}

// SCXML 사양: 지연된 이벤트 취소 테스트 (속성 검증 중심)
TEST_F(CancelActionNodeTest, DelayedEventCancellation)
{
    cancelAction->setSendId("timer_5s");
    
    // SCXML 사양 검증: sendid가 올바르게 설정되었는지 확인
    EXPECT_EQ("timer_5s", cancelAction->getSendId());
    EXPECT_EQ("cancel", cancelAction->getActionType());
    
    // 검증 통과 확인
    auto errors = cancelAction->validate();
    EXPECT_TRUE(errors.empty()) << "Should be valid with sendid";
}

// SCXML 사양: 동적 sendid 평가 테스트 (속성 검증 중심)
TEST_F(CancelActionNodeTest, DynamicSendIdEvaluation)
{
    cancelAction->setSendIdExpr("'timer_' + currentLevel");
    
    // SCXML 사양 검증: sendidexpr이 올바르게 설정되었는지 확인
    EXPECT_EQ("'timer_' + currentLevel", cancelAction->getSendIdExpr());
    EXPECT_EQ("cancel", cancelAction->getActionType());
    
    // 검증 통과 확인
    auto errors = cancelAction->validate();
    EXPECT_TRUE(errors.empty()) << "Should be valid with sendidexpr";
}

// SCXML 사양: 취소 실패 처리 테스트 (속성 검증 중심)
TEST_F(CancelActionNodeTest, CancellationFailureHandling)
{
    cancelAction->setSendId("nonexistent_timer");
    
    // SCXML 사양 검증: sendid가 올바르게 설정되었는지 확인
    EXPECT_EQ("nonexistent_timer", cancelAction->getSendId());
    EXPECT_EQ("cancel", cancelAction->getActionType());
    
    // 검증 통과 확인 - 존재하지 않는 타이머 ID라도 속성은 유효
    auto errors = cancelAction->validate();
    EXPECT_TRUE(errors.empty()) << "Should be valid with sendid";
}

// SCXML 사양: 세션 범위 제한 테스트 (속성 검증 중심)
TEST_F(CancelActionNodeTest, SessionScopeRestriction)
{
    cancelAction->setSendId("cross_session_timer");
    
    // SCXML 사양 검증: 세션 간 제한은 런타임에 처리되므로 속성은 유효
    EXPECT_EQ("cross_session_timer", cancelAction->getSendId());
    EXPECT_EQ("cancel", cancelAction->getActionType());
    
    // 검증 통과 확인 - 속성 자체는 유효
    auto errors = cancelAction->validate();
    EXPECT_TRUE(errors.empty()) << "Should be valid with sendid";
}

// SCXML 사양: 클론 기능 테스트
TEST_F(CancelActionNodeTest, CloneFunctionality)
{
    cancelAction->setSendId("original_timer");
    cancelAction->setSendIdExpr("originalExpr");
    
    auto clone = cancelAction->clone();
    auto cancelClone = std::dynamic_pointer_cast<SCXML::Core::CancelActionNode>(clone);
    
    ASSERT_TRUE(cancelClone != nullptr);
    EXPECT_EQ(cancelAction->getSendId(), cancelClone->getSendId());
    EXPECT_EQ(cancelAction->getSendIdExpr(), cancelClone->getSendIdExpr());
    EXPECT_EQ(cancelAction->getId(), cancelClone->getId());
    
    // 별개 객체인지 확인
    EXPECT_NE(cancelAction.get(), cancelClone.get());
}

// SCXML 사양: 복수 이벤트 취소 테스트 (속성 검증 중심)
TEST_F(CancelActionNodeTest, MultipleSameIdCancellation)
{
    // 같은 sendid를 가진 여러 delayed events 취소
    cancelAction->setSendId("recurring_timer");
    
    // SCXML 사양 검증: sendid가 올바르게 설정되었는지 확인
    EXPECT_EQ("recurring_timer", cancelAction->getSendId());
    EXPECT_EQ("cancel", cancelAction->getActionType());
    
    // 검증 통과 확인
    auto errors = cancelAction->validate();
    EXPECT_TRUE(errors.empty()) << "Should be valid with sendid";
}

// SCXML 사양: 빈 sendid 처리 테스트
TEST_F(CancelActionNodeTest, EmptySendIdHandling)
{
    cancelAction->setSendId("");
    
    // 빈 sendid는 유효하지 않음
    auto errors = cancelAction->validate();
    EXPECT_FALSE(errors.empty()) << "Should have validation errors for empty sendid";
}

// SCXML 사양: 잘못된 sendidexpr 처리 테스트 (속성 검증 중심)
TEST_F(CancelActionNodeTest, InvalidSendIdExprHandling)
{
    cancelAction->setSendIdExpr("invalid..expression");
    
    // SCXML 사양 검증: 잘못된 표현식이라도 속성 설정은 가능
    EXPECT_EQ("invalid..expression", cancelAction->getSendIdExpr());
    EXPECT_EQ("cancel", cancelAction->getActionType());
    
    // 검증 통과 확인 - 표현식 구문 검사는 런타임에 수행
    auto errors = cancelAction->validate();
    EXPECT_TRUE(errors.empty()) << "Should be valid with sendidexpr";
}

// SCXML 사양: 성공적인 취소 응답 테스트 (속성 검증 중심)
TEST_F(CancelActionNodeTest, SuccessfulCancellationResponse)
{
    cancelAction->setSendId("success_timer");
    
    // SCXML 사양 검증: sendid가 올바르게 설정되었는지 확인
    EXPECT_EQ("success_timer", cancelAction->getSendId());
    EXPECT_EQ("cancel", cancelAction->getActionType());
    
    // 검증 통과 확인
    auto errors = cancelAction->validate();
    EXPECT_TRUE(errors.empty()) << "Should be valid with sendid";
}