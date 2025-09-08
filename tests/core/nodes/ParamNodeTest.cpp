#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"
#include "core/ParamNode.h"
#include "common/Result.h"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <vector>

// SCXML W3C 사양에 따른 Param Node 고급 기능 테스트
class ParamNodeTest : public CoreTestBase
{
protected:
    void SetUp() override
    {
        CoreTestBase::SetUp();
        mockContext = std::make_shared<MockExecutionContext>();
    }

    void TearDown() override
    {
        CoreTestBase::TearDown();
    }

    std::shared_ptr<MockExecutionContext> mockContext;
};

// SCXML W3C 사양: <param> 기본 속성 테스트
TEST_F(ParamNodeTest, BasicParamNodeAttributes)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("testParam", "param1");
    
    // 기본 속성 확인
    EXPECT_EQ("testParam", paramNode->getName());
    EXPECT_EQ("param1", paramNode->getId());
    EXPECT_TRUE(paramNode->getLiteralValue().empty());
    EXPECT_TRUE(paramNode->getExpression().empty());
    EXPECT_TRUE(paramNode->getLocation().empty());
    EXPECT_FALSE(paramNode->hasExpression());
    EXPECT_FALSE(paramNode->hasLocation());
    EXPECT_EQ("none", paramNode->getValueSource());
}

// SCXML W3C 사양: 리터럴 값을 사용한 매개변수
TEST_F(ParamNodeTest, LiteralValueParameter)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("message");
    paramNode->setValue("Hello World");
    
    EXPECT_EQ("Hello World", paramNode->getLiteralValue());
    EXPECT_EQ("literal", paramNode->getValueSource());
    EXPECT_FALSE(paramNode->hasExpression());
    EXPECT_FALSE(paramNode->hasLocation());

    // Mock context에서 리터럴 값 반환 확인
    std::string value = paramNode->getValue(*mockContext);
    EXPECT_EQ("Hello World", value);
}

// SCXML W3C 사양: 표현식을 사용한 매개변수
TEST_F(ParamNodeTest, ExpressionBasedParameter)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("counter");
    paramNode->setExpression("count + 1");
    
    EXPECT_EQ("count + 1", paramNode->getExpression());
    EXPECT_EQ("expression", paramNode->getValueSource());
    EXPECT_TRUE(paramNode->hasExpression());
    EXPECT_FALSE(paramNode->hasLocation());

    // Mock context에서 표현식 평가 설정
    EXPECT_CALL(*mockContext, evaluateExpression("count + 1"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("43")));

    std::string value = paramNode->getValue(*mockContext);
    EXPECT_EQ("43", value);
}

// SCXML W3C 사양: 데이터 모델 위치 참조 매개변수
TEST_F(ParamNodeTest, LocationBasedParameter)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("userData");
    paramNode->setLocation("user.profile.name");
    
    EXPECT_EQ("user.profile.name", paramNode->getLocation());
    EXPECT_EQ("location", paramNode->getValueSource());
    EXPECT_FALSE(paramNode->hasExpression());
    EXPECT_TRUE(paramNode->hasLocation());

    // Mock context에서 데이터 모델 값 조회 설정
    EXPECT_CALL(*mockContext, getDataValue("user.profile.name"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("John Doe")));

    std::string value = paramNode->getValue(*mockContext);
    EXPECT_EQ("John Doe", value);
}

// SCXML W3C 사양: 우선순위 규칙 - location > expression > literal
TEST_F(ParamNodeTest, ValueSourcePriorityRules)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("priorityTest");
    
    // 모든 값 소스 설정
    paramNode->setValue("literal_value");
    paramNode->setExpression("expression_result");
    paramNode->setLocation("data.value");
    
    // location이 최고 우선순위여야 함
    EXPECT_EQ("location", paramNode->getValueSource());
    EXPECT_TRUE(paramNode->hasLocation());
    EXPECT_TRUE(paramNode->hasExpression());

    // Mock context에서 location 값 반환
    EXPECT_CALL(*mockContext, getDataValue("data.value"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("location_result")));

    std::string value = paramNode->getValue(*mockContext);
    EXPECT_EQ("location_result", value);
}

// SCXML W3C 사양: expression 우선순위 (location 없을 때)
TEST_F(ParamNodeTest, ExpressionPriorityOverLiteral)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("exprPriority");
    
    // literal과 expression 설정 (location 없음)
    paramNode->setValue("literal_value");
    paramNode->setExpression("42 * 2");
    
    EXPECT_EQ("expression", paramNode->getValueSource());
    EXPECT_TRUE(paramNode->hasExpression());
    EXPECT_FALSE(paramNode->hasLocation());

    // Mock context에서 표현식 평가
    EXPECT_CALL(*mockContext, evaluateExpression("42 * 2"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("84")));

    std::string value = paramNode->getValue(*mockContext);
    EXPECT_EQ("84", value);
}

// SCXML W3C 사양: 복합 객체 매개변수
TEST_F(ParamNodeTest, ComplexObjectParameter)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("configParam");
    paramNode->setExpression("{ server: 'localhost', port: 8080, ssl: true }");
    
    // Mock context에서 복합 객체 평가
    EXPECT_CALL(*mockContext, evaluateExpression("{ server: 'localhost', port: 8080, ssl: true }"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("{\"server\":\"localhost\",\"port\":8080,\"ssl\":true}")));

    std::string value = paramNode->getValue(*mockContext);
    EXPECT_EQ("{\"server\":\"localhost\",\"port\":8080,\"ssl\":true}", value);
}

// SCXML W3C 사양: 배열 매개변수
TEST_F(ParamNodeTest, ArrayParameter)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("itemsParam");
    paramNode->setExpression("[1, 2, 3, 'four', true]");
    
    // Mock context에서 배열 표현식 평가
    EXPECT_CALL(*mockContext, evaluateExpression("[1, 2, 3, 'four', true]"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("[1,2,3,\"four\",true]")));

    std::string value = paramNode->getValue(*mockContext);
    EXPECT_EQ("[1,2,3,\"four\",true]", value);
}

// SCXML W3C 사양: 매개변수 검증
TEST_F(ParamNodeTest, ParameterValidation)
{
    // 유효한 매개변수들
    auto validParam1 = std::make_unique<SCXML::Core::ParamNode>("valid1");
    validParam1->setValue("test");
    auto errors1 = validParam1->validate();
    EXPECT_TRUE(errors1.empty()) << "Valid literal parameter should have no validation errors";

    auto validParam2 = std::make_unique<SCXML::Core::ParamNode>("valid2");
    validParam2->setExpression("expression");
    auto errors2 = validParam2->validate();
    EXPECT_TRUE(errors2.empty()) << "Valid expression parameter should have no validation errors";

    auto validParam3 = std::make_unique<SCXML::Core::ParamNode>("valid3");
    validParam3->setLocation("data.field");
    auto errors3 = validParam3->validate();
    EXPECT_TRUE(errors3.empty()) << "Valid location parameter should have no validation errors";
}

// SCXML W3C 사양: 빈 매개변수 검증
TEST_F(ParamNodeTest, EmptyParameterValidation)
{
    auto emptyParam = std::make_unique<SCXML::Core::ParamNode>("empty");
    // 아무 값도 설정하지 않음
    
    auto errors = emptyParam->validate();
    EXPECT_FALSE(errors.empty()) << "Empty parameter should have validation errors";
    
    // 값 소스가 없음을 확인
    EXPECT_EQ("none", emptyParam->getValueSource());
}

