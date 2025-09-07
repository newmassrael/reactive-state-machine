#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"

// Simplified Guard Node test - testing basic functionality without the problematic GuardNode class
#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"
#include "core/GuardNode.h"

// SCXML W3C 사양에 따른 Guard Node 테스트
class GuardNodeTest : public CoreTestBase
{
protected:
    void SetUp() override
    {
        CoreTestBase::SetUp();
        mockContext = std::make_shared<MockExecutionContext>();
        guardNode = std::make_shared<SCXML::Core::GuardNode>("guard1", "targetState");
    }

    std::shared_ptr<MockExecutionContext> mockContext;
    std::shared_ptr<SCXML::Core::GuardNode> guardNode;
};;

// SCXML 사양: Guard 기본 속성 테스트
TEST_F(GuardNodeTest, BasicGuardAttributes)
{
    // SCXML 사양: id 속성 (필수)
    EXPECT_EQ("guard1", guardNode->getId());
    
    // SCXML 사양: 타겟 상태 설정
    guardNode->setTargetState("targetState");
    EXPECT_EQ("targetState", guardNode->getTargetState());
    
    // SCXML 사양: 가드 조건 설정 (cond 속성)
    guardNode->setCondition("count > 0");
    EXPECT_EQ("count > 0", guardNode->getCondition());
    
    // 기본 reactive 상태 (false)
    EXPECT_FALSE(guardNode->isReactive());
}

// SCXML 사양: Guard 의존성 관리 테스트
TEST_F(GuardNodeTest, GuardDependencyManagement)
{
    // SCXML 사양: 의존성 추가
    guardNode->addDependency("data.counter");
    guardNode->addDependency("session.isActive");
    guardNode->addDependency("user.permissions");
    
    // 의존성 조회
    auto dependencies = guardNode->getDependencies();
    EXPECT_EQ(3, dependencies.size());
    
    // 의존성이 올바르게 저장되었는지 확인
    EXPECT_TRUE(std::find(dependencies.begin(), dependencies.end(), "data.counter") != dependencies.end());
    EXPECT_TRUE(std::find(dependencies.begin(), dependencies.end(), "session.isActive") != dependencies.end());
    EXPECT_TRUE(std::find(dependencies.begin(), dependencies.end(), "user.permissions") != dependencies.end());
}

// SCXML 사양: Guard 조건 표현식 테스트
TEST_F(GuardNodeTest, GuardConditionExpressions)
{
    // SCXML 사양: 단순 boolean 조건
    guardNode->setCondition("true");
    EXPECT_EQ("true", guardNode->getCondition());
    
    guardNode->setCondition("false");
    EXPECT_EQ("false", guardNode->getCondition());
    
    // SCXML 사양: 변수 비교 조건
    guardNode->setCondition("data.counter > 5");
    EXPECT_EQ("data.counter > 5", guardNode->getCondition());
    
    // SCXML 사양: 복합 조건 (AND, OR)
    guardNode->setCondition("(age >= 18) && (status == 'active')");
    EXPECT_EQ("(age >= 18) && (status == 'active')", guardNode->getCondition());
    
    // SCXML 사양: In() 함수 조건
    guardNode->setCondition("In('state1') || In('state2')");
    EXPECT_EQ("In('state1') || In('state2')", guardNode->getCondition());
}

// SCXML 사양: 외부 클래스 및 팩토리 설정 테스트
TEST_F(GuardNodeTest, ExternalClassAndFactoryHandling)
{
    // SCXML 사양: 외부 클래스 설정
    guardNode->setExternalClass("com.example.CustomGuard");
    EXPECT_EQ("com.example.CustomGuard", guardNode->getExternalClass());
    
    // SCXML 사양: 외부 팩토리 설정
    guardNode->setExternalFactory("customGuardFactory");
    EXPECT_EQ("customGuardFactory", guardNode->getExternalFactory());
    
    // 빈 값 설정 테스트
    guardNode->setExternalClass("");
    EXPECT_EQ("", guardNode->getExternalClass());
    
    guardNode->setExternalFactory("");
    EXPECT_EQ("", guardNode->getExternalFactory());
}

