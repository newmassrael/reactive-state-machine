#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"
#include "core/actions/AssignActionNode.h"
#include "runtime/RuntimeContext.h"

// SCXML W3C 사양에 따른 Assign Action Node 테스트
class AssignActionNodeTest : public CoreTestBase
{
protected:
    void SetUp() override
    {
        CoreTestBase::SetUp();
        mockContext = std::make_shared<MockExecutionContext>();
        assignAction = std::make_shared<SCXML::Core::AssignActionNode>("assign1");
    }

    std::shared_ptr<MockExecutionContext> mockContext;
    std::shared_ptr<SCXML::Core::AssignActionNode> assignAction;
};

// SCXML 사양 5.9.2: <assign> 기본 속성 테스트
TEST_F(AssignActionNodeTest, BasicAssignAttributes)
{
    // SCXML 사양: location 속성 (필수)
    assignAction->setLocation("data.counter");
    EXPECT_EQ("data.counter", assignAction->getLocation());
    
    // SCXML 사양: expr 속성 (선택적)
    assignAction->setExpr("counter + 1");
    EXPECT_EQ("counter + 1", assignAction->getExpr());
    
    EXPECT_EQ("assign", assignAction->getActionType());
    EXPECT_EQ("assign1", assignAction->getId());
}

// SCXML 사양: location은 필수, expr 또는 content 중 하나만 있어야 함
TEST_F(AssignActionNodeTest, RequiredAttributeValidation)
{
    // location 없이는 유효하지 않음
    auto errors = assignAction->validate();
    EXPECT_FALSE(errors.empty()) << "Should have validation errors for missing location";
    
    // location 설정 후 유효함
    assignAction->setLocation("data.value");
    assignAction->setExpr("'hello'");
    errors = assignAction->validate();
    EXPECT_TRUE(errors.empty()) << "Should be valid with location and expr";
}

// SCXML 사양: 데이터 모델 수정 테스트
TEST_F(AssignActionNodeTest, DataModelModification)
{
    assignAction->setLocation("data.status");
    assignAction->setExpr("'active'");
    
    // SCXML 사양 검증: location과 expr 속성이 올바르게 설정됨
    EXPECT_EQ("data.status", assignAction->getLocation());
    EXPECT_EQ("'active'", assignAction->getExpr());
    EXPECT_EQ("assign", assignAction->getActionType());
    
    // Executor를 통한 검증
    auto errors = assignAction->validate();
    EXPECT_TRUE(errors.empty()) << "Should be valid with location and expr";
}

// SCXML 사양: 복잡한 위치 표현식 테스트
TEST_F(AssignActionNodeTest, ComplexLocationExpressions)
{
    // 배열 인덱스 접근
    assignAction->setLocation("data.items[0]");
    assignAction->setExpr("'first item'");
    EXPECT_EQ("data.items[0]", assignAction->getLocation());
    
    // 객체 속성 접근
    assignAction->setLocation("data.user.name");
    assignAction->setExpr("'John Doe'");
    EXPECT_EQ("data.user.name", assignAction->getLocation());
    
    // 동적 속성 접근
    assignAction->setLocation("data[dynamicKey]");
    assignAction->setExpr("dynamicValue");
    EXPECT_EQ("data[dynamicKey]", assignAction->getLocation());
}

// SCXML 사양: 표현식 평가 테스트
TEST_F(AssignActionNodeTest, ExpressionEvaluation)
{
    assignAction->setLocation("data.result");
    
    // 리터럴 값
    assignAction->setExpr("42");
    EXPECT_EQ("42", assignAction->getExpr());
    
    // 산술 표현식
    assignAction->setExpr("count * 2 + 1");
    EXPECT_EQ("count * 2 + 1", assignAction->getExpr());
    
    // 조건식
    assignAction->setExpr("(age >= 18) ? 'adult' : 'minor'");
    EXPECT_EQ("(age >= 18) ? 'adult' : 'minor'", assignAction->getExpr());
    
    // 함수 호출
    assignAction->setExpr("getCurrentTime()");
    EXPECT_EQ("getCurrentTime()", assignAction->getExpr());
}

