#include "CoreTestCommon.h"

// TransitionNode core functionality tests
class TransitionNodeTest : public CoreTestBase
{
};

// Basic transition creation test
TEST_F(TransitionNodeTest, BasicTransitionCreation)
{
    auto transition = std::make_unique<SCXML::Core::TransitionNode>("", "target1");
    
    EXPECT_EQ(1, transition->getTargets().size());
    EXPECT_EQ("target1", transition->getTargets()[0]);
    EXPECT_TRUE(transition->getEvent().empty());
    EXPECT_TRUE(transition->getGuard().empty());
}

// Event-based transition test
TEST_F(TransitionNodeTest, EventBasedTransition)
{
    auto transition = std::make_unique<SCXML::Core::TransitionNode>("button.click", "target1");
    
    EXPECT_EQ("button.click", transition->getEvent());
    EXPECT_EQ("target1", transition->getTargets()[0]);
}

// Conditional transition test
TEST_F(TransitionNodeTest, ConditionalTransition)
{
    auto transition = std::make_unique<SCXML::Core::TransitionNode>("", "target1");
    
    transition->setGuard("x > 0");
    
    EXPECT_EQ("x > 0", transition->getGuard());
    EXPECT_EQ("target1", transition->getTargets()[0]);
}

// Multiple target transition test
TEST_F(TransitionNodeTest, MultipleTargetTransition)
{
    auto transition = std::make_unique<SCXML::Core::TransitionNode>("", "target1");
    transition->addTarget("target2");
    transition->addTarget("target3");
    
    EXPECT_EQ(3, transition->getTargets().size());
    EXPECT_EQ("target1", transition->getTargets()[0]);
    EXPECT_EQ("target2", transition->getTargets()[1]);
    EXPECT_EQ("target3", transition->getTargets()[2]);
}

// Transition with event and condition test
TEST_F(TransitionNodeTest, EventAndConditionTransition)
{
    auto transition = std::make_unique<SCXML::Core::TransitionNode>("process.complete", "success");
    
    transition->setGuard("result == 'success'");
    
    EXPECT_EQ("process.complete", transition->getEvent());
    EXPECT_EQ("result == 'success'", transition->getGuard());
    EXPECT_EQ("success", transition->getTargets()[0]);
}

// Internal transition test (targetless)
TEST_F(TransitionNodeTest, InternalTransition)
{
    auto transition = std::make_unique<SCXML::Core::TransitionNode>("internal.event", ""); // Empty target for internal transition
    
    EXPECT_EQ("internal.event", transition->getEvent());
    EXPECT_TRUE(transition->getTargets().empty());
}

// Transition type test
TEST_F(TransitionNodeTest, TransitionTypeTest)
{
    std::vector<std::string> targets = {"target1"};
    auto transition = std::make_unique<SCXML::Core::TransitionNode>("", "target1");
    
    // Test default type (external)
    EXPECT_FALSE(transition->isInternal());
    
    // Test internal type
    transition->setInternal(true);
    EXPECT_TRUE(transition->isInternal());
}