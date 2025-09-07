#include "CoreTestCommon.h"

// StateNode core functionality tests
class StateNodeTest : public CoreTestBase
{
};

// Basic StateNode creation test
TEST_F(StateNodeTest, BasicStateNodeCreation)
{
    // Create basic state node
    auto stateNode = std::make_unique<SCXML::Core::StateNode>("testState", SCXML::Type::COMPOUND);
    
    EXPECT_EQ("testState", stateNode->getId());
    EXPECT_EQ(SCXML::Type::COMPOUND, stateNode->getType());
    EXPECT_NE(SCXML::Type::FINAL, stateNode->getType());
    EXPECT_NE(SCXML::Type::PARALLEL, stateNode->getType());
    EXPECT_NE(SCXML::Type::HISTORY, stateNode->getType());
}

// Final state creation test
TEST_F(StateNodeTest, FinalStateCreation)
{
    auto finalState = std::make_unique<SCXML::Core::StateNode>("final", SCXML::Type::FINAL);
    
    EXPECT_EQ("final", finalState->getId());
    EXPECT_EQ(SCXML::Type::FINAL, finalState->getType());
    EXPECT_EQ(SCXML::Type::FINAL, finalState->getType());
    EXPECT_NE(SCXML::Type::PARALLEL, finalState->getType());
    EXPECT_NE(SCXML::Type::HISTORY, finalState->getType());
}

// Parallel state creation test
TEST_F(StateNodeTest, ParallelStateCreation)
{
    auto parallelState = std::make_unique<SCXML::Core::StateNode>("parallel", SCXML::Type::PARALLEL);
    
    EXPECT_EQ("parallel", parallelState->getId());
    EXPECT_EQ(SCXML::Type::PARALLEL, parallelState->getType());
    EXPECT_NE(SCXML::Type::FINAL, parallelState->getType());
    EXPECT_EQ(SCXML::Type::PARALLEL, parallelState->getType());
    EXPECT_NE(SCXML::Type::HISTORY, parallelState->getType());
}

// History state creation test
TEST_F(StateNodeTest, HistoryStateCreation)
{
    auto historyState = std::make_unique<SCXML::Core::StateNode>("history", SCXML::Type::HISTORY);
    
    EXPECT_EQ("history", historyState->getId());
    EXPECT_EQ(SCXML::Type::HISTORY, historyState->getType());
    EXPECT_NE(SCXML::Type::FINAL, historyState->getType());
    EXPECT_NE(SCXML::Type::PARALLEL, historyState->getType());
    EXPECT_EQ(SCXML::Type::HISTORY, historyState->getType());
}

// Child state management test
TEST_F(StateNodeTest, ChildStateManagement)
{
    auto parent = std::make_unique<SCXML::Core::StateNode>("parent", SCXML::Type::COMPOUND);
    auto child1 = std::make_unique<SCXML::Core::StateNode>("child1", SCXML::Type::COMPOUND);
    auto child2 = std::make_unique<SCXML::Core::StateNode>("child2", SCXML::Type::COMPOUND);
    
    // Add children
    parent->addChild(std::move(child1));
    parent->addChild(std::move(child2));
    
    // Check children count
    const auto& children = parent->getChildren();
    EXPECT_EQ(2, children.size());
    
    // Check children IDs
    std::vector<std::string> childIds;
    for (const auto& child : children) {
        childIds.push_back(child->getId());
    }
    
    EXPECT_TRUE(std::find(childIds.begin(), childIds.end(), "child1") != childIds.end());
    EXPECT_TRUE(std::find(childIds.begin(), childIds.end(), "child2") != childIds.end());
}

// Initial state setting test
TEST_F(StateNodeTest, InitialStateManagement)
{
    auto parent = std::make_unique<SCXML::Core::StateNode>("parent", SCXML::Type::COMPOUND);
    
    // Set initial state
    parent->setInitialState("child1");
    EXPECT_EQ("child1", parent->getInitialState());
    
    // Change initial state
    parent->setInitialState("child2");
    EXPECT_EQ("child2", parent->getInitialState());
}