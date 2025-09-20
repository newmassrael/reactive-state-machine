#include "actions/AssignAction.h"
#include "mocks/MockActionExecutor.h"
#include <gtest/gtest.h>
#include <memory>

using namespace RSM;
using namespace RSM::Test;

class AssignActionTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockExecutor = std::make_shared<MockActionExecutor>("test_session");
        context = std::make_shared<MockExecutionContext>(mockExecutor);
    }

    std::shared_ptr<MockActionExecutor> mockExecutor;
    std::shared_ptr<MockExecutionContext> context;
};

TEST_F(AssignActionTest, ConstructorAndBasicProperties) {
    AssignAction action("myVar", "42", "test_assign");

    EXPECT_EQ(action.getId(), "test_assign");
    EXPECT_EQ(action.getActionType(), "assign");
    EXPECT_EQ(action.getLocation(), "myVar");
    EXPECT_EQ(action.getExpr(), "42");
    EXPECT_TRUE(action.getType().empty());  // Default: no type hint
}

TEST_F(AssignActionTest, SuccessfulAssignment) {
    AssignAction action("counter", "counter + 1", "increment");

    mockExecutor->setVariableAssignmentResult(true);

    bool result = action.execute(*context);
    EXPECT_TRUE(result);

    // Verify assignment was performed
    auto &assignments = mockExecutor->getAssignedVariables();
    ASSERT_EQ(assignments.size(), 1);
    EXPECT_EQ(assignments.at("counter"), "counter + 1");
}

TEST_F(AssignActionTest, FailedAssignment) {
    AssignAction action("invalidVar", "invalid.expression", "fail_assign");

    mockExecutor->setVariableAssignmentResult(false);

    bool result = action.execute(*context);
    EXPECT_FALSE(result);

    // Verify assignment was attempted
    auto &assignments = mockExecutor->getAssignedVariables();
    ASSERT_EQ(assignments.size(), 1);
    EXPECT_EQ(assignments.at("invalidVar"), "invalid.expression");
}

TEST_F(AssignActionTest, ComplexLocationPaths) {
    // Test dot notation
    AssignAction dotAction("data.user.name", "'John Doe'", "dot_assign");
    mockExecutor->setVariableAssignmentResult(true);

    bool result = dotAction.execute(*context);
    EXPECT_TRUE(result);

    auto &assignments = mockExecutor->getAssignedVariables();
    EXPECT_EQ(assignments.at("data.user.name"), "'John Doe'");

    // Test nested object assignment
    mockExecutor->clearHistory();
    AssignAction nestedAction("user.profile.settings", "{theme: 'dark'}", "nested_assign");

    result = nestedAction.execute(*context);
    EXPECT_TRUE(result);

    auto &newAssignments = mockExecutor->getAssignedVariables();
    EXPECT_EQ(newAssignments.at("user.profile.settings"), "{theme: 'dark'}");
}

TEST_F(AssignActionTest, TypeHintHandling) {
    AssignAction action("stringVar", "'hello'", "typed_assign");
    action.setType("string");

    EXPECT_EQ(action.getType(), "string");

    mockExecutor->setVariableAssignmentResult(true);
    bool result = action.execute(*context);
    EXPECT_TRUE(result);

    // Type hint should be preserved in cloning
    auto cloned = action.clone();
    auto assignCloned = std::dynamic_pointer_cast<AssignAction>(cloned);
    ASSERT_NE(assignCloned, nullptr);
    EXPECT_EQ(assignCloned->getType(), "string");
}

TEST_F(AssignActionTest, EmptyLocationHandling) {
    AssignAction action("", "some_value", "empty_location");

    bool result = action.execute(*context);
    EXPECT_FALSE(result);

    // Should not call executor for empty location
    EXPECT_EQ(mockExecutor->getOperationCount("assign"), 0);
}