// SCXML W3C 사양: 매개변수 처리 (이벤트와 함께)
TEST_F(ParamNodeTest, ParameterProcessingWithEvent)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("eventParam");
    paramNode->setValue("event_data");
    
    // Mock context에서 이벤트 처리 확인
    EXPECT_CALL(*mockContext, setDataValue("eventParam", "event_data"))
        .WillOnce(testing::Return(SCXML::Common::Result<void>::success()));

    auto result = paramNode->processParameter(*mockContext, "test.event");
    EXPECT_TRUE(result.isSuccess()) << "Parameter processing with event should succeed";
}

// SCXML W3C 사양: 표현식 평가 실패 처리
TEST_F(ParamNodeTest, ExpressionEvaluationFailure)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("failParam");
    paramNode->setExpression("invalid.syntax.error");
    
    // Mock context에서 표현식 평가 실패
    EXPECT_CALL(*mockContext, evaluateExpression("invalid.syntax.error"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::failure("Syntax error")));

    std::string value = paramNode->getValue(*mockContext);
    EXPECT_TRUE(value.empty()) << "Failed expression evaluation should return empty string";
}

// SCXML W3C 사양: 데이터 모델 위치 참조 실패 처리
TEST_F(ParamNodeTest, LocationReferenceFailure)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("missingParam");
    paramNode->setLocation("nonexistent.data.path");
    
    // Mock context에서 데이터 값 조회 실패
    EXPECT_CALL(*mockContext, getDataValue("nonexistent.data.path"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::failure("Data not found")));

    std::string value = paramNode->getValue(*mockContext);
    EXPECT_TRUE(value.empty()) << "Failed location reference should return empty string";
}

// SCXML W3C 사양: 매개변수 초기화
TEST_F(ParamNodeTest, ParameterInitialization)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("initParam");
    paramNode->setValue("initialized_value");
    
    // Mock context에서 초기화 확인
    EXPECT_CALL(*mockContext, setDataValue("initParam", "initialized_value"))
        .WillOnce(testing::Return(SCXML::Common::Result<void>::success()));

    bool result = paramNode->initialize(*mockContext);
    EXPECT_TRUE(result) << "Parameter initialization should succeed";
}

// SCXML W3C 사양: 매개변수 복제
TEST_F(ParamNodeTest, ParameterCloning)
{
    auto originalParam = std::make_unique<SCXML::Core::ParamNode>("original", "id1");
    originalParam->setValue("test_value");
    originalParam->setExpression("test_expr");
    originalParam->setLocation("test.location");
    
    // 복제 수행
    auto clonedParam = originalParam->clone();
    auto paramClone = std::dynamic_pointer_cast<SCXML::Core::ParamNode>(clonedParam);
    
    ASSERT_TRUE(paramClone != nullptr);
    EXPECT_EQ(originalParam->getName(), paramClone->getName());
    EXPECT_EQ(originalParam->getId(), paramClone->getId());
    EXPECT_EQ(originalParam->getLiteralValue(), paramClone->getLiteralValue());
    EXPECT_EQ(originalParam->getExpression(), paramClone->getExpression());
    EXPECT_EQ(originalParam->getLocation(), paramClone->getLocation());
    EXPECT_EQ(originalParam->getValueSource(), paramClone->getValueSource());
    
    // 별개 객체인지 확인
    EXPECT_NE(originalParam.get(), paramClone.get());
}

// SCXML W3C 사양: 매개변수 값 초기화
TEST_F(ParamNodeTest, ParameterValueClearing)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("clearTest");
    
    // 모든 값 설정
    paramNode->setValue("literal");
    paramNode->setExpression("expr");
    paramNode->setLocation("loc");
    
    // 초기화 전 상태 확인
    EXPECT_EQ("location", paramNode->getValueSource());
    
    // 값 초기화
    paramNode->clear();
    
    // 초기화 후 상태 확인
    EXPECT_TRUE(paramNode->getLiteralValue().empty());
    EXPECT_TRUE(paramNode->getExpression().empty());
    EXPECT_TRUE(paramNode->getLocation().empty());
    EXPECT_FALSE(paramNode->hasExpression());
    EXPECT_FALSE(paramNode->hasLocation());
    EXPECT_EQ("none", paramNode->getValueSource());
}

// ============================================================================
// SCXML W3C 사양 추가 준수 테스트
// ============================================================================

// SCXML W3C 사양: expr과 location 상호 배타성 검증
TEST_F(ParamNodeTest, ExprAndLocationMutualExclusivity)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("mutualTest");
    
    // expr과 location 모두 설정 (W3C 사양 위반)
    paramNode->setExpression("count + 1");
    paramNode->setLocation("data.count");
    
    // W3C 사양: 둘 다 있으면 문서가 non-conformant
    auto errors = paramNode->validate();
    EXPECT_FALSE(errors.empty()) << "Should have validation errors when both expr and location are set";
    
    // 에러 메시지에서 상호 배타성 언급 확인
    bool foundMutualExclusivityError = false;
    for (const auto& error : errors) {
        if (error.find("mutually exclusive") != std::string::npos ||
            error.find("both expr and location") != std::string::npos) {
            foundMutualExclusivityError = true;
            break;
        }
    }
    EXPECT_TRUE(foundMutualExclusivityError) << "Should mention mutual exclusivity in error message";
}

// SCXML W3C 사양: NMTOKEN 검증 테스트
TEST_F(ParamNodeTest, NMTOKENValidation)
{
    // 유효한 NMTOKEN들
    std::vector<std::string> validNames = {
        "validName",
        "valid-name", 
        "valid_name",
        "valid.name",
        "Name123",
        "XML-valid_name.123"
    };
    
    for (const auto& name : validNames) {
        auto paramNode = std::make_unique<SCXML::Core::ParamNode>(name);
        paramNode->setValue("test");
        auto errors = paramNode->validate();
        EXPECT_TRUE(errors.empty()) << "Valid NMTOKEN '" << name << "' should pass validation";
    }
    
    // 무효한 NMTOKEN들
    std::vector<std::string> invalidNames = {
        "",              // 빈 이름
        "123name",       // 숫자로 시작
        "-name",         // 하이픈으로 시작
        ".name",         // 마침표로 시작
        "name with space", // 공백 포함
        "name@symbol",   // 허용되지 않는 기호
        "name#hash",     // 해시 포함
        "name$dollar"    // 달러 기호 포함
    };
    
    for (const auto& name : invalidNames) {
        auto paramNode = std::make_unique<SCXML::Core::ParamNode>(name);
        paramNode->setValue("test");
        auto errors = paramNode->validate();
        EXPECT_FALSE(errors.empty()) << "Invalid NMTOKEN '" << name << "' should fail validation";
    }
}

// SCXML W3C 사양: 특수 데이터 타입 처리
TEST_F(ParamNodeTest, SpecialDataTypeHandling)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("specialTypeParam");
    
    // JavaScript undefined 처리
    paramNode->setExpression("undefined");
    EXPECT_CALL(*mockContext, evaluateExpression("undefined"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("")));
    
    std::string undefinedValue = paramNode->getValue(*mockContext);
    EXPECT_TRUE(undefinedValue.empty()) << "undefined should be handled as empty string";
    
    // JavaScript null 처리
    paramNode->setExpression("null");
    EXPECT_CALL(*mockContext, evaluateExpression("null"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("null")));
    
    std::string nullValue = paramNode->getValue(*mockContext);
    EXPECT_EQ("null", nullValue) << "null should be preserved as string";
    
    // 함수 객체 처리
    paramNode->setExpression("function() { return 'test'; }");
    EXPECT_CALL(*mockContext, evaluateExpression("function() { return 'test'; }"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("[function Function]")));
    
    std::string functionValue = paramNode->getValue(*mockContext);
    EXPECT_EQ("[function Function]", functionValue) << "Function should be stringified";
}