// SCXML 사양: Reactive 가드 기능 테스트
TEST_F(GuardNodeTest, ReactiveGuardFunctionality)
{
    // SCXML 사양: reactive 속성 설정
    EXPECT_FALSE(guardNode->isReactive()); // 기본값
    
    guardNode->setReactive(true);
    EXPECT_TRUE(guardNode->isReactive());
    
    guardNode->setReactive(false);
    EXPECT_FALSE(guardNode->isReactive());
    
    // reactive 가드는 데이터 변경 시 자동으로 재평가됨
    guardNode->setCondition("data.value > threshold");
    guardNode->addDependency("data.value");
    guardNode->addDependency("threshold");
    guardNode->setReactive(true);
    
    EXPECT_TRUE(guardNode->isReactive());
    EXPECT_EQ("data.value > threshold", guardNode->getCondition());
}

// SCXML 사양: Guard 속성 관리 테스트
TEST_F(GuardNodeTest, GuardAttributeManagement)
{
    // SCXML 사양: 커스텀 속성 설정
    guardNode->setAttribute("priority", "high");
    guardNode->setAttribute("timeout", "5000");
    guardNode->setAttribute("category", "security");
    
    // 속성 조회
    EXPECT_EQ("high", guardNode->getAttribute("priority"));
    EXPECT_EQ("5000", guardNode->getAttribute("timeout"));
    EXPECT_EQ("security", guardNode->getAttribute("category"));
    
    // 존재하지 않는 속성 조회
    EXPECT_EQ("", guardNode->getAttribute("nonexistent"));
    
    // 모든 속성 조회
    auto attributes = guardNode->getAttributes();
    EXPECT_EQ(3, attributes.size());
    EXPECT_TRUE(attributes.find("priority") != attributes.end());
    EXPECT_TRUE(attributes.find("timeout") != attributes.end());
    EXPECT_TRUE(attributes.find("category") != attributes.end());
}

// SCXML 사양: 복잡한 Boolean 표현식 테스트
TEST_F(GuardNodeTest, ComplexBooleanExpressions)
{
    // SCXML 사양: AND 조건
    guardNode->setCondition("(status == 'active') && (count > 0) && (user.authenticated)");
    EXPECT_EQ("(status == 'active') && (count > 0) && (user.authenticated)", guardNode->getCondition());
    
    // SCXML 사양: OR 조건
    guardNode->setCondition("(level == 'admin') || (permissions.includes('write'))");
    EXPECT_EQ("(level == 'admin') || (permissions.includes('write'))", guardNode->getCondition());
    
    // SCXML 사양: NOT 조건
    guardNode->setCondition("!(error.occurred) && !In('errorState')");
    EXPECT_EQ("!(error.occurred) && !In('errorState')", guardNode->getCondition());
    
    // SCXML 사양: 중첩된 복합 조건
    guardNode->setCondition("((a > b) || (c < d)) && !(e == f)");
    EXPECT_EQ("((a > b) || (c < d)) && !(e == f)", guardNode->getCondition());
}

// SCXML 사양: 데이터 모델 통합 테스트
TEST_F(GuardNodeTest, DataModelIntegration)
{
    // SCXML 사양: 데이터 모델 변수 접근
    guardNode->setCondition("_data.currentState == 'processing'");
    guardNode->addDependency("_data.currentState");
    
    EXPECT_EQ("_data.currentState == 'processing'", guardNode->getCondition());
    
    // SCXML 사양: 이벤트 데이터 접근
    guardNode->setCondition("_event.data.userId == _data.sessionId");
    guardNode->addDependency("_event.data");
    guardNode->addDependency("_data.sessionId");
    
    EXPECT_EQ("_event.data.userId == _data.sessionId", guardNode->getCondition());
}

