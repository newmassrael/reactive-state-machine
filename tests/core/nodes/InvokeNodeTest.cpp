#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"

// Simplified Invoke Node test - testing basic functionality without the problematic InvokeNode class
#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"
#include "core/InvokeNode.h"

// SCXML W3C 사양에 따른 Invoke Node 테스트
class InvokeNodeTest : public CoreTestBase
{
protected:
    void SetUp() override
    {
        CoreTestBase::SetUp();
        mockContext = std::make_shared<MockExecutionContext>();
        invokeNode = std::make_shared<SCXML::Core::InvokeNode>("invoke1");
    }

    std::shared_ptr<MockExecutionContext> mockContext;
    std::shared_ptr<SCXML::Core::InvokeNode> invokeNode;
};;

// SCXML 사양: <invoke> 기본 속성 테스트
TEST_F(InvokeNodeTest, BasicInvokeAttributes)
{
    // SCXML 사양: id 속성 (필수)
    EXPECT_EQ("invoke1", invokeNode->getId());
    
    // SCXML 사양: type 속성 설정
    invokeNode->setType("http://www.w3.org/TR/scxml/");
    EXPECT_EQ("http://www.w3.org/TR/scxml/", invokeNode->getType());
    
    // SCXML 사양: src 속성 설정
    invokeNode->setSrc("child.scxml");
    EXPECT_EQ("child.scxml", invokeNode->getSrc());
    
    // SCXML 사양: autoforward 속성 (기본값 false)
    EXPECT_FALSE(invokeNode->isAutoForward());
    invokeNode->setAutoForward(true);
    EXPECT_TRUE(invokeNode->isAutoForward());
}

// SCXML 사양: idlocation과 namelist 속성 테스트
TEST_F(InvokeNodeTest, IdLocationAndNamelistAttributes)
{
    // SCXML 사양: idlocation 속성 (선택적)
    invokeNode->setIdLocation("data.childId");
    EXPECT_EQ("data.childId", invokeNode->getIdLocation());
    
    // SCXML 사양: namelist 속성 (선택적)
    invokeNode->setNamelist("param1 param2 param3");
    EXPECT_EQ("param1 param2 param3", invokeNode->getNamelist());
    
    // 빈 값 처리 테스트
    invokeNode->setIdLocation("");
    EXPECT_EQ("", invokeNode->getIdLocation());
    
    invokeNode->setNamelist("");
    EXPECT_EQ("", invokeNode->getNamelist());
}

// SCXML 사양: <param> 요소 관리 테스트
TEST_F(InvokeNodeTest, ParameterManagement)
{
    // SCXML 사양: <param> 요소 추가 (name 속성 필수)
    invokeNode->addParam("username", "user.name", "");
    invokeNode->addParam("sessionId", "", "data.session");
    invokeNode->addParam("config", "defaultConfig", "");
    
    // Parameter 추가가 정상적으로 수행되는지 확인
    // 실제 구현에서는 getParams() 같은 메소드가 있을 수 있지만,
    // 현재는 addParam 호출이 성공하는지만 확인
    EXPECT_NO_THROW(invokeNode->addParam("test", "value", ""));
    
    // 빈 이름으로 파라미터 추가 시 처리
    EXPECT_NO_THROW(invokeNode->addParam("", "value", "location"));
}

// SCXML 사양: content와 finalize 요소 테스트
TEST_F(InvokeNodeTest, ContentAndFinalizeHandling)
{
    // SCXML 사양: <content> 요소 설정
    std::string contentData = R"(<data xmlns="http://www.w3.org/TR/scxml/" id="config">
        <property name="timeout">5000</property>
        <property name="retries">3</property>
    </data>)";
    
    invokeNode->setContent(contentData);
    EXPECT_EQ(contentData, invokeNode->getContent());
    
    // SCXML 사양: <finalize> 요소 설정
    std::string finalizeScript = R"(<assign location="data.result" expr="childData"/>
    <log expr="'Child process completed with: ' + data.result"/>)";
    
    invokeNode->setFinalize(finalizeScript);
    EXPECT_EQ(finalizeScript, invokeNode->getFinalize());
    
    // 빈 content와 finalize 처리
    invokeNode->setContent("");
    EXPECT_EQ("", invokeNode->getContent());
    
    invokeNode->setFinalize("");
    EXPECT_EQ("", invokeNode->getFinalize());
}