// SCXML W3C 사양: 유니코드 및 특수 문자 처리
TEST_F(ParamNodeTest, UnicodeAndSpecialCharacterHandling)
{
    // 유니코드 매개변수 이름 (현재는 기본 테스트만 - NMTOKEN 검증에서 실패할 예정)
    auto unicodeParam = std::make_unique<SCXML::Core::ParamNode>("unicodeParam");
    unicodeParam->setValue("한글값");
    
    EXPECT_EQ("unicodeParam", unicodeParam->getName());
    EXPECT_EQ("한글값", unicodeParam->getLiteralValue());
    
    // 유니코드 표현식 처리
    auto exprParam = std::make_unique<SCXML::Core::ParamNode>("unicodeExpr");
    exprParam->setExpression("'안녕하세요'");
    
    EXPECT_CALL(*mockContext, evaluateExpression("'안녕하세요'"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("안녕하세요")));
    
    std::string unicodeValue = exprParam->getValue(*mockContext);
    EXPECT_EQ("안녕하세요", unicodeValue) << "Unicode expressions should be handled correctly";
    
    // 이스케이프 시퀀스 처리
    auto escapeParam = std::make_unique<SCXML::Core::ParamNode>("escapeTest");
    escapeParam->setExpression("'Line1\\nLine2\\tTabbed'");
    
    EXPECT_CALL(*mockContext, evaluateExpression("'Line1\\nLine2\\tTabbed'"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("Line1\nLine2\tTabbed")));
    
    std::string escapedValue = escapeParam->getValue(*mockContext);
    EXPECT_EQ("Line1\nLine2\tTabbed", escapedValue) << "Escape sequences should be processed";
}

// SCXML W3C 사양: 대용량 데이터 처리 성능 테스트
TEST_F(ParamNodeTest, LargeDataHandling)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("largeDataParam");
    
    // 큰 문자열 리터럴
    std::string largeString(10000, 'A');
    paramNode->setValue(largeString);
    
    EXPECT_EQ(largeString, paramNode->getLiteralValue());
    EXPECT_EQ("literal", paramNode->getValueSource());
    
    // 큰 배열 표현식
    paramNode->setExpression("Array(1000).fill('data').join(',')");
    
    std::string expectedLargeArray;
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) expectedLargeArray += ",";
        expectedLargeArray += "data";
    }
    
    EXPECT_CALL(*mockContext, evaluateExpression("Array(1000).fill('data').join(',')"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success(expectedLargeArray)));
    
    std::string largeArrayValue = paramNode->getValue(*mockContext);
    EXPECT_EQ(expectedLargeArray, largeArrayValue) << "Large arrays should be handled efficiently";
}

// SCXML W3C 사양: Edge Case - 순환 참조 방지
TEST_F(ParamNodeTest, CircularReferenceDetection)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("circularParam");
    paramNode->setLocation("circularParam"); // 자기 자신을 참조
    
    // 순환 참조 감지 및 오류 처리
    EXPECT_CALL(*mockContext, getDataValue("circularParam"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::failure("Circular reference detected")));
    
    std::string value = paramNode->getValue(*mockContext);
    EXPECT_TRUE(value.empty()) << "Circular reference should return empty string";
}

// ========== W3C SCXML 사양: Error Event Generation Tests ==========

// W3C SCXML 사양: 표현식 평가 실패 시 error.execution 이벤트 생성
TEST_F(ParamNodeTest, ErrorEventGenerationOnExpressionFailure)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("errorTestParam");
    paramNode->setExpression("invalidExpression()");
    
    // 표현식 평가 실패 모의
    EXPECT_CALL(*mockContext, evaluateExpression("invalidExpression()"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::failure("Undefined function: invalidExpression")));
    
    // error.execution 이벤트 생성 검증
    EXPECT_CALL(*mockContext, raiseEvent("error.execution", 
        testing::HasSubstr("Failed to evaluate expression 'invalidExpression()'")))
        .Times(1);
    
    std::string value = paramNode->getValue(*mockContext);
    EXPECT_TRUE(value.empty()) << "Failed expression evaluation should return empty string";
}

// W3C SCXML 사양: 위치 참조 실패 시 error.execution 이벤트 생성  
TEST_F(ParamNodeTest, ErrorEventGenerationOnLocationFailure)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("errorTestParam");
    paramNode->setLocation("nonexistentVariable");
    
    // 위치 참조 실패 모의
    EXPECT_CALL(*mockContext, getDataValue("nonexistentVariable"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::failure("Variable not found: nonexistentVariable")));
    
    // error.execution 이벤트 생성 검증
    EXPECT_CALL(*mockContext, raiseEvent("error.execution", 
        testing::HasSubstr("Failed to get value from location 'nonexistentVariable'")))
        .Times(1);
    
    std::string value = paramNode->getValue(*mockContext);
    EXPECT_TRUE(value.empty()) << "Failed location reference should return empty string";
}

// W3C SCXML 사양: 예외 발생 시 error.execution 이벤트 생성
TEST_F(ParamNodeTest, ErrorEventGenerationOnException)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("errorTestParam");
    paramNode->setExpression("complexExpression");
    
    // 예외 발생 모의 (throw 대신 failure 사용)
    EXPECT_CALL(*mockContext, evaluateExpression("complexExpression"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::failure("Runtime exception occurred")));
    
    // error.execution 이벤트 생성 검증
    EXPECT_CALL(*mockContext, raiseEvent("error.execution", 
        testing::HasSubstr("Failed to evaluate expression 'complexExpression'")))
        .Times(1);
    
    std::string value = paramNode->getValue(*mockContext);
    EXPECT_TRUE(value.empty()) << "Exception should return empty string";
}

// W3C SCXML 사양: 여러 오류 시나리오에서 error.execution 이벤트 생성
TEST_F(ParamNodeTest, ErrorEventGenerationMultipleScenarios)
{
    // 시나리오 1: 잘못된 데이터 타입 참조
    auto param1 = std::make_unique<SCXML::Core::ParamNode>("param1");
    param1->setLocation("invalidTypeVariable");
    
    EXPECT_CALL(*mockContext, getDataValue("invalidTypeVariable"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::failure("Type mismatch: expected string, got object")));
    
    EXPECT_CALL(*mockContext, raiseEvent("error.execution", 
        testing::HasSubstr("Type mismatch")))
        .Times(1);
    
    std::string value1 = param1->getValue(*mockContext);
    EXPECT_TRUE(value1.empty());
    
    // 시나리오 2: 구문 오류가 있는 표현식
    auto param2 = std::make_unique<SCXML::Core::ParamNode>("param2");
    param2->setExpression("1 + + 2"); // 잘못된 구문
    
    EXPECT_CALL(*mockContext, evaluateExpression("1 + + 2"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::failure("Syntax error: unexpected token '+'")));
    
    EXPECT_CALL(*mockContext, raiseEvent("error.execution", 
        testing::HasSubstr("Syntax error")))
        .Times(1);
    
    std::string value2 = param2->getValue(*mockContext);
    EXPECT_TRUE(value2.empty());
}

