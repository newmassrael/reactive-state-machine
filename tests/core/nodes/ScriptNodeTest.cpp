#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"
#include "core/ScriptNode.h"
#include "common/Result.h"
#include <fstream>
#include <filesystem>
#include <memory>

// SCXML W3C 사양에 따른 Script Node 테스트
class ScriptNodeTest : public CoreTestBase
{
protected:
    void SetUp() override
    {
        CoreTestBase::SetUp();
        mockContext = std::make_shared<MockExecutionContext>();
        scriptNode = std::make_shared<SCXML::Core::ScriptNode>();
        
        // 임시 테스트 스크립트 파일 생성
        testScriptFile = "test_script.js";
        testScriptContent = "var x = 42; _sessionid = 'test123';";
        createTestScriptFile();
    }

    void TearDown() override
    {
        // 임시 파일 정리
        cleanupTestFile();
        CoreTestBase::TearDown();
    }

    void createTestScriptFile()
    {
        std::ofstream file(testScriptFile);
        if (file.is_open()) {
            file << testScriptContent;
            file.close();
        }
    }

    void cleanupTestFile()
    {
        if (std::filesystem::exists(testScriptFile)) {
            std::filesystem::remove(testScriptFile);
        }
    }

    std::shared_ptr<MockExecutionContext> mockContext;
    std::shared_ptr<SCXML::Core::ScriptNode> scriptNode;
    std::string testScriptFile;
    std::string testScriptContent;
};

// SCXML 사양: <script> 기본 속성 테스트
TEST_F(ScriptNodeTest, BasicScriptAttributes)
{
    // 기본 설정 확인
    EXPECT_EQ("", scriptNode->getContent());
    EXPECT_EQ("", scriptNode->getSrc());
    EXPECT_EQ("ecmascript", scriptNode->getType());  // 기본값은 ecmascript
    EXPECT_TRUE(scriptNode->isInitializationScript()); // 기본값은 true
    EXPECT_EQ(0, scriptNode->getExecutionPriority());

    // 속성 설정 테스트
    scriptNode->setContent("console.log('test');");
    EXPECT_EQ("console.log('test');", scriptNode->getContent());

    scriptNode->setSrc("script.js");
    EXPECT_EQ("script.js", scriptNode->getSrc());

    scriptNode->setType("javascript");
    EXPECT_EQ("javascript", scriptNode->getType());

    scriptNode->setInitializationScript(false);
    EXPECT_FALSE(scriptNode->isInitializationScript());

    scriptNode->setExecutionPriority(10);
    EXPECT_EQ(10, scriptNode->getExecutionPriority());
}

// SCXML W3C 사양: 인라인 스크립트 실행 테스트
TEST_F(ScriptNodeTest, InlineScriptExecution)
{
    std::string scriptContent = "var counter = 0; counter++;";
    scriptNode->setContent(scriptContent);

    // Mock execution context 설정
    EXPECT_CALL(*mockContext, evaluateExpression(scriptContent))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("undefined")));

    // 스크립트 실행
    auto result = scriptNode->execute(*mockContext);
    EXPECT_TRUE(result.isSuccess()) << "Inline script execution should succeed";
}

// SCXML W3C 사양: 외부 스크립트 파일 로드 및 실행 테스트
TEST_F(ScriptNodeTest, ExternalScriptExecution)
{
    scriptNode->setSrc(testScriptFile);

    // Mock execution context 설정
    EXPECT_CALL(*mockContext, evaluateExpression(testScriptContent))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("undefined")));

    // 외부 스크립트 실행
    auto result = scriptNode->execute(*mockContext);
    EXPECT_TRUE(result.isSuccess()) << "External script execution should succeed";
}

// SCXML W3C 사양: loadFromFile 기능 테스트
TEST_F(ScriptNodeTest, LoadFromFileFunction)
{
    auto result = scriptNode->loadFromFile(testScriptFile);
    EXPECT_TRUE(result.isSuccess()) << "Script loading from file should succeed";
    EXPECT_EQ(testScriptContent, scriptNode->getContent());
    EXPECT_EQ(testScriptFile, scriptNode->getSrc());
}

// SCXML W3C 사양: 스크립트 타입 검증 테스트
TEST_F(ScriptNodeTest, ScriptTypeValidation)
{
    // 지원되는 타입 테스트
    scriptNode->setType("ecmascript");
    auto errors = scriptNode->validate();
    EXPECT_FALSE(std::any_of(errors.begin(), errors.end(), 
        [](const std::string& error) { return error.find("Unsupported script type") != std::string::npos; }));

    scriptNode->setType("javascript");
    errors = scriptNode->validate();
    EXPECT_FALSE(std::any_of(errors.begin(), errors.end(), 
        [](const std::string& error) { return error.find("Unsupported script type") != std::string::npos; }));

    // 지원되지 않는 타입 테스트
    scriptNode->setType("python");
    errors = scriptNode->validate();
    EXPECT_TRUE(std::any_of(errors.begin(), errors.end(), 
        [](const std::string& error) { return error.find("Unsupported script type: python") != std::string::npos; }));
}

// SCXML W3C 사양: 스크립트 검증 - content 또는 src 필수
TEST_F(ScriptNodeTest, ContentOrSrcRequired)
{
    // content도 src도 없는 경우
    auto errors = scriptNode->validate();
    EXPECT_TRUE(std::any_of(errors.begin(), errors.end(), 
        [](const std::string& error) { return error.find("must have either content or src") != std::string::npos; }));

    // content만 있는 경우 - 유효
    scriptNode->setContent("var x = 1;");
    errors = scriptNode->validate();
    EXPECT_FALSE(std::any_of(errors.begin(), errors.end(), 
        [](const std::string& error) { return error.find("must have either content or src") != std::string::npos; }));

    // src만 있는 경우 - 유효 (파일이 존재하면)
    scriptNode->setContent("");
    scriptNode->setSrc(testScriptFile);
    errors = scriptNode->validate();
    EXPECT_FALSE(std::any_of(errors.begin(), errors.end(), 
        [](const std::string& error) { return error.find("must have either content or src") != std::string::npos; }));
}

// SCXML W3C 사양: content와 src 동시 사용 금지
TEST_F(ScriptNodeTest, ContentAndSrcMutuallyExclusive)
{
    scriptNode->setContent("var x = 1;");
    scriptNode->setSrc(testScriptFile);

    auto errors = scriptNode->validate();
    EXPECT_TRUE(std::any_of(errors.begin(), errors.end(), 
        [](const std::string& error) { return error.find("cannot have both content and src") != std::string::npos; }));
}

// SCXML W3C 사양: 존재하지 않는 src 파일 오류 처리
TEST_F(ScriptNodeTest, NonExistentSrcFileValidation)
{
    scriptNode->setSrc("nonexistent_file.js");
    
    auto errors = scriptNode->validate();
    EXPECT_TRUE(std::any_of(errors.begin(), errors.end(), 
        [](const std::string& error) { return error.find("Script source file not found") != std::string::npos; }));
}

// SCXML W3C 사양: 스크립트 실행 실패 처리
TEST_F(ScriptNodeTest, ScriptExecutionFailure)
{
    scriptNode->setContent("invalid javascript syntax !!!");

    // Mock execution context가 실행 실패를 반환하도록 설정
    EXPECT_CALL(*mockContext, evaluateExpression(testing::_))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::failure("Syntax error")));

    auto result = scriptNode->execute(*mockContext);
    EXPECT_FALSE(result.isSuccess()) << "Invalid script should fail execution";
}

// SCXML W3C 사양: 빈 스크립트 콘텐츠 처리
TEST_F(ScriptNodeTest, EmptyScriptContentHandling)
{
    scriptNode->setContent("");

    // 빈 스크립트는 성공적으로 처리되지만 실제 실행은 하지 않음
    auto result = scriptNode->execute(*mockContext);
    EXPECT_TRUE(result.isSuccess()) << "Empty script should succeed without execution";
}

// SCXML W3C 사양: 스크립트 노드 복제 테스트
TEST_F(ScriptNodeTest, CloneFunctionality)
{
    // 원본 설정
    scriptNode->setContent("var test = 'clone';");
    scriptNode->setType("javascript");
    scriptNode->setSrc("original.js");
    scriptNode->setInitializationScript(false);
    scriptNode->setExecutionPriority(5);

    // 복제 수행
    auto cloned = scriptNode->clone();
    auto scriptClone = std::dynamic_pointer_cast<SCXML::Core::ScriptNode>(cloned);

    ASSERT_TRUE(scriptClone != nullptr);
    EXPECT_EQ(scriptNode->getContent(), scriptClone->getContent());
    EXPECT_EQ(scriptNode->getType(), scriptClone->getType());
    EXPECT_EQ(scriptNode->getSrc(), scriptClone->getSrc());
    EXPECT_EQ(scriptNode->isInitializationScript(), scriptClone->isInitializationScript());
    EXPECT_EQ(scriptNode->getExecutionPriority(), scriptClone->getExecutionPriority());

    // 별개 객체인지 확인
    EXPECT_NE(scriptNode.get(), scriptClone.get());
}

// SCXML W3C 사양: 초기화 스크립트 동작 테스트
TEST_F(ScriptNodeTest, InitializationScriptBehavior)
{
    // 초기화 스크립트는 기본값으로 true
    EXPECT_TRUE(scriptNode->isInitializationScript());

    // 초기화 스크립트 설정/해제
    scriptNode->setInitializationScript(false);
    EXPECT_FALSE(scriptNode->isInitializationScript());

    scriptNode->setInitializationScript(true);
    EXPECT_TRUE(scriptNode->isInitializationScript());
}

// SCXML W3C 사양: 실행 우선순위 테스트
TEST_F(ScriptNodeTest, ExecutionPriorityOrdering)
{
    // 기본 우선순위는 0
    EXPECT_EQ(0, scriptNode->getExecutionPriority());

    // 우선순위 설정
    scriptNode->setExecutionPriority(100);
    EXPECT_EQ(100, scriptNode->getExecutionPriority());

    scriptNode->setExecutionPriority(-50);
    EXPECT_EQ(-50, scriptNode->getExecutionPriority());
}



// SCXML W3C 사양: ECMAScript 실행 엔진 테스트
TEST_F(ScriptNodeTest, ECMAScriptEngineExecution)
{
    std::string ecmaScript = "function test() { return 'hello'; } test();";
    scriptNode->setContent(ecmaScript);
    scriptNode->setType("ecmascript");

    // Mock execution context 설정
    EXPECT_CALL(*mockContext, evaluateExpression(ecmaScript))
        .WillOnce(testing::Return(SCXML::Common::Result<std::string>::success("hello")));

    auto result = scriptNode->execute(*mockContext);
    EXPECT_TRUE(result.isSuccess()) << "ECMAScript execution should succeed";
}

// SCXML W3C 사양: 파일에서 로드 실패 처리
TEST_F(ScriptNodeTest, LoadFromFileFailure)
{
    std::string nonExistentFile = "does_not_exist.js";
    
    auto result = scriptNode->loadFromFile(nonExistentFile);
    EXPECT_FALSE(result.isSuccess()) << "Loading non-existent file should fail";
}