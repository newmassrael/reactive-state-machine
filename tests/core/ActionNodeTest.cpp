#include "CoreTestCommon.h"
#include "generator/include/core/actions/LogActionNode.h"
#include "generator/include/core/actions/AssignActionNode.h"
#include "generator/include/core/actions/RaiseActionNode.h"

// ActionNode core functionality tests
class ActionNodeTest : public CoreTestBase
{
};

// Basic LogAction test
TEST_F(ActionNodeTest, LogActionCreation)
{
    auto logAction = std::make_unique<SCXML::Core::LogActionNode>("log1");
    logAction->setExpr("'Hello World'");
    logAction->setLabel("test_log");
    
    EXPECT_EQ("log", logAction->getActionType());
    EXPECT_EQ("'Hello World'", logAction->getExpr());
    EXPECT_EQ("test_log", logAction->getLabel());
}

// Basic AssignAction test
TEST_F(ActionNodeTest, AssignActionCreation)
{
    auto assignAction = std::make_unique<SCXML::Core::AssignActionNode>("assign1");
    assignAction->setLocation("x");
    assignAction->setExpr("10");
    
    EXPECT_EQ("assign", assignAction->getActionType());
    EXPECT_EQ("x", assignAction->getLocation());
    EXPECT_EQ("10", assignAction->getExpr());
}

// Basic RaiseAction test  
TEST_F(ActionNodeTest, RaiseActionCreation)
{
    auto raiseAction = std::make_unique<SCXML::Core::RaiseActionNode>("raise1");
    raiseAction->setEvent("internal.event");
    
    EXPECT_EQ("raise", raiseAction->getActionType());
    EXPECT_EQ("internal.event", raiseAction->getEvent());
}

// Action polymorphism test
TEST_F(ActionNodeTest, ActionPolymorphism)
{
    std::vector<std::unique_ptr<SCXML::Model::IActionNode>> actions;
    
    auto logAction = std::make_unique<SCXML::Core::LogActionNode>("log1");
    logAction->setExpr("'Test message'");
    
    auto assignAction = std::make_unique<SCXML::Core::AssignActionNode>("assign1");
    assignAction->setLocation("counter");
    assignAction->setExpr("counter + 1");
    
    auto raiseAction = std::make_unique<SCXML::Core::RaiseActionNode>("raise1");
    raiseAction->setEvent("counter.updated");
    
    actions.push_back(std::move(logAction));
    actions.push_back(std::move(assignAction));
    actions.push_back(std::move(raiseAction));
    
    EXPECT_EQ(3, actions.size());
    EXPECT_EQ("log", actions[0]->getActionType());
    EXPECT_EQ("assign", actions[1]->getActionType());
    EXPECT_EQ("raise", actions[2]->getActionType());
}

// Action execution order test
TEST_F(ActionNodeTest, ActionExecutionOrder)
{
    auto assignAction = std::make_unique<SCXML::Core::AssignActionNode>("assign1");
    assignAction->setLocation("step");
    assignAction->setExpr("1");
    
    auto logAction = std::make_unique<SCXML::Core::LogActionNode>("log1");
    logAction->setExpr("'Step: ' + step");
    
    auto raiseAction = std::make_unique<SCXML::Core::RaiseActionNode>("raise1");
    raiseAction->setEvent("step.completed");
    
    // Verify actions can be sequenced
    std::vector<std::unique_ptr<SCXML::Model::IActionNode>> sequence;
    sequence.push_back(std::move(assignAction));
    sequence.push_back(std::move(logAction));
    sequence.push_back(std::move(raiseAction));
    
    EXPECT_EQ(3, sequence.size());
    
    // Verify sequence maintains order
    EXPECT_EQ("assign", sequence[0]->getActionType());
    EXPECT_EQ("log", sequence[1]->getActionType());
    EXPECT_EQ("raise", sequence[2]->getActionType());
}