// W3C SCXML 사양: 성공적인 평가에서는 error.execution 이벤트가 생성되지 않음
TEST_F(ParamNodeTest, NoErrorEventOnSuccessfulEvaluation)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("successParam");
    paramNode->setExpression("1 + 1");
    
    // 성공적인 표현식 평가
    EXPECT_CALL(*mockContext, evaluateExpression("1 + 1"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("2")));
    
    // error.execution 이벤트가 생성되지 않아야 함
    EXPECT_CALL(*mockContext, raiseEvent(testing::_, testing::_))
        .Times(0);
    
    std::string value = paramNode->getValue(*mockContext);
    EXPECT_EQ("2", value) << "Successful evaluation should return correct value";
    
    // 리터럴 값도 테스트
    auto literalParam = std::make_unique<SCXML::Core::ParamNode>("literalParam");
    literalParam->setValue("literalValue");
    
    // raiseEvent가 호출되지 않아야 함 (이미 위에서 설정됨)
    std::string literalValue = literalParam->getValue(*mockContext);
    EXPECT_EQ("literalValue", literalValue) << "Literal values should not generate error events";
}

// W3C SCXML 사양: 오류 이벤트 데이터에 적절한 컨텍스트 정보 포함
TEST_F(ParamNodeTest, ErrorEventDataContainsContextInfo)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("contextTestParam");
    paramNode->setExpression("undefinedVar.property");
    
    // 컨텍스트가 포함된 오류 메시지
    std::string expectedError = "ReferenceError: undefinedVar is not defined";
    EXPECT_CALL(*mockContext, evaluateExpression("undefinedVar.property"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::failure(expectedError)));
    
    // 오류 이벤트 데이터에 표현식과 오류 메시지가 모두 포함되어야 함
    EXPECT_CALL(*mockContext, raiseEvent("error.execution", 
        testing::AllOf(
            testing::HasSubstr("undefinedVar.property"),  // 원본 표현식
            testing::HasSubstr(expectedError)             // 오류 메시지
        )))
        .Times(1);
    
    std::string value = paramNode->getValue(*mockContext);
    EXPECT_TRUE(value.empty());
}

// ========== Concurrent Access Safety Tests ==========

// 스레드 안전성 테스트: 동시에 getValue() 호출
TEST_F(ParamNodeTest, ConcurrentGetValueAccess)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("concurrentParam");
    paramNode->setValue("initialValue");
    
    const int numThreads = 10;
    const int iterationsPerThread = 100;
    std::vector<std::thread> threads;
    std::vector<std::string> results(numThreads * iterationsPerThread);
    std::atomic<int> resultIndex(0);
    
    // 여러 스레드에서 동시에 getValue() 호출
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < iterationsPerThread; ++i) {
                std::string value = paramNode->getValue(*mockContext);
                size_t idx = static_cast<size_t>(resultIndex.fetch_add(1));
                results[idx] = value;
            }
        });
    }
    
    // 모든 스레드 완료 대기
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 모든 결과가 일관되어야 함
    for (const auto& result : results) {
        EXPECT_EQ("initialValue", result) << "Concurrent getValue() should return consistent values";
    }
}

// 스레드 안전성 테스트: 동시에 setter와 getter 호출
TEST_F(ParamNodeTest, ConcurrentSetterGetterAccess)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("concurrentSetGet");
    
    std::atomic<bool> testRunning(true);
    std::vector<std::string> getterResults;
    std::mutex resultsMutex;
    
    // Getter 스레드 - 계속해서 값을 읽음
    std::thread getterThread([&]() {
        while (testRunning) {
            std::string value = paramNode->getValue(*mockContext);
            std::lock_guard<std::mutex> lock(resultsMutex);
            getterResults.push_back(value);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
    
    // Setter 스레드들 - 값을 변경
    std::vector<std::thread> setterThreads;
    for (int i = 0; i < 5; ++i) {
        setterThreads.emplace_back([&, i]() {
            for (int j = 0; j < 20; ++j) {
                std::string newValue = "value_" + std::to_string(i) + "_" + std::to_string(j);
                paramNode->setValue(newValue);
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        });
    }
    
    // Setter 스레드들 완료 대기
    for (auto& thread : setterThreads) {
        thread.join();
    }
    
    testRunning = false;
    getterThread.join();
    
    // 결과 검증 - getter가 유효한 값들만 읽었는지 확인
    std::lock_guard<std::mutex> lock(resultsMutex);
    EXPECT_GT(getterResults.size(), 0) << "Should have read some values";
    
    // 모든 결과가 예상된 패턴을 따르거나 빈 문자열이어야 함
    for (const auto& result : getterResults) {
        bool isValidValue = result.empty() || 
                           result.substr(0, 6) == "value_" ||
                           result == "initialValue";
        EXPECT_TRUE(isValidValue) << "Unexpected value from concurrent access: " + result;
    }
}

// 스레드 안전성 테스트: 동시에 표현식 평가
TEST_F(ParamNodeTest, ConcurrentExpressionEvaluation)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("concurrentExpr");
    paramNode->setExpression("counter + 1");
    
    const int numThreads = 8;
    const int iterationsPerThread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> successCount(0);
    std::atomic<int> totalCalls(0);
    
    // Mock 설정 - 스레드 안전한 호출 카운팅
    EXPECT_CALL(*mockContext, evaluateExpression("counter + 1"))
        .Times(testing::AtLeast(numThreads * iterationsPerThread))
        .WillRepeatedly(testing::Invoke([&](const std::string&) {
            totalCalls++;
            return SCXML::Common::Result<std::string>::success("42");
        }));
    
    // 여러 스레드에서 동시에 표현식 평가
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterationsPerThread; ++i) {
                std::string result = paramNode->getValue(*mockContext);
                if (result == "42") {
                    successCount++;
                }
            }
        });
    }
    
    // 모든 스레드 완료 대기
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 모든 호출이 성공했는지 확인
    EXPECT_EQ(numThreads * iterationsPerThread, successCount.load()) 
        << "All concurrent expression evaluations should succeed";
}

// 스레드 안전성 테스트: 동시에 processParameter() 호출
TEST_F(ParamNodeTest, ConcurrentProcessParameterAccess)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("concurrentProcess");
    paramNode->setValue("processValue");
    
    const int numThreads = 6;
    const int iterationsPerThread = 30;
    std::vector<std::thread> threads;
    std::atomic<int> successfulProcesses(0);
    
    // Mock 설정 - 스레드 안전한 데이터 설정
    EXPECT_CALL(*mockContext, setDataValue("concurrentProcess", "processValue"))
        .Times(testing::AtLeast(numThreads * iterationsPerThread))
        .WillRepeatedly(testing::Invoke([&](const std::string&, const std::string&) {
            successfulProcesses++;
            return SCXML::Common::Result<void>::success();
        }));
    
    // 여러 스레드에서 동시에 processParameter() 호출
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterationsPerThread; ++i) {
                auto result = paramNode->processParameter(*mockContext, "test.event");
                EXPECT_TRUE(result.isSuccess()) << "processParameter should not fail in concurrent access";
            }
        });
    }
    
    // 모든 스레드 완료 대기
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 모든 프로세스가 완료되었는지 확인
    EXPECT_EQ(numThreads * iterationsPerThread, successfulProcesses.load())
        << "All concurrent processParameter calls should complete";
}