// SCXML 사양: 다양한 invoke 타입 테스트
TEST_F(InvokeNodeTest, InvokeTypeHandling)
{
    // SCXML 사양: SCXML 타입 invoke
    invokeNode->setType("http://www.w3.org/TR/scxml/");
    invokeNode->setSrc("child-machine.scxml");
    EXPECT_EQ("http://www.w3.org/TR/scxml/", invokeNode->getType());
    EXPECT_EQ("child-machine.scxml", invokeNode->getSrc());
    
    // 다른 invoke 타입들
    invokeNode->setType("http://www.w3.org/TR/ccxml/");
    EXPECT_EQ("http://www.w3.org/TR/ccxml/", invokeNode->getType());
    
    // 커스텀 invoke 타입
    invokeNode->setType("http://example.com/custom-service");
    invokeNode->setSrc("http://api.example.com/service");
    EXPECT_EQ("http://example.com/custom-service", invokeNode->getType());
    EXPECT_EQ("http://api.example.com/service", invokeNode->getSrc());
}

// SCXML 사양: autoforward 기능 테스트
TEST_F(InvokeNodeTest, AutoforwardFunctionality)
{
    // SCXML 사양: autoforward가 false일 때 (기본값)
    EXPECT_FALSE(invokeNode->isAutoForward());
    
    // autoforward 활성화
    invokeNode->setAutoForward(true);
    EXPECT_TRUE(invokeNode->isAutoForward());
    
    // autoforward 비활성화
    invokeNode->setAutoForward(false);
    EXPECT_FALSE(invokeNode->isAutoForward());
}

// SCXML 사양: invoke 완전성 검증 테스트
TEST_F(InvokeNodeTest, InvokeCompleteness)
{
    // 완전한 invoke 설정
    invokeNode->setType("http://www.w3.org/TR/scxml/");
    invokeNode->setSrc("workflow.scxml");
    invokeNode->setAutoForward(true);
    invokeNode->setIdLocation("data.workflowId");
    invokeNode->setNamelist("userId sessionToken");
    
    // 파라미터 추가
    invokeNode->addParam("userId", "user.id", "");
    invokeNode->addParam("sessionToken", "", "session.token");
    
    // Content 설정
    invokeNode->setContent("<config><timeout>30000</timeout></config>");
    
    // Finalize 설정  
    invokeNode->setFinalize("<log expr=\"'Workflow completed'\"/>");
    
    // 모든 속성이 올바르게 설정되었는지 확인
    EXPECT_EQ("invoke1", invokeNode->getId());
    EXPECT_EQ("http://www.w3.org/TR/scxml/", invokeNode->getType());
    EXPECT_EQ("workflow.scxml", invokeNode->getSrc());
    EXPECT_TRUE(invokeNode->isAutoForward());
    EXPECT_EQ("data.workflowId", invokeNode->getIdLocation());
    EXPECT_EQ("userId sessionToken", invokeNode->getNamelist());
    EXPECT_EQ("<config><timeout>30000</timeout></config>", invokeNode->getContent());
    EXPECT_EQ("<log expr=\"'Workflow completed'\"/>", invokeNode->getFinalize());
}

// SCXML 사양: 빈 값 처리 테스트
TEST_F(InvokeNodeTest, EmptyValueHandling)
{
    // 빈 type 설정
    invokeNode->setType("");
    EXPECT_EQ("", invokeNode->getType());
    
    // 빈 src 설정
    invokeNode->setSrc("");
    EXPECT_EQ("", invokeNode->getSrc());
    
    // 이러한 상황에서도 기본 동작이 안정적이어야 함
    EXPECT_EQ("invoke1", invokeNode->getId());
    EXPECT_FALSE(invokeNode->isAutoForward()); // 기본값 유지
}