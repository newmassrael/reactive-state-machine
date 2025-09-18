#include <gtest/gtest.h>
#include "parsing/SCXMLParser.h"
#include "model/SCXMLModel.h"
#include "factory/NodeFactory.h"
#include <sstream>

namespace RSM {
namespace Tests {

class SCXMLParserBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        nodeFactory_ = std::make_shared<NodeFactory>();
        parser_ = std::make_unique<SCXMLParser>(nodeFactory_);
    }

    std::shared_ptr<NodeFactory> nodeFactory_;
    std::unique_ptr<SCXMLParser> parser_;
};

// Test basic SCXML document parsing
TEST_F(SCXMLParserBasicTest, ParseSimpleStateMachine) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
    <state id="start">
        <transition event="go" target="end"/>
    </state>
    <final id="end"/>
</scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_FALSE(parser_->hasErrors());
    
    // Verify basic model properties
    EXPECT_EQ(model->getInitialState(), "start");
}

// Test parser error handling
TEST_F(SCXMLParserBasicTest, ParseInvalidXML) {
    std::string invalidContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0">
    <state id="start"
        <transition event="go" target="end"/>
    </state>
</scxml>)";

    auto model = parser_->parseContent(invalidContent);
    EXPECT_EQ(model, nullptr);
    EXPECT_TRUE(parser_->hasErrors());
    
    auto errors = parser_->getErrorMessages();
    EXPECT_FALSE(errors.empty());
}

// Test state hierarchy parsing
TEST_F(SCXMLParserBasicTest, ParseNestedStates) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="parent">
    <state id="parent" initial="child1">
        <state id="child1">
            <transition event="next" target="child2"/>
        </state>
        <state id="child2">
            <transition event="done" target="end"/>
        </state>
    </state>
    <final id="end"/>
</scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_FALSE(parser_->hasErrors());
    
    // Verify nested structure - getAllStates() only returns top-level states
    auto states = model->getAllStates();
    EXPECT_GE(states.size(), 2); // parent, end (child states are nested within parent)
}

// Test action parsing
TEST_F(SCXMLParserBasicTest, ParseActionsInTransitions) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
    <state id="start">
        <transition event="go" target="end">
            <script>console.log('transitioning');</script>
            <assign location="result" expr="'success'"/>
        </transition>
    </state>
    <final id="end"/>
</scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_FALSE(parser_->hasErrors());
}

// Test guard conditions
TEST_F(SCXMLParserBasicTest, ParseGuardConditions) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
    <datamodel>
        <data id="counter" expr="0"/>
    </datamodel>
    <state id="start">
        <transition event="increment" cond="counter &lt; 10" target="start">
            <assign location="counter" expr="counter + 1"/>
        </transition>
        <transition event="increment" cond="counter >= 10" target="end"/>
    </state>
    <final id="end"/>
</scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_FALSE(parser_->hasErrors());
}

// Test data model parsing
TEST_F(SCXMLParserBasicTest, ParseDataModel) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
    <datamodel>
        <data id="name" expr="'test'"/>
        <data id="count" expr="42"/>
        <data id="flag" expr="true"/>
    </datamodel>
    <state id="start">
        <transition event="done" target="end"/>
    </state>
    <final id="end"/>
</scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_FALSE(parser_->hasErrors());
    
    // Verify data model items exist
    auto dataModelItems = model->getDataModelItems();
    EXPECT_GE(dataModelItems.size(), 3); // name, count, flag
}

// Test final states
TEST_F(SCXMLParserBasicTest, ParseFinalStates) {
    std::string scxmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
    <state id="start">
        <transition event="success" target="success_end"/>
        <transition event="failure" target="failure_end"/>
    </state>
    <final id="success_end">
        <donedata>
            <content expr="'completed successfully'"/>
        </donedata>
    </final>
    <final id="failure_end">
        <donedata>
            <content expr="'failed'"/>
        </donedata>
    </final>
</scxml>)";

    auto model = parser_->parseContent(scxmlContent);
    ASSERT_NE(model, nullptr);
    EXPECT_FALSE(parser_->hasErrors());
}

} // namespace Tests
} // namespace RSM