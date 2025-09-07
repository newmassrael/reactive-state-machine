#include "../CoreTestCommon.h"
#include "../../mocks/MockExecutionContext.h"
#include "core/HistoryNode.h"

// Simplified History Node test - testing basic functionality without the problematic HistoryNode class
class HistoryNodeTest : public CoreTestBase
{
protected:
    void SetUp() override
    {
        CoreTestBase::SetUp();
        mockContext = std::make_shared<MockExecutionContext>();
    }
    
    std::shared_ptr<MockExecutionContext> mockContext;
};;

// Basic construction test
TEST_F(HistoryNodeTest, BasicConstruction)
{
    // Test shallow history construction
    auto shallowHistory = std::make_shared<SCXML::Core::HistoryNode>("h1", SCXML::Model::HistoryType::SHALLOW);
    
    EXPECT_EQ("h1", shallowHistory->getId());
    EXPECT_EQ(SCXML::Model::HistoryType::SHALLOW, shallowHistory->getType());
    EXPECT_FALSE(shallowHistory->hasHistory(*mockContext));
    
    // Test deep history construction
    auto deepHistory = std::make_shared<SCXML::Core::HistoryNode>("h2", SCXML::Model::HistoryType::DEEP);
    
    EXPECT_EQ("h2", deepHistory->getId());
    EXPECT_EQ(SCXML::Model::HistoryType::DEEP, deepHistory->getType());
    EXPECT_FALSE(deepHistory->hasHistory(*mockContext));
}

// History Type utilities test
TEST_F(HistoryNodeTest, HistoryTypeUtilities)
{
    // Test type to string conversion
    EXPECT_EQ("shallow", SCXML::Model::IHistoryNode::historyTypeToString(SCXML::Model::HistoryType::SHALLOW));
    EXPECT_EQ("deep", SCXML::Model::IHistoryNode::historyTypeToString(SCXML::Model::HistoryType::DEEP));
    
    // Test string to type conversion
    EXPECT_EQ(SCXML::Model::HistoryType::SHALLOW, SCXML::Model::IHistoryNode::stringToHistoryType("shallow"));
    EXPECT_EQ(SCXML::Model::HistoryType::DEEP, SCXML::Model::IHistoryNode::stringToHistoryType("deep"));
}

// History state recording and retrieval test
TEST_F(HistoryNodeTest, HistoryRecordingAndRetrieval)
{
    auto history = std::make_shared<SCXML::Core::HistoryNode>("h1", SCXML::Model::HistoryType::SHALLOW);
    history->setParentState("parentState");
    history->setDefaultTarget("defaultState");
    
    // Set up mock context expectations
    EXPECT_CALL(*mockContext, setDataValue(testing::_, testing::_))
        .WillRepeatedly(testing::Return(SCXML::Common::Result<void>::success()));
    EXPECT_CALL(*mockContext, getDataValue(testing::_))
        .WillRepeatedly(testing::Return(SCXML::Common::Result<std::string>::success("state1,state2")));
    
    // Test recording history
    std::set<std::string> activeStates = {"state1", "state2"};
    auto recordResult = history->recordHistory(*mockContext, activeStates);
    EXPECT_TRUE(recordResult.isSuccess());
    
    // Test retrieving history
    auto retrieveResult = history->getStoredHistory(*mockContext);
    EXPECT_TRUE(retrieveResult.isSuccess());
}

// History validation test
TEST_F(HistoryNodeTest, HistoryValidation)
{
    auto history = std::make_shared<SCXML::Core::HistoryNode>("h1", SCXML::Model::HistoryType::SHALLOW);
    
    // Test validation with missing parent state
    auto errors = history->validate();
    EXPECT_FALSE(errors.empty()) << "Should have validation errors for missing parent state";
    
    // Test validation with proper setup
    history->setParentState("parentState");
    history->setDefaultTarget("defaultState");
    errors = history->validate();
    EXPECT_TRUE(errors.empty()) << "Should have no validation errors with proper setup";
}

// History clone test
TEST_F(HistoryNodeTest, HistoryClone)
{
    auto original = std::make_shared<SCXML::Core::HistoryNode>("h1", SCXML::Model::HistoryType::DEEP);
    original->setParentState("parentState");
    original->setDefaultTarget("defaultState");
    
    auto clone = original->clone();
    
    EXPECT_EQ(original->getId(), clone->getId());
    EXPECT_EQ(original->getType(), clone->getType());
    EXPECT_EQ(original->getParentState(), clone->getParentState());
    EXPECT_EQ(original->getDefaultTarget(), clone->getDefaultTarget());
    
    // Ensure it's a different object
    EXPECT_NE(original.get(), clone.get());
}