TEST_F(AssignActionTest, ValidationTests) {
    // Valid assignment
    AssignAction validAction("validVar", "42", "valid_id");
    auto errors = validAction.validate();
    EXPECT_TRUE(errors.empty());

    // Valid dot notation
    AssignAction dotAction("data.field", "value");
    errors = dotAction.validate();
    EXPECT_TRUE(errors.empty());

    // Empty location
    AssignAction emptyLocationAction("", "value");
    errors = emptyLocationAction.validate();
    EXPECT_FALSE(errors.empty());
    EXPECT_TRUE(errors[0].find("location cannot be empty") != std::string::npos);

    // Empty expression
    AssignAction emptyExprAction("var", "");
    errors = emptyExprAction.validate();
    EXPECT_FALSE(errors.empty());
    EXPECT_TRUE(errors[0].find("expression cannot be empty") != std::string::npos);

    // Invalid location characters
    AssignAction invalidLocationAction("invalid-var-name", "value");
    errors = invalidLocationAction.validate();
    EXPECT_FALSE(errors.empty());
    EXPECT_TRUE(errors[0].find("Invalid assignment location") != std::string::npos);

    // Invalid type hint
    AssignAction invalidTypeAction("var", "value");
    invalidTypeAction.setType("invalidtype");
    errors = invalidTypeAction.validate();
    EXPECT_FALSE(errors.empty());
    EXPECT_TRUE(errors[0].find("Invalid type hint") != std::string::npos);

    // Valid type hints
    std::vector<std::string> validTypes = {"string", "number", "boolean", "object", "array"};
    for (const auto &type : validTypes) {
        AssignAction typeAction("var", "value");
        typeAction.setType(type);
        errors = typeAction.validate();
        EXPECT_TRUE(errors.empty()) << "Type '" << type << "' should be valid";
    }
}

TEST_F(AssignActionTest, CloneOperation) {
    AssignAction original("originalVar", "originalExpr", "original_id");
    original.setType("number");

    auto cloned = original.clone();
    ASSERT_NE(cloned, nullptr);

    auto assignCloned = std::dynamic_pointer_cast<AssignAction>(cloned);
    ASSERT_NE(assignCloned, nullptr);

    EXPECT_EQ(assignCloned->getId(), "original_id");
    EXPECT_EQ(assignCloned->getActionType(), "assign");
    EXPECT_EQ(assignCloned->getLocation(), "originalVar");
    EXPECT_EQ(assignCloned->getExpr(), "originalExpr");
    EXPECT_EQ(assignCloned->getType(), "number");

    // Verify independence
    assignCloned->setLocation("modifiedVar");
    assignCloned->setExpr("modifiedExpr");
    assignCloned->setType("string");

    EXPECT_EQ(original.getLocation(), "originalVar");
    EXPECT_EQ(original.getExpr(), "originalExpr");
    EXPECT_EQ(original.getType(), "number");
}

TEST_F(AssignActionTest, DescriptionGeneration) {
    // Basic assignment
    AssignAction action("counter", "counter + 1", "increment");
    std::string desc = action.getDescription();
    EXPECT_TRUE(desc.find("assign") != std::string::npos);
    EXPECT_TRUE(desc.find("counter") != std::string::npos);
    EXPECT_TRUE(desc.find("counter + 1") != std::string::npos);

    // Assignment with type
    AssignAction typedAction("name", "'John'", "name_assign");
    typedAction.setType("string");
    desc = typedAction.getDescription();
    EXPECT_TRUE(desc.find("type: string") != std::string::npos);
}

TEST_F(AssignActionTest, PropertyModification) {
    AssignAction action("initial", "0");

    // Test location modification
    EXPECT_EQ(action.getLocation(), "initial");
    action.setLocation("modified");
    EXPECT_EQ(action.getLocation(), "modified");

    // Test expression modification
    EXPECT_EQ(action.getExpr(), "0");
    action.setExpr("100");
    EXPECT_EQ(action.getExpr(), "100");

    // Test type modification
    EXPECT_TRUE(action.getType().empty());
    action.setType("number");
    EXPECT_EQ(action.getType(), "number");
}

TEST_F(AssignActionTest, InvalidContextHandling) {
    AssignAction action("var", "value", "invalid_context");

    // Create context with null executor
    auto invalidContext = std::make_shared<MockExecutionContext>(nullptr);

    bool result = action.execute(*invalidContext);
    EXPECT_FALSE(result);
}

TEST_F(AssignActionTest, VariousExpressionTypes) {
    mockExecutor->setVariableAssignmentResult(true);

    // String literal
    AssignAction stringAction("str", "'hello world'");
    EXPECT_TRUE(stringAction.execute(*context));

    // Number literal
    AssignAction numberAction("num", "42.5");
    EXPECT_TRUE(numberAction.execute(*context));

    // Boolean literal
    AssignAction boolAction("flag", "true");
    EXPECT_TRUE(boolAction.execute(*context));

    // Object literal
    AssignAction objAction("obj", "{name: 'test', value: 123}");
    EXPECT_TRUE(objAction.execute(*context));

    // Array literal
    AssignAction arrAction("arr", "[1, 2, 3, 'four']");
    EXPECT_TRUE(arrAction.execute(*context));

    // Expression
    AssignAction exprAction("result", "Math.sqrt(16) + 2");
    EXPECT_TRUE(exprAction.execute(*context));

    // Verify all assignments were attempted
    auto &assignments = mockExecutor->getAssignedVariables();
    EXPECT_EQ(assignments.size(), 6);
}