// SCXML 사양: 에러 처리 테스트
TEST_F(GuardNodeTest, ErrorHandlingForInvalidExpressions)
{
    // SCXML 사양: 잘못된 구문도 속성으로는 저장 가능
    guardNode->setCondition("invalid..syntax((");
    EXPECT_EQ("invalid..syntax((", guardNode->getCondition());
    
    // 빈 조건 설정
    guardNode->setCondition("");
    EXPECT_EQ("", guardNode->getCondition());
    
    // null 값 같은 조건도 문자열로 처리
    guardNode->setCondition("null");
    EXPECT_EQ("null", guardNode->getCondition());
}

// SCXML 사양: 성능 테스트 (복잡한 조건)
TEST_F(GuardNodeTest, ComplexConditionPerformance)
{
    // SCXML 사양: 매우 복잡한 조건식
    std::string complexCondition = R"(
        (user.role == 'admin' && permissions.read && permissions.write) ||
        (user.role == 'moderator' && permissions.read && !session.expired) ||
        (user.role == 'user' && permissions.read && session.active && 
         user.lastLogin > (Date.now() - 86400000))
    )";
    
    guardNode->setCondition(complexCondition);
    EXPECT_EQ(complexCondition, guardNode->getCondition());
    
    // 관련 의존성들 추가
    guardNode->addDependency("user.role");
    guardNode->addDependency("permissions");
    guardNode->addDependency("session");
    guardNode->addDependency("user.lastLogin");
    
    // 의존성이 모두 올바르게 추가되었는지 확인
    auto dependencies = guardNode->getDependencies();
    EXPECT_EQ(4, dependencies.size());
}

// SCXML 사양: 동적 조건 평가 준비 테스트
TEST_F(GuardNodeTest, DynamicConditionEvaluationPreparation)
{
    // SCXML 사양: 런타임에 변경될 수 있는 조건 설정
    guardNode->setCondition("dynamicValue > threshold");
    guardNode->setTargetState("nextState");
    guardNode->setReactive(true);
    
    // 관련 데이터 의존성 설정
    guardNode->addDependency("dynamicValue");
    guardNode->addDependency("threshold");
    
    // 모든 설정이 올바른지 확인
    EXPECT_EQ("dynamicValue > threshold", guardNode->getCondition());
    EXPECT_EQ("nextState", guardNode->getTargetState());
    EXPECT_TRUE(guardNode->isReactive());
    
    auto dependencies = guardNode->getDependencies();
    EXPECT_EQ(2, dependencies.size());
}

// SCXML 사양: Guard 완전성 검증 테스트
TEST_F(GuardNodeTest, GuardCompleteness)
{
    // 완전한 Guard 설정
    guardNode->setTargetState("finalState");
    guardNode->setCondition("(status == 'ready') && (count > 0)");
    guardNode->setReactive(true);
    guardNode->setExternalClass("CustomGuardEvaluator");
    guardNode->setExternalFactory("guardFactory");
    
    // 의존성과 속성 추가
    guardNode->addDependency("status");
    guardNode->addDependency("count");
    guardNode->setAttribute("priority", "high");
    guardNode->setAttribute("description", "Main transition guard");
    
    // 모든 속성이 올바르게 설정되었는지 확인
    EXPECT_EQ("guard1", guardNode->getId());
    EXPECT_EQ("finalState", guardNode->getTargetState());
    EXPECT_EQ("(status == 'ready') && (count > 0)", guardNode->getCondition());
    EXPECT_TRUE(guardNode->isReactive());
    EXPECT_EQ("CustomGuardEvaluator", guardNode->getExternalClass());
    EXPECT_EQ("guardFactory", guardNode->getExternalFactory());
    EXPECT_EQ("high", guardNode->getAttribute("priority"));
    EXPECT_EQ("Main transition guard", guardNode->getAttribute("description"));
    
    auto dependencies = guardNode->getDependencies();
    EXPECT_EQ(2, dependencies.size());
}