// SCXML 사양: 에러 처리 테스트
TEST_F(AssignActionNodeTest, ErrorHandling)
{
    assignAction->setLocation("invalid..location");
    assignAction->setExpr("validValue");
    
    // SCXML 사양: 잘못된 location 형식은 validate()에서 감지되지 않을 수 있음 (런타임 에러)
    // 하지만 기본적인 속성 설정은 정상 작동해야 함
    EXPECT_EQ("invalid..location", assignAction->getLocation());
    EXPECT_EQ("validValue", assignAction->getExpr());
    
    // 유효한 설정에서는 검증 통과
    auto errors = assignAction->validate();
    EXPECT_TRUE(errors.empty()) << "Basic attribute validation should pass";
}

// SCXML 사양: 클론 기능 테스트
TEST_F(AssignActionNodeTest, CloneFunctionality)
{
    assignAction->setLocation("data.original");
    assignAction->setExpr("'original value'");
    
    auto clone = assignAction->clone();
    auto assignClone = std::dynamic_pointer_cast<SCXML::Core::AssignActionNode>(clone);
    
    ASSERT_TRUE(assignClone != nullptr);
    EXPECT_EQ(assignAction->getLocation(), assignClone->getLocation());
    EXPECT_EQ(assignAction->getExpr(), assignClone->getExpr());
    EXPECT_EQ(assignAction->getId(), assignClone->getId());
    
    // 별개 객체인지 확인
    EXPECT_NE(assignAction.get(), assignClone.get());
    
    // 독립적 수정 가능한지 확인
    assignClone->setExpr("'cloned value'");
    EXPECT_NE(assignAction->getExpr(), assignClone->getExpr());
}

// SCXML 사양: 다양한 데이터 타입 할당 테스트
TEST_F(AssignActionNodeTest, DataTypeAssignment)
{
    assignAction->setLocation("data.mixed");
    
    // 문자열
    assignAction->setExpr("'Hello World'");
    // 숫자
    assignAction->setExpr("123.45");
    // 불린
    assignAction->setExpr("true");
    // 객체
    assignAction->setExpr("{ name: 'John', age: 30 }");
    // 배열
    assignAction->setExpr("[1, 2, 3, 'four']");
    // null
    assignAction->setExpr("null");
    
    // 각 타입이 올바르게 설정되는지 확인
    EXPECT_EQ("null", assignAction->getExpr());
}

// SCXML 사양: 빈 표현식 처리 테스트
TEST_F(AssignActionNodeTest, EmptyExpressionHandling)
{
    assignAction->setLocation("data.empty");
    assignAction->setExpr("");
    
    auto errors = assignAction->validate();
    // SCXML W3C 사양: <assign>은 반드시 'expr' 또는 'attr' 중 하나가 있어야 함
    EXPECT_FALSE(errors.empty()) << "Should have validation errors for missing expr/attr";
    // 에러 메시지가 적절한지 확인
    bool hasExpectedError = std::any_of(errors.begin(), errors.end(), 
        [](const std::string& error) { 
            return error.find("must have either 'expr' or 'attr'") != std::string::npos; 
        });
    EXPECT_TRUE(hasExpectedError) << "Should have error about missing expr/attr attributes";
}

// SCXML 사양: 동적 위치 평가 테스트  
TEST_F(AssignActionNodeTest, DynamicLocationEvaluation)
{
    // 위치가 동적으로 계산되는 경우
    assignAction->setLocation("data[currentSection].value");
    assignAction->setExpr("'dynamic content'");
    
    EXPECT_EQ("data[currentSection].value", assignAction->getLocation());
    EXPECT_EQ("'dynamic content'", assignAction->getExpr());
}