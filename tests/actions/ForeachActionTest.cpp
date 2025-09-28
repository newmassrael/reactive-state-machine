#include "actions/ForeachAction.h"
#include "mocks/MockActionExecutor.h"
#include <gtest/gtest.h>
#include <memory>

using namespace RSM;
using namespace RSM::Test;

class ForeachActionTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockExecutor = std::make_shared<MockActionExecutor>("foreach_test_session");
        context = std::make_shared<MockExecutionContext>(mockExecutor);
    }

    std::shared_ptr<MockActionExecutor> mockExecutor;
    std::shared_ptr<MockExecutionContext> context;
};

TEST_F(ForeachActionTest, DeclaresNewVariableForUndefinedItem) {
    // Test that foreach declares a new variable when item doesn't exist
    auto foreach = std::make_shared<ForeachAction>("var4", "", "myArray");

    // Mock: var4 doesn't exist initially
    EXPECT_CALL(*mockExecutor, hasVariable("var4")).WillOnce(testing::Return(false));

    // Mock: var4 should be declared
    EXPECT_CALL(*mockExecutor, declareVariable("var4", testing::_)).Times(1);

    // Execute foreach action
    foreach {
        ->execute(context);
    }
}

TEST_F(ForeachActionTest, PreservesExistingVariableForDefinedItem) {
    // Test that foreach uses existing variable when item already exists
    auto foreach = std::make_shared<ForeachAction>("existingVar", "", "myArray");

    // Mock: existingVar already exists
    EXPECT_CALL(*mockExecutor, hasVariable("existingVar")).WillOnce(testing::Return(true));

    // Mock: should NOT declare new variable
    EXPECT_CALL(*mockExecutor, declareVariable("existingVar", testing::_)).Times(0);

    // Execute foreach action
    foreach {
        ->execute(context);
    }
}

TEST_F(ForeachActionTest, DeclaresIndexVariableWhenSpecified) {
    // Test that foreach declares index variable when index attribute is present
    auto foreach = std::make_shared<ForeachAction>("var4", "var5", "myArray");

    // Mock: neither var4 nor var5 exist
    EXPECT_CALL(*mockExecutor, hasVariable("var4")).WillOnce(testing::Return(false));
    EXPECT_CALL(*mockExecutor, hasVariable("var5")).WillOnce(testing::Return(false));

    // Mock: both variables should be declared
    EXPECT_CALL(*mockExecutor, declareVariable("var4", testing::_)).Times(1);
    EXPECT_CALL(*mockExecutor, declareVariable("var5", testing::_)).Times(1);

    // Execute foreach action
    foreach {
        ->execute(context);
    }
}

TEST_F(ForeachActionTest, EmptyForeachStillDeclaresVariables) {
    // Test that empty foreach (no child actions) still declares variables
    auto foreach = std::make_shared<ForeachAction>("var4", "var5", "myArray");
    // No child actions added - this is an empty foreach

    // Mock: variables don't exist
    EXPECT_CALL(*mockExecutor, hasVariable("var4")).WillOnce(testing::Return(false));
    EXPECT_CALL(*mockExecutor, hasVariable("var5")).WillOnce(testing::Return(false));

    // Mock: variables should still be declared even if foreach is empty
    EXPECT_CALL(*mockExecutor, declareVariable("var4", testing::_)).Times(1);
    EXPECT_CALL(*mockExecutor, declareVariable("var5", testing::_)).Times(1);

    // Execute empty foreach action
    foreach {
        ->execute(context);
    }
}

TEST_F(ForeachActionTest, HandlesNumericVariableNames) {
    // Test that numeric variable names are handled correctly (like conf:item="4" -> var4)
    auto foreach = std::make_shared<ForeachAction>("var4", "var5", "var3");

    // Mock: numeric-based variables don't exist
    EXPECT_CALL(*mockExecutor, hasVariable("var4")).WillOnce(testing::Return(false));
    EXPECT_CALL(*mockExecutor, hasVariable("var5")).WillOnce(testing::Return(false));

    // Mock: variables should be declared
    EXPECT_CALL(*mockExecutor, declareVariable("var4", testing::_)).Times(1);
    EXPECT_CALL(*mockExecutor, declareVariable("var5", testing::_)).Times(1);

    // Execute foreach action
    foreach {
        ->execute(context);
    }
}

TEST_F(ForeachActionTest, VariableDeclarationFollowsECMAScriptRules) {
    // Test that variable declaration follows ECMAScript naming rules
    // This test ensures we don't try to declare invalid variable names like "4"
    auto foreach = std::make_shared<ForeachAction>("var4", "var5", "validArray");

    // Mock: variables don't exist
    EXPECT_CALL(*mockExecutor, hasVariable("var4")).WillOnce(testing::Return(false));
    EXPECT_CALL(*mockExecutor, hasVariable("var5")).WillOnce(testing::Return(false));

    // Mock: ensure we're declaring valid ECMAScript variable names
    EXPECT_CALL(*mockExecutor, declareVariable("var4", testing::_)).Times(1);
    EXPECT_CALL(*mockExecutor, declareVariable("var5", testing::_)).Times(1);

    // Should NOT try to declare invalid names like "4" or "5"
    EXPECT_CALL(*mockExecutor, declareVariable("4", testing::_)).Times(0);
    EXPECT_CALL(*mockExecutor, declareVariable("5", testing::_)).Times(0);

    // Execute foreach action
    foreach {
        ->execute(context);
    }
}