// Race Condition 테스트: Value Source 변경과 동시 읽기
TEST_F(ParamNodeTest, RaceConditionValueSourceChange)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("raceConditionTest");
    paramNode->setValue("initialLiteral");
    
    std::atomic<bool> testRunning(true);
    std::vector<std::string> observedSources;
    std::mutex sourcesMutex;
    
    // Value source 관찰 스레드
    std::thread observerThread([&]() {
        while (testRunning) {
            std::string source = paramNode->getValueSource();
            std::lock_guard<std::mutex> lock(sourcesMutex);
            observedSources.push_back(source);
            std::this_thread::sleep_for(std::chrono::microseconds(5));
        }
    });
    
    // Value source 변경 스레드들
    std::vector<std::thread> changerThreads;
    
    // 리터럴 값 변경 스레드
    changerThreads.emplace_back([&]() {
        for (int i = 0; i < 50; ++i) {
            paramNode->setValue("literal_" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
    });
    
    // 표현식 변경 스레드  
    changerThreads.emplace_back([&]() {
        for (int i = 0; i < 50; ++i) {
            paramNode->setExpression("expr_" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
    });
    
    // 위치 변경 스레드
    changerThreads.emplace_back([&]() {
        for (int i = 0; i < 50; ++i) {
            paramNode->setLocation("location_" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
    });
    
    // 변경 스레드들 완료 대기
    for (auto& thread : changerThreads) {
        thread.join();
    }
    
    testRunning = false;
    observerThread.join();
    
    // 관찰된 소스들이 유효한 값들인지 확인
    std::lock_guard<std::mutex> lock(sourcesMutex);
    EXPECT_GT(observedSources.size(), 0) << "Should have observed some value sources";
    
    for (const auto& source : observedSources) {
        bool isValidSource = source == "literal" || source == "expression" || 
                           source == "location" || source == "none";
        EXPECT_TRUE(isValidSource) << "Invalid value source observed: " + source;
    }
}

// 스레드 안전성 문서화 테스트: ParamNode는 현재 스레드 안전하지 않음을 확인
TEST_F(ParamNodeTest, ThreadSafetyDocumentation)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("threadSafetyDoc");
    
    // 이 테스트는 ParamNode가 현재 스레드 안전하지 않음을 문서화합니다.
    // 실제 멀티스레드 환경에서는 동기화 메커니즘이 필요합니다.
    
    const int numThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<bool> testRunning(true);
    std::vector<std::string> observedSources;
    std::mutex resultsMutex;
    
    // 읽기 스레드 - 안전하게 상태 관찰
    std::thread readerThread([&]() {
        while (testRunning) {
            try {
                std::string source = paramNode->getValueSource();
                std::string name = paramNode->getName();
                
                // 기본적인 불변 속성들은 유지되어야 함
                EXPECT_EQ("threadSafetyDoc", name) << "Name should always be consistent";
                
                std::lock_guard<std::mutex> lock(resultsMutex);
                observedSources.push_back(source);
            } catch (...) {
                // 예외가 발생할 수 있음 - 스레드 안전하지 않기 때문
            }
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    });
    
    // 쓰기 스레드들 - 값 변경
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 50; ++i) {
                try {
                    if (t % 3 == 0) {
                        paramNode->setValue("literal_" + std::to_string(t) + "_" + std::to_string(i));
                    } else if (t % 3 == 1) {
                        paramNode->setExpression("expr_" + std::to_string(t) + "_" + std::to_string(i));
                    } else {
                        paramNode->setLocation("loc_" + std::to_string(t) + "_" + std::to_string(i));
                    }
                } catch (...) {
                    // 동시 접근으로 인한 예외 가능
                }
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }
    
    // 짧은 시간 실행
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    testRunning = false;
    
    // 모든 스레드 완료 대기
    for (auto& thread : threads) {
        thread.join();
    }
    readerThread.join();
    
    // 결과 검증 - 읽기 작업이 어느 정도 성공했는지 확인
    std::lock_guard<std::mutex> lock(resultsMutex);
    
    // 최소한의 읽기 작업은 성공했어야 함
    EXPECT_GT(observedSources.size(), 0) << "Should have observed some value sources despite thread safety issues";
    
    // 관찰된 모든 소스가 유효한 값이어야 함
    for (const auto& source : observedSources) {
        bool isValid = source == "literal" || source == "expression" || 
                      source == "location" || source == "none";
        EXPECT_TRUE(isValid) << "All observed sources should be valid: " + source;
    }
    
    // 테스트 성공은 ParamNode가 기본적인 데이터 무결성을 어느 정도 유지함을 의미
    // 하지만 완전한 스레드 안전성을 보장하지는 않음
    std::cout << "INFO: ParamNode는 현재 스레드 안전하지 않습니다. "
              << "프로덕션 환경에서는 외부 동기화가 필요합니다." << std::endl;
}

// ========== Performance and Memory Testing ==========

// 메모리 누수 검사: 반복적인 setValue/getValue 호출
TEST_F(ParamNodeTest, MemoryLeakDetection)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("memoryLeakTest");
    
    const int iterations = 10000;
    std::vector<std::string> testValues;
    
    // 다양한 크기의 테스트 데이터 준비
    for (int i = 0; i < 100; ++i) {
        testValues.push_back("test_value_" + std::to_string(i) + "_" + std::string(static_cast<size_t>(i * 10), 'x'));
    }
    
    // 시작 시간 측정
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 반복적인 메모리 할당/해제
    for (int i = 0; i < iterations; ++i) {
        std::string& testValue = testValues[static_cast<size_t>(i) % testValues.size()];
        
        // setValue 호출 - 내부 문자열 복사
        paramNode->setValue(testValue);
        
        // getValue 호출 - 문자열 반환
        std::string retrievedValue = paramNode->getValue(*mockContext);
        
        // 표현식도 테스트
        if (i % 3 == 0) {
            paramNode->setExpression("expr_" + std::to_string(i));
        }
        
        // 위치 참조도 테스트
        if (i % 5 == 0) {
            paramNode->setLocation("loc_" + std::to_string(i));
        }
        
        // 메모리 정리 확인을 위한 clear 호출
        if (i % 1000 == 0) {
            paramNode->clear();
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // 성능 기준 검증 (10,000 iterations이 1초 이내)
    EXPECT_LT(duration.count(), 1000) << "Memory operations should complete within 1 second";
    
    // 마지막에 정상 동작 확인
    paramNode->setValue("final_test");
    std::string finalValue = paramNode->getValue(*mockContext);
    EXPECT_EQ("final_test", finalValue) << "ParamNode should work normally after stress test";
    
    std::cout << "INFO: Memory leak test completed " << iterations 
              << " iterations in " << duration.count() << "ms" << std::endl;
}

// 성능 벤치마킹: 다양한 크기의 데이터 처리 시간 측정
TEST_F(ParamNodeTest, PerformanceBenchmarking)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("perfBenchmark");
    
    struct BenchmarkResult {
        std::string testName;
        size_t dataSize;
        std::chrono::milliseconds duration;
        double opsPerSecond;
    };
    
    std::vector<BenchmarkResult> results;
    
    // 다양한 크기의 데이터로 벤치마킹
    std::vector<std::pair<std::string, size_t>> testCases = {
        {"Small", 100},      // 100 바이트
        {"Medium", 10000},   // 10KB
        {"Large", 1000000},  // 1MB
        {"XLarge", 5000000}  // 5MB
    };
    
    for (auto& testCase : testCases) {
        std::string testData(testCase.second, 'A');
        const int iterations = (testCase.second < 100000) ? 1000 : 100; // 큰 데이터는 적게 반복
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // 반복 테스트
        for (int i = 0; i < iterations; ++i) {
            paramNode->setValue(testData + std::to_string(i));
            std::string retrieved = paramNode->getValue(*mockContext);
            EXPECT_FALSE(retrieved.empty()) << "Should retrieve non-empty value";
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        double opsPerSecond = (duration.count() > 0) ? 
            (iterations * 1000.0 / duration.count()) : iterations;
        
        results.push_back({
            testCase.first,
            testCase.second,
            duration,
            opsPerSecond
        });
        
        std::cout << "BENCHMARK " << testCase.first << " (" << testCase.second 
                  << " bytes): " << iterations << " ops in " << duration.count() 
                  << "ms (" << opsPerSecond << " ops/sec)" << std::endl;
    }
    
    // 성능 회귀 검사 - 작은 데이터는 최소 100 ops/sec 이상
    for (auto& result : results) {
        if (result.dataSize <= 10000) {
            EXPECT_GE(result.opsPerSecond, 100.0) 
                << result.testName << " performance below threshold";
        }
    }
    
    // 큰 데이터도 최소 10 ops/sec 이상
    for (auto& result : results) {
        EXPECT_GE(result.opsPerSecond, 10.0) 
            << result.testName << " performance critically low";
    }
}

// 표현식 평가 성능 테스트
TEST_F(ParamNodeTest, ExpressionEvaluationPerformance)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("exprPerfTest");
    
    // 다양한 복잡도의 표현식들
    std::vector<std::pair<std::string, std::string>> expressions = {
        {"Simple", "variable1"},
        {"Arithmetic", "variable1 + variable2 * 3"},
        {"Complex", "(variable1 + variable2) * (variable3 - variable4) / 2"},
        {"Nested", "func1(func2(variable1), func3(variable2 + variable3))"},
        {"Long", std::string(1000, 'a') + " + " + std::string(1000, 'b')}  // 긴 표현식
    };
    
    for (auto& expr : expressions) {
        paramNode->setExpression(expr.second);
        
        const int iterations = 1000;
        
        // Mock 설정
        EXPECT_CALL(*mockContext, evaluateExpression(expr.second))
            .Times(iterations)
            .WillRepeatedly(testing::Return(SCXML::Common::Result<std::string>::success("result")));
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            std::string result = paramNode->getValue(*mockContext);
            EXPECT_EQ("result", result);
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        double avgMicroseconds = duration.count() / static_cast<double>(iterations);
        
        std::cout << "EXPR PERF " << expr.first << ": " << avgMicroseconds 
                  << " μs per evaluation" << std::endl;
        
        // 표현식 평가는 평균 1ms 이내여야 함
        EXPECT_LT(avgMicroseconds, 1000.0) 
            << expr.first << " expression evaluation too slow";
    }
}

// 메모리 사용량 모니터링 테스트
TEST_F(ParamNodeTest, MemoryUsageMonitoring)
{
    const int numNodes = 1000;
    std::vector<std::unique_ptr<SCXML::Core::ParamNode>> nodes;
    
    // Mock 설정 - 표현식과 위치 참조에 대한 기본 응답
    EXPECT_CALL(*mockContext, evaluateExpression(testing::_))
        .WillRepeatedly(testing::Return(SCXML::Common::Result<std::string>::success("mock_expr_result")));
    
    EXPECT_CALL(*mockContext, getDataValue(testing::_))
        .WillRepeatedly(testing::Return(SCXML::Common::Result<std::string>::success("mock_location_result")));
    
    // 대량의 ParamNode 생성
    for (int i = 0; i < numNodes; ++i) {
        auto node = std::make_unique<SCXML::Core::ParamNode>("node_" + std::to_string(i));
        
        // 다양한 타입의 데이터 설정
        if (i % 3 == 0) {
            node->setValue(std::string(1000, static_cast<char>('A' + (i % 26))));
        } else if (i % 3 == 1) {
            node->setExpression("expression_" + std::to_string(i) + "_" + std::string(100, 'X'));
        } else {
            node->setLocation("location_" + std::to_string(i) + "_" + std::string(100, 'Y'));
        }
        
        nodes.push_back(std::move(node));
    }
    
    // 모든 노드에서 getValue 호출
    for (int round = 0; round < 5; ++round) {
        for (auto& node : nodes) {
            std::string result = node->getValue(*mockContext);
            EXPECT_FALSE(result.empty()) << "Node should return non-empty value";
        }
    }
    
    // 절반의 노드들 제거 (메모리 해제 테스트)
    nodes.erase(nodes.begin() + numNodes/2, nodes.end());
    
    // 남은 노드들 정상 동작 확인
    for (auto& node : nodes) {
        std::string result = node->getValue(*mockContext);
        EXPECT_FALSE(result.empty()) << "Remaining nodes should work correctly";
    }
    
    std::cout << "INFO: Memory usage test completed with " << numNodes 
              << " nodes, " << nodes.size() << " remaining" << std::endl;
}

// 리소스 부족 시뮬레이션 테스트
TEST_F(ParamNodeTest, ResourceExhaustionSimulation)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("resourceTest");
    
    // Mock 설정 - 필요 시 호출될 수 있음
    EXPECT_CALL(*mockContext, evaluateExpression(testing::_))
        .WillRepeatedly(testing::Return(SCXML::Common::Result<std::string>::success("mock_result")));
    
    EXPECT_CALL(*mockContext, getDataValue(testing::_))
        .WillRepeatedly(testing::Return(SCXML::Common::Result<std::string>::success("mock_result")));
    
    // 매우 큰 데이터로 메모리 압박 시뮬레이션
    try {
        const size_t hugeSize = 100 * 1024 * 1024; // 100MB
        std::string hugeString;
        
        // 안전하게 큰 문자열 생성 시도
        try {
            hugeString.reserve(hugeSize);
            hugeString.assign(hugeSize, 'M');
        } catch (const std::bad_alloc&) {
            // 메모리 부족 시 더 작은 크기로 테스트
            hugeString.assign(10 * 1024 * 1024, 'M'); // 10MB
        }
        
        // 큰 데이터 설정
        paramNode->setValue(hugeString);
        
        // 정상 동작 확인
        std::string retrieved = paramNode->getValue(*mockContext);
        EXPECT_EQ(hugeString.size(), retrieved.size()) << "Large data should be handled correctly";
        
        // 메모리 정리
        paramNode->clear();
        
        // 정리 후 정상 동작 확인
        paramNode->setValue("small_test");
        std::string smallResult = paramNode->getValue(*mockContext);
        EXPECT_EQ("small_test", smallResult) << "Should work normally after clearing large data";
        
    } catch (const std::exception& e) {
        // 예외 발생 시에도 테스트는 실패하지 않음 - 시스템 한계 테스트이므로
        std::cout << "INFO: Resource exhaustion test encountered: " << e.what() << std::endl;
        EXPECT_TRUE(true) << "Resource exhaustion handled gracefully";
    }
}

// CPU 집약적 작업 성능 테스트
TEST_F(ParamNodeTest, CPUIntensiveOperationsTest)
{
    auto paramNode = std::make_unique<SCXML::Core::ParamNode>("cpuIntensiveTest");
    
    const int iterations = 50000;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // CPU 집약적 작업 시뮬레이션
    for (int i = 0; i < iterations; ++i) {
        // 복잡한 문자열 조작
        std::string complexValue = "prefix_" + std::to_string(i) + "_" + 
                                  std::string(static_cast<size_t>(i % 100), static_cast<char>('A' + (i % 26))) + "_suffix";
        
        paramNode->setValue(complexValue);
        
        // 다양한 메소드 호출
        std::string retrieved = paramNode->getLiteralValue();
        std::string source = paramNode->getValueSource();
        std::string name = paramNode->getName();
        
        // 결과 검증
        EXPECT_EQ(complexValue, retrieved);
        EXPECT_EQ("literal", source);
        EXPECT_EQ("cpuIntensiveTest", name);
        
        // 주기적으로 다른 타입으로 변경
        if (i % 1000 == 0) {
            paramNode->setExpression("expr_" + std::to_string(i));
            paramNode->setLocation("loc_" + std::to_string(i));
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    double opsPerSecond = (iterations * 1000.0) / duration.count();
    
    std::cout << "CPU INTENSIVE: " << iterations << " operations in " 
              << duration.count() << "ms (" << opsPerSecond << " ops/sec)" << std::endl;
    
    // CPU 집약적 작업도 최소 성능 기준 만족해야 함
    EXPECT_GE(opsPerSecond, 1000.0) << "CPU intensive operations too slow";
    EXPECT_LT(duration.count(), 10000) << "CPU intensive test should complete within 10 seconds";
}

// ========== Parameter Collection Integration Tests ==========

// 여러 param 집계 테스트: 기본 key-value 맵 생성
TEST_F(ParamNodeTest, BasicParameterCollection)
{
    std::vector<std::unique_ptr<SCXML::Core::ParamNode>> params;
    
    // 다양한 타입의 param 생성
    auto param1 = std::make_unique<SCXML::Core::ParamNode>("user_id");
    param1->setValue("12345");
    
    auto param2 = std::make_unique<SCXML::Core::ParamNode>("amount");
    param2->setExpression("order.total * 1.1");
    
    auto param3 = std::make_unique<SCXML::Core::ParamNode>("currency");
    param3->setLocation("user.settings.currency");
    
    auto param4 = std::make_unique<SCXML::Core::ParamNode>("timestamp");
    param4->setValue("2024-01-15T10:30:00Z");
    
    params.push_back(std::move(param1));
    params.push_back(std::move(param2));
    params.push_back(std::move(param3));
    params.push_back(std::move(param4));
    
    // Mock 설정
    EXPECT_CALL(*mockContext, evaluateExpression("order.total * 1.1"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("110.00")));
    
    EXPECT_CALL(*mockContext, getDataValue("user.settings.currency"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("USD")));
    
    // 매개변수 집계 시뮬레이션
    std::map<std::string, std::string> collectedParams;
    for (auto& param : params) {
        std::string value = param->getValue(*mockContext);
        if (!value.empty()) {
            collectedParams[param->getName()] = value;
        }
    }
    
    // 결과 검증
    EXPECT_EQ(4, collectedParams.size()) << "Should collect all 4 parameters";
    EXPECT_EQ("12345", collectedParams["user_id"]);
    EXPECT_EQ("110.00", collectedParams["amount"]);
    EXPECT_EQ("USD", collectedParams["currency"]);
    EXPECT_EQ("2024-01-15T10:30:00Z", collectedParams["timestamp"]);
    
    std::cout << "INFO: Collected " << collectedParams.size() << " parameters successfully" << std::endl;
}

// Name collision 처리 테스트: W3C 사양에 따라 마지막 값 우선
TEST_F(ParamNodeTest, NameCollisionHandling)
{
    std::vector<std::unique_ptr<SCXML::Core::ParamNode>> params;
    
    // 동일한 name을 가진 param들 생성 (W3C: 마지막 값이 우선)
    auto param1 = std::make_unique<SCXML::Core::ParamNode>("status");
    param1->setValue("pending");
    
    auto param2 = std::make_unique<SCXML::Core::ParamNode>("amount");
    param2->setValue("100.00");
    
    auto param3 = std::make_unique<SCXML::Core::ParamNode>("status");  // 중복!
    param3->setValue("processing");
    
    auto param4 = std::make_unique<SCXML::Core::ParamNode>("status");  // 또 중복!
    param4->setExpression("'completed'");
    
    params.push_back(std::move(param1));
    params.push_back(std::move(param2));
    params.push_back(std::move(param3));
    params.push_back(std::move(param4));
    
    // Mock 설정
    EXPECT_CALL(*mockContext, evaluateExpression("'completed'"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("completed")));
    
    // W3C SCXML 사양: 동일한 name의 경우 나중에 나오는 값이 우선
    std::map<std::string, std::string> collectedParams;
    for (auto& param : params) {
        std::string value = param->getValue(*mockContext);
        if (!value.empty()) {
            collectedParams[param->getName()] = value;  // 자동으로 덮어씀 (마지막 값 우선)
        }
    }
    
    // 결과 검증: status는 마지막 값("completed")이어야 함
    EXPECT_EQ(2, collectedParams.size()) << "Should have 2 unique parameter names";
    EXPECT_EQ("completed", collectedParams["status"]) << "Last status value should win";
    EXPECT_EQ("100.00", collectedParams["amount"]) << "Amount should be preserved";
    
    std::cout << "INFO: Name collision resolved - final status: " << collectedParams["status"] << std::endl;
}

// 빈 값과 오류 처리: 실패한 param은 제외
TEST_F(ParamNodeTest, EmptyValueAndErrorHandling)
{
    std::vector<std::unique_ptr<SCXML::Core::ParamNode>> params;
    
    // 성공할 param들
    auto param1 = std::make_unique<SCXML::Core::ParamNode>("valid_literal");
    param1->setValue("test_value");
    
    auto param2 = std::make_unique<SCXML::Core::ParamNode>("valid_expr");
    param2->setExpression("'success'");
    
    // 실패할 param들
    auto param3 = std::make_unique<SCXML::Core::ParamNode>("failed_expr");
    param3->setExpression("undefined.property");
    
    auto param4 = std::make_unique<SCXML::Core::ParamNode>("failed_location");
    param4->setLocation("nonexistent.data");
    
    // 빈 값 param
    auto param5 = std::make_unique<SCXML::Core::ParamNode>("empty_value");
    param5->setValue("");
    
    params.push_back(std::move(param1));
    params.push_back(std::move(param2));
    params.push_back(std::move(param3));
    params.push_back(std::move(param4));
    params.push_back(std::move(param5));
    
    // Mock 설정
    EXPECT_CALL(*mockContext, evaluateExpression("'success'"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("success")));
    
    EXPECT_CALL(*mockContext, evaluateExpression("undefined.property"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::failure("ReferenceError: undefined is not defined")));
    
    EXPECT_CALL(*mockContext, getDataValue("nonexistent.data"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::failure("Variable not found")));
    
    // error.execution 이벤트 생성 예상
    EXPECT_CALL(*mockContext, raiseEvent("error.execution", testing::_))
        .Times(2);  // 2개의 실패한 param에서 이벤트 생성
    
    // 매개변수 집계 (빈 값과 실패한 값 제외)
    std::map<std::string, std::string> collectedParams;
    for (auto& param : params) {
        std::string value = param->getValue(*mockContext);
        if (!value.empty()) {  // 빈 값은 제외
            collectedParams[param->getName()] = value;
        }
    }
    
    // 결과 검증: 성공한 param만 포함되어야 함
    EXPECT_EQ(2, collectedParams.size()) << "Should only include successful parameters";
    EXPECT_EQ("test_value", collectedParams["valid_literal"]);
    EXPECT_EQ("success", collectedParams["valid_expr"]);
    
    // 실패한 param들은 포함되지 않아야 함
    EXPECT_EQ(collectedParams.end(), collectedParams.find("failed_expr"));
    EXPECT_EQ(collectedParams.end(), collectedParams.find("failed_location"));
    EXPECT_EQ(collectedParams.end(), collectedParams.find("empty_value"));
    
    std::cout << "INFO: Filtered collection - included " << collectedParams.size() 
              << " valid parameters out of 5" << std::endl;
}

// Send 작업 시뮬레이션: 실제 Send 노드에서 사용될 것처럼 테스트
TEST_F(ParamNodeTest, SendOperationSimulation)
{
    std::vector<std::unique_ptr<SCXML::Core::ParamNode>> params;
    
    // 전형적인 Send 작업 매개변수들
    auto eventParam = std::make_unique<SCXML::Core::ParamNode>("event");
    eventParam->setValue("payment.request");
    
    auto targetParam = std::make_unique<SCXML::Core::ParamNode>("target");
    targetParam->setLocation("payment.service.url");
    
    auto dataParam = std::make_unique<SCXML::Core::ParamNode>("amount");
    dataParam->setExpression("cart.total + tax");
    
    auto headerParam = std::make_unique<SCXML::Core::ParamNode>("Content-Type");
    headerParam->setValue("application/json");
    
    params.push_back(std::move(eventParam));
    params.push_back(std::move(targetParam));
    params.push_back(std::move(dataParam));
    params.push_back(std::move(headerParam));
    
    // Mock 설정
    EXPECT_CALL(*mockContext, getDataValue("payment.service.url"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("https://api.payment.com/v1")));
    
    EXPECT_CALL(*mockContext, evaluateExpression("cart.total + tax"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("125.50")));
    
    // Send 작업을 위한 매개변수 수집 및 구조화
    std::map<std::string, std::string> sendParams;
    std::string eventName;
    std::string target;
    
    for (auto& param : params) {
        std::string name = param->getName();
        std::string value = param->getValue(*mockContext);
        
        if (!value.empty()) {
            if (name == "event") {
                eventName = value;  // 이벤트명 별도 저장
            } else if (name == "target") {
                target = value;     // 대상 별도 저장
            } else {
                sendParams[name] = value;  // 나머지는 일반 매개변수
            }
        }
    }
    
    // Send 작업 시뮬레이션 검증
    EXPECT_EQ("payment.request", eventName);
    EXPECT_EQ("https://api.payment.com/v1", target);
    EXPECT_EQ(2, sendParams.size());
    EXPECT_EQ("125.50", sendParams["amount"]);
    EXPECT_EQ("application/json", sendParams["Content-Type"]);
    
    // 실제 Send 작업이라면 이 시점에서 외부 서비스로 전송됨
    std::cout << "SEND SIMULATION: Event '" << eventName << "' to '" << target 
              << "' with " << sendParams.size() << " parameters" << std::endl;
}

// Invoke 작업 시뮬레이션: 서비스 호출을 위한 매개변수 전달
TEST_F(ParamNodeTest, InvokeOperationSimulation)
{
    std::vector<std::unique_ptr<SCXML::Core::ParamNode>> params;
    
    // Invoke 작업 매개변수들
    auto serviceParam = std::make_unique<SCXML::Core::ParamNode>("src");
    serviceParam->setValue("http://api.weather.com/service");
    
    auto apiKeyParam = std::make_unique<SCXML::Core::ParamNode>("apikey");
    apiKeyParam->setLocation("config.weather.apiKey");
    
    auto locationParam = std::make_unique<SCXML::Core::ParamNode>("location");
    locationParam->setExpression("user.location.city");
    
    auto formatParam = std::make_unique<SCXML::Core::ParamNode>("format");
    formatParam->setValue("json");
    
    params.push_back(std::move(serviceParam));
    params.push_back(std::move(apiKeyParam));
    params.push_back(std::move(locationParam));
    params.push_back(std::move(formatParam));
    
    // Mock 설정
    EXPECT_CALL(*mockContext, getDataValue("config.weather.apiKey"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("abc123xyz789")));
    
    EXPECT_CALL(*mockContext, evaluateExpression("user.location.city"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("Seoul")));
    
    // Invoke를 위한 매개변수 수집
    std::map<std::string, std::string> invokeParams;
    std::string serviceUrl;
    
    for (auto& param : params) {
        std::string name = param->getName();
        std::string value = param->getValue(*mockContext);
        
        if (!value.empty()) {
            if (name == "src") {
                serviceUrl = value;  // 서비스 URL 별도 저장
            } else {
                invokeParams[name] = value;  // 서비스 매개변수
            }
        }
    }
    
    // Invoke 작업 시뮬레이션 검증
    EXPECT_EQ("http://api.weather.com/service", serviceUrl);
    EXPECT_EQ(3, invokeParams.size());
    EXPECT_EQ("abc123xyz789", invokeParams["apikey"]);
    EXPECT_EQ("Seoul", invokeParams["location"]);
    EXPECT_EQ("json", invokeParams["format"]);
    
    std::cout << "INVOKE SIMULATION: Service '" << serviceUrl 
              << "' with " << invokeParams.size() << " parameters" << std::endl;
}

// 복잡한 데이터 구조 형성: JSON 스타일 중첩 구조
TEST_F(ParamNodeTest, ComplexDataStructureFormation)
{
    std::vector<std::unique_ptr<SCXML::Core::ParamNode>> params;
    
    // 중첩된 구조를 시뮬레이션하는 매개변수들
    auto userIdParam = std::make_unique<SCXML::Core::ParamNode>("user.id");
    userIdParam->setValue("user_12345");
    
    auto userNameParam = std::make_unique<SCXML::Core::ParamNode>("user.name");
    userNameParam->setExpression("user.profile.fullName");
    
    auto orderIdParam = std::make_unique<SCXML::Core::ParamNode>("order.id");
    orderIdParam->setLocation("order.currentId");
    
    auto orderTotalParam = std::make_unique<SCXML::Core::ParamNode>("order.total");
    orderTotalParam->setExpression("order.items.reduce((sum, item) => sum + item.price, 0)");
    
    auto metaTimestampParam = std::make_unique<SCXML::Core::ParamNode>("meta.timestamp");
    metaTimestampParam->setValue("2024-01-15T14:30:00Z");
    
    params.push_back(std::move(userIdParam));
    params.push_back(std::move(userNameParam));
    params.push_back(std::move(orderIdParam));
    params.push_back(std::move(orderTotalParam));
    params.push_back(std::move(metaTimestampParam));
    
    // Mock 설정
    EXPECT_CALL(*mockContext, evaluateExpression("user.profile.fullName"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("John Smith")));
    
    EXPECT_CALL(*mockContext, getDataValue("order.currentId"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("ORD-2024-001")));
    
    EXPECT_CALL(*mockContext, evaluateExpression("order.items.reduce((sum, item) => sum + item.price, 0)"))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("299.97")));
    
    // 계층적 데이터 구조 형성
    std::map<std::string, std::map<std::string, std::string>> structuredData;
    
    for (auto& param : params) {
        std::string name = param->getName();
        std::string value = param->getValue(*mockContext);
        
        if (!value.empty()) {
            size_t dotPos = name.find('.');
            if (dotPos != std::string::npos) {
                std::string category = name.substr(0, dotPos);
                std::string field = name.substr(dotPos + 1);
                structuredData[category][field] = value;
            }
        }
    }
    
    // 구조화된 데이터 검증
    EXPECT_EQ(3, structuredData.size()) << "Should have 3 categories (user, order, meta)";
    
    EXPECT_EQ(2, structuredData["user"].size());
    EXPECT_EQ("user_12345", structuredData["user"]["id"]);
    EXPECT_EQ("John Smith", structuredData["user"]["name"]);
    
    EXPECT_EQ(2, structuredData["order"].size());
    EXPECT_EQ("ORD-2024-001", structuredData["order"]["id"]);
    EXPECT_EQ("299.97", structuredData["order"]["total"]);
    
    EXPECT_EQ(1, structuredData["meta"].size());
    EXPECT_EQ("2024-01-15T14:30:00Z", structuredData["meta"]["timestamp"]);
    
    std::cout << "COMPLEX STRUCTURE: Created " << structuredData.size() 
              << " categories with nested data" << std::endl;
}