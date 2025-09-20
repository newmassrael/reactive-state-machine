#include "actions/ScriptAction.h"
#include "mocks/MockActionExecutor.h"
#include "runtime/ExecutionContextImpl.h"
#include <gtest/gtest.h>
#include <memory>

using namespace RSM;
using namespace RSM::Test;

class ScriptActionTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockExecutor = std::make_shared<MockActionExecutor>("test_session");
        context = std::make_shared<MockExecutionContext>(mockExecutor);
    }

    std::shared_ptr<MockActionExecutor> mockExecutor;
    std::shared_ptr<MockExecutionContext> context;
};

TEST_F(ScriptActionTest, ConstructorAndBasicProperties) {
    ScriptAction action("console.log('Hello');", "test_id");

    EXPECT_EQ(action.getId(), "test_id");
    EXPECT_EQ(action.getActionType(), "script");
    EXPECT_EQ(action.getContent(), "console.log('Hello');");
    EXPECT_FALSE(action.isEmpty());
}

TEST_F(ScriptActionTest, EmptyScriptHandling) {
    ScriptAction action("", "empty_script");

    EXPECT_TRUE(action.isEmpty());
    EXPECT_EQ(action.getContent(), "");

    // Empty script should execute successfully (no-op)
    bool result = action.execute(*context);
    EXPECT_TRUE(result);

    // Should not call executor for empty script
    EXPECT_EQ(mockExecutor->getOperationCount("script"), 0);
}

TEST_F(ScriptActionTest, WhitespaceOnlyScript) {
    ScriptAction action("   \n\t  ", "whitespace_script");

    EXPECT_TRUE(action.isEmpty());

    // Whitespace-only script should execute successfully (no-op)
    bool result = action.execute(*context);
    EXPECT_TRUE(result);

    EXPECT_EQ(mockExecutor->getOperationCount("script"), 0);
}

TEST_F(ScriptActionTest, SuccessfulScriptExecution) {
    ScriptAction action("var x = 42; console.log(x);", "success_script");

    mockExecutor->setScriptExecutionResult(true);

    bool result = action.execute(*context);
    EXPECT_TRUE(result);

    // Verify script was executed
    auto &executedScripts = mockExecutor->getExecutedScripts();
    ASSERT_EQ(executedScripts.size(), 1);
    EXPECT_EQ(executedScripts[0], "var x = 42; console.log(x);");
}

TEST_F(ScriptActionTest, FailedScriptExecution) {
    ScriptAction action("invalid.syntax.error;", "fail_script");

    mockExecutor->setScriptExecutionResult(false);

    bool result = action.execute(*context);
    EXPECT_FALSE(result);

    // Verify script was attempted
    auto &executedScripts = mockExecutor->getExecutedScripts();
    ASSERT_EQ(executedScripts.size(), 1);
    EXPECT_EQ(executedScripts[0], "invalid.syntax.error;");
}

TEST_F(ScriptActionTest, InvalidContextHandling) {
    ScriptAction action("console.log('test');", "invalid_context");

    // Create context with null executor
    auto invalidContext = std::make_shared<MockExecutionContext>(nullptr);

    // Should return false for invalid context
    bool result = action.execute(*invalidContext);
    EXPECT_FALSE(result);
}

TEST_F(ScriptActionTest, CloneOperation) {
    ScriptAction original("original_script();", "original_id");

    auto cloned = original.clone();
    ASSERT_NE(cloned, nullptr);

    auto scriptCloned = std::dynamic_pointer_cast<ScriptAction>(cloned);
    ASSERT_NE(scriptCloned, nullptr);

    EXPECT_EQ(scriptCloned->getId(), "original_id");
    EXPECT_EQ(scriptCloned->getActionType(), "script");
    EXPECT_EQ(scriptCloned->getContent(), "original_script();");

    // Verify independence - modifying clone shouldn't affect original
    scriptCloned->setContent("modified_script();");
    EXPECT_EQ(original.getContent(), "original_script();");
    EXPECT_EQ(scriptCloned->getContent(), "modified_script();");
}

TEST_F(ScriptActionTest, ValidationTests) {
    // Valid script
    ScriptAction validAction("console.log('valid');", "valid_id");
    auto errors = validAction.validate();
    EXPECT_TRUE(errors.empty());

    // Empty script (should be valid)
    ScriptAction emptyAction("", "empty_id");
    errors = emptyAction.validate();
    EXPECT_TRUE(errors.empty());

    // Invalid ID characters
    ScriptAction invalidIdAction("console.log('test');", "invalid-id-with-dashes");
    errors = invalidIdAction.validate();
    EXPECT_FALSE(errors.empty());
    EXPECT_EQ(errors.size(), 1);
    EXPECT_TRUE(errors[0].find("invalid characters") != std::string::npos);
}

TEST_F(ScriptActionTest, DescriptionGeneration) {
    // Script with ID
    ScriptAction actionWithId("console.log('Hello World');", "my_script");
    std::string desc = actionWithId.getDescription();
    EXPECT_TRUE(desc.find("script") != std::string::npos);
    EXPECT_TRUE(desc.find("my_script") != std::string::npos);
    EXPECT_TRUE(desc.find("Hello World") != std::string::npos);

    // Script without ID
    ScriptAction actionNoId("var x = 1;");
    desc = actionNoId.getDescription();
    EXPECT_TRUE(desc.find("script") != std::string::npos);
    EXPECT_TRUE(desc.find("var x = 1") != std::string::npos);

    // Long script (should be truncated)
    std::string longScript =
        "var verylongvariablenamethatexceedsthelimitfor description truncation and testing purposes;";
    ScriptAction longAction(longScript);
    desc = longAction.getDescription();
    EXPECT_TRUE(desc.find("...") != std::string::npos);
    EXPECT_LT(desc.length(), longScript.length() + 20);  // Should be truncated

    // Empty script
    ScriptAction emptyAction("");
    desc = emptyAction.getDescription();
    EXPECT_TRUE(desc.find("empty script") != std::string::npos);
}

TEST_F(ScriptActionTest, ContentModification) {
    ScriptAction action("initial_content();");

    EXPECT_EQ(action.getContent(), "initial_content();");
    EXPECT_FALSE(action.isEmpty());

    action.setContent("modified_content();");
    EXPECT_EQ(action.getContent(), "modified_content();");
    EXPECT_FALSE(action.isEmpty());

    action.setContent("");
    EXPECT_EQ(action.getContent(), "");
    EXPECT_TRUE(action.isEmpty());
}

TEST_F(ScriptActionTest, MultipleExecutions) {
    ScriptAction action("counter++;", "counter_script");
    mockExecutor->setScriptExecutionResult(true);

    // Execute multiple times
    EXPECT_TRUE(action.execute(*context));
    EXPECT_TRUE(action.execute(*context));
    EXPECT_TRUE(action.execute(*context));

    // Verify all executions were recorded
    auto &executedScripts = mockExecutor->getExecutedScripts();
    EXPECT_EQ(executedScripts.size(), 3);
    for (const auto &script : executedScripts) {
        EXPECT_EQ(script, "counter++;");
    }
}
