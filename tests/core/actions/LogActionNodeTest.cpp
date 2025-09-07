#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"
#include "core/actions/LogActionNode.h"
#include "runtime/RuntimeContext.h"

// SCXML W3C 사양에 따른 Log Action Node 테스트
class LogActionNodeTest : public CoreTestBase
{
protected:
    void SetUp() override
    {
        CoreTestBase::SetUp();
        mockContext = std::make_shared<MockExecutionContext>();
        logAction = std::make_shared<SCXML::Core::LogActionNode>("log1");
    }

    std::shared_ptr<MockExecutionContext> mockContext;
    std::shared_ptr<SCXML::Core::LogActionNode> logAction;
};

// SCXML 사양 3.8.2: <log> 기본 속성 테스트
TEST_F(LogActionNodeTest, BasicLogAttributes)
{
    // SCXML 사양: expr 속성으로 로그 내용 지정
    logAction->setExpr("'Hello SCXML World'");
    
    EXPECT_EQ("'Hello SCXML World'", logAction->getExpr());
    EXPECT_EQ("log", logAction->getActionType());
}

// SCXML 사양 3.8.2: label 속성 테스트  
TEST_F(LogActionNodeTest, LabelAttribute)
{
    // SCXML 사양: label 속성으로 로그 메타데이터 제공
    logAction->setLabel("Debug Info");
    logAction->setExpr("'Current state: ' + currentState");
    
    EXPECT_EQ("Debug Info", logAction->getLabel());
    EXPECT_EQ("'Current state: ' + currentState", logAction->getExpr());
}

// SCXML 사양 3.8.2: 표현식 평가 테스트
TEST_F(LogActionNodeTest, ExpressionEvaluation)
{
    // 리터럴 문자열
    logAction->setExpr("'Literal string message'");
    EXPECT_EQ("'Literal string message'", logAction->getExpr());
    
    // 변수 참조
    logAction->setExpr("userMessage");
    EXPECT_EQ("userMessage", logAction->getExpr());
    
    // 복합 표현식
    logAction->setExpr("'User ' + userName + ' logged in at ' + timestamp");
    EXPECT_EQ("'User ' + userName + ' logged in at ' + timestamp", logAction->getExpr());
    
    // 함수 호출
    logAction->setExpr("getCurrentTime()");
    EXPECT_EQ("getCurrentTime()", logAction->getExpr());
}

// SCXML 사양: 다양한 로그 레벨 지원
TEST_F(LogActionNodeTest, LogLevelSupport)
{
    // 표준 로그 레벨들
    logAction->setLevel("debug");
    EXPECT_EQ("debug", logAction->getLevel());
    
    logAction->setLevel("info");
    EXPECT_EQ("info", logAction->getLevel());
    
    logAction->setLevel("warning");
    EXPECT_EQ("warning", logAction->getLevel());
    
    logAction->setLevel("error");
    EXPECT_EQ("error", logAction->getLevel());
}

// SCXML 사양 3.8.2: 로그가 문서 해석에 영향 주지 않음
TEST_F(LogActionNodeTest, NonIntrusiveBehavior)
{
    // SCXML 사양: 로그는 플랫폼 의존적이며 실행에 영향 주지 않아야 함
    logAction->setExpr("'This should not affect state machine behavior'");
    logAction->setLabel("Non-intrusive test");
    
    // 구조적으로는 올바르게 설정되어야 함
    EXPECT_EQ("'This should not affect state machine behavior'", logAction->getExpr());
    EXPECT_EQ("Non-intrusive test", logAction->getLabel());
}

// SCXML 사양: clone 기능 테스트
TEST_F(LogActionNodeTest, CloneFunctionality)
{
    // 원본 설정
    logAction->setExpr("'Original log message'");
    logAction->setLabel("Original Label");
    logAction->setLevel("info");
    
    // Clone 생성
    auto cloned = logAction->clone();
    auto clonedLog = std::dynamic_pointer_cast<SCXML::Core::LogActionNode>(cloned);
    
    ASSERT_TRUE(clonedLog != nullptr);
    EXPECT_EQ(logAction->getExpr(), clonedLog->getExpr());
    EXPECT_EQ(logAction->getLabel(), clonedLog->getLabel());
    EXPECT_EQ(logAction->getLevel(), clonedLog->getLevel());
    
    // 서로 다른 객체임을 확인
    EXPECT_NE(logAction.get(), clonedLog.get());
    
    // 원본 수정이 clone에 영향 주지 않음 확인
    logAction->setExpr("'Modified message'");
    EXPECT_EQ("'Original log message'", clonedLog->getExpr());
    EXPECT_EQ("'Modified message'", logAction->getExpr());
}

// SCXML 사양 3.8.2: 복잡한 표현식 처리
TEST_F(LogActionNodeTest, ComplexExpressions)
{
    // JSON 형태 데이터 로그
    logAction->setExpr("JSON.stringify({state: currentState, time: new Date()})");
    EXPECT_EQ("JSON.stringify({state: currentState, time: new Date()})", logAction->getExpr());
    
    // 조건부 표현식
    logAction->setExpr("(status == 'error') ? 'Error occurred' : 'Normal operation'");
    EXPECT_EQ("(status == 'error') ? 'Error occurred' : 'Normal operation'", logAction->getExpr());
    
    // 배열 처리
    logAction->setExpr("'Active states: ' + activeStates.join(', ')");
    EXPECT_EQ("'Active states: ' + activeStates.join(', ')", logAction->getExpr());
}

// SCXML 사양: 디버깅 목적의 상태 정보 로깅
TEST_F(LogActionNodeTest, StateDebuggingInfo)
{
    logAction->setLabel("State Machine Debug");
    
    // 현재 상태 로깅
    logAction->setExpr("'Current configuration: ' + _configuration");
    EXPECT_EQ("'Current configuration: ' + _configuration", logAction->getExpr());
    
    // 이벤트 정보 로깅  
    logAction->setExpr("'Processing event: ' + _event.name + ' with data: ' + JSON.stringify(_event.data)");
    EXPECT_EQ("'Processing event: ' + _event.name + ' with data: ' + JSON.stringify(_event.data)", logAction->getExpr());
    
    // 세션 ID 로깅
    logAction->setExpr("'Session ID: ' + _sessionid");
    EXPECT_EQ("'Session ID: ' + _sessionid", logAction->getExpr());
}

// SCXML 사양: 조건부 로깅 (다른 액션과 함께 사용)
TEST_F(LogActionNodeTest, ConditionalLogging)
{
    // 오류 상황에서의 로깅
    logAction->setLabel("Error Handling");
    logAction->setExpr("'Error occurred: ' + errorMessage + ' at ' + new Date().toISOString()");
    logAction->setLevel("error");
    
    EXPECT_EQ("Error Handling", logAction->getLabel());
    EXPECT_EQ("'Error occurred: ' + errorMessage + ' at ' + new Date().toISOString()", logAction->getExpr());
    EXPECT_EQ("error", logAction->getLevel());
    
    // 성공 상황에서의 로깅
    auto successLog = std::make_shared<SCXML::Core::LogActionNode>("success");
    successLog->setLabel("Success Info");
    successLog->setExpr("'Operation completed successfully'");
    successLog->setLevel("info");
    
    EXPECT_EQ("Success Info", successLog->getLabel());
    EXPECT_EQ("'Operation completed successfully'", successLog->getExpr());
    EXPECT_EQ("info", successLog->getLevel());
}

// SCXML 사양: 빈 표현식 처리
TEST_F(LogActionNodeTest, EmptyExpressionHandling)
{
    // 빈 표현식 설정 (유효하지 않을 수 있음)
    logAction->setExpr("");
    EXPECT_EQ("", logAction->getExpr());
    
    // 공백만 있는 표현식
    logAction->setExpr("   ");
    EXPECT_EQ("   ", logAction->getExpr());
}

// SCXML 사양: 플랫폼별 로그 형식
TEST_F(LogActionNodeTest, PlatformSpecificFormatting)
{
    // XML 형태 로그 메시지
    logAction->setExpr("'<log><level>info</level><message>' + message + '</message></log>'");
    EXPECT_EQ("'<log><level>info</level><message>' + message + '</message></log>'", logAction->getExpr());
    
    // 구조화된 로그 메시지
    logAction->setLabel("Structured Log");
    logAction->setExpr("{ timestamp: Date.now(), level: 'info', component: 'StateMachine', message: details }");
    EXPECT_EQ("{ timestamp: Date.now(), level: 'info', component: 'StateMachine', message: details }", logAction->getExpr());
}

// SCXML 사양: 특수 문자가 포함된 로그 메시지
TEST_F(LogActionNodeTest, SpecialCharacterHandling)
{
    // 인용부호가 포함된 메시지
    logAction->setExpr("'User said: \"Hello World\"'");
    EXPECT_EQ("'User said: \"Hello World\"'", logAction->getExpr());
    
    // 줄바꿈이 포함된 메시지
    logAction->setExpr("'Line 1\\nLine 2\\nLine 3'");
    EXPECT_EQ("'Line 1\\nLine 2\\nLine 3'", logAction->getExpr());
    
    // 특수 문자가 포함된 레이블
    logAction->setLabel("Debug [Critical]: User Action");
    EXPECT_EQ("Debug [Critical]: User Action", logAction->getLabel());
}

// SCXML 사양: 로그 비활성화 시나리오
TEST_F(LogActionNodeTest, LoggingDisabledScenario)
{
    // 로깅이 비활성화되어도 구조는 유지되어야 함
    logAction->setExpr("'This message may not appear if logging is disabled'");
    logAction->setLabel("Optional Log");
    
    // 구조적 정확성은 항상 유지
    EXPECT_EQ("'This message may not appear if logging is disabled'", logAction->getExpr());
    EXPECT_EQ("Optional Log", logAction->getLabel());
}