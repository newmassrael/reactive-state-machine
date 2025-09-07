#include "../CoreTestCommon.h"
#include "generator/include/core/actions/LogActionNode.h"
#include "generator/include/core/actions/AssignActionNode.h"
#include "generator/include/core/actions/RaiseActionNode.h"

// Detailed action node integration tests
class ActionNodeIntegrationTest : public CoreTestBase
{
};

// Action sequence execution test
TEST_F(ActionNodeIntegrationTest, ActionSequenceExecution)
{
    std::vector<std::unique_ptr<SCXML::Model::IActionNode>> actionSequence;
    
    auto assignAction = std::make_unique<SCXML::Core::AssignActionNode>("assign1");
    assignAction->setLocation("counter");
    assignAction->setExpr("1");
    
    auto logAction = std::make_unique<SCXML::Core::LogActionNode>("log1");
    logAction->setExpr("'Counter initialized: ' + counter");
    
    auto raiseAction = std::make_unique<SCXML::Core::RaiseActionNode>("raise1");
    raiseAction->setEvent("counter.initialized");
    
    actionSequence.push_back(std::move(assignAction));
    actionSequence.push_back(std::move(logAction));
    actionSequence.push_back(std::move(raiseAction));
    
    EXPECT_EQ(3, actionSequence.size());
    EXPECT_EQ("assign", actionSequence[0]->getActionType());
    EXPECT_EQ("log", actionSequence[1]->getActionType());
    EXPECT_EQ("raise", actionSequence[2]->getActionType());
}