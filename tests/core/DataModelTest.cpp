#include "CoreTestCommon.h"

// DataModel core functionality tests
class DataModelTest : public CoreTestBase
{
};

// Basic DataModelItem creation test
TEST_F(DataModelTest, BasicDataModelItemCreation)
{
    auto dataItem = std::make_unique<SCXML::Core::DataModelItem>("counter", "0");
    
    EXPECT_EQ("counter", dataItem->getId());
    EXPECT_EQ("0", dataItem->getExpr());
    EXPECT_TRUE(dataItem->getSrc().empty());
}

// DataModelItem with source test
TEST_F(DataModelTest, DataModelItemWithSource)
{
    auto dataItem = std::make_unique<SCXML::Core::DataModelItem>("config", "");
    dataItem->setSrc("config.json");
    
    EXPECT_EQ("config", dataItem->getId());
    EXPECT_TRUE(dataItem->getExpr().empty());
    EXPECT_EQ("config.json", dataItem->getSrc());
}

// DataModelItem with expression test
TEST_F(DataModelTest, DataModelItemWithExpression)
{
    auto dataItem = std::make_unique<SCXML::Core::DataModelItem>("user", "{ name: 'John', age: 30 }");
    
    EXPECT_EQ("user", dataItem->getId());
    EXPECT_EQ("{ name: 'John', age: 30 }", dataItem->getExpr());
    EXPECT_TRUE(dataItem->getSrc().empty());
}

// Multiple data items test
TEST_F(DataModelTest, MultipleDataItems)
{
    std::vector<std::unique_ptr<SCXML::Model::IDataModelItem>> dataModel;
    
    dataModel.push_back(std::make_unique<SCXML::Core::DataModelItem>("counter", "0"));
    dataModel.push_back(std::make_unique<SCXML::Core::DataModelItem>("message", "'Hello World'"));
    dataModel.push_back(std::make_unique<SCXML::Core::DataModelItem>("isActive", "true"));
    
    EXPECT_EQ(3, dataModel.size());
    
    EXPECT_EQ("counter", dataModel[0]->getId());
    EXPECT_EQ("0", dataModel[0]->getExpr());
    
    EXPECT_EQ("message", dataModel[1]->getId());
    EXPECT_EQ("'Hello World'", dataModel[1]->getExpr());
    
    EXPECT_EQ("isActive", dataModel[2]->getId());
    EXPECT_EQ("true", dataModel[2]->getExpr());
}

// Data item validation test
TEST_F(DataModelTest, DataItemValidation)
{
    // Valid data items
    auto validItem1 = std::make_unique<SCXML::Core::DataModelItem>("valid1", "123");
    auto validItem2 = std::make_unique<SCXML::Core::DataModelItem>("valid2", "");
    validItem2->setSrc("data.xml");
    
    EXPECT_FALSE(validItem1->getId().empty());
    EXPECT_FALSE(validItem1->getExpr().empty() && validItem1->getSrc().empty());
    
    EXPECT_FALSE(validItem2->getId().empty());
    EXPECT_FALSE(validItem2->getExpr().empty() && validItem2->getSrc().empty());
}

// Complex data model test
TEST_F(DataModelTest, ComplexDataModel)
{
    std::vector<std::unique_ptr<SCXML::Model::IDataModelItem>> dataModel;
    
    // Simple values
    dataModel.push_back(std::make_unique<SCXML::Core::DataModelItem>("step", "1"));
    dataModel.push_back(std::make_unique<SCXML::Core::DataModelItem>("total", "0"));
    
    // Complex objects
    dataModel.push_back(std::make_unique<SCXML::Core::DataModelItem>(
        "config", 
        "{ server: 'localhost', port: 8080, debug: true }"
    ));
    
    // Arrays
    dataModel.push_back(std::make_unique<SCXML::Core::DataModelItem>(
        "items", 
        "['item1', 'item2', 'item3']"
    ));
    
    // External data
    auto externalItem = std::make_unique<SCXML::Core::DataModelItem>("external", "");
    externalItem->setSrc("external.json");
    dataModel.push_back(std::move(externalItem));
    
    EXPECT_EQ(5, dataModel.size());
    
    // Verify all items have proper IDs
    for (const auto& item : dataModel) {
        EXPECT_FALSE(item->getId().empty());
        EXPECT_FALSE(item->getExpr().empty() && item->getSrc().empty());
    }
}