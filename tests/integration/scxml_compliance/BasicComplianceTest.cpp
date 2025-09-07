#include "../IntegrationTestCommon.h"

// Basic SCXML W3C compliance tests
class BasicComplianceTest : public IntegrationTestBase
{
};

// Test basic state machine structure compliance
TEST_F(BasicComplianceTest, BasicStateMachineStructure)
{
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="start">
      <state id="start">
        <transition event="go" target="end"/>
      </state>
      <final id="end"/>
    </scxml>)";
    
    auto model = parser->parseContent(scxml);
    
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());
    EXPECT_EQ("start", model->getInitialState());
    
    // Verify states exist
    auto startState = model->findStateById("start");
    auto endState = model->findStateById("end");
    
    ASSERT_TRUE(startState != nullptr);
    ASSERT_TRUE(endState != nullptr);
    EXPECT_EQ(SCXML::Type::FINAL, endState->getType());
}

// Test transition compliance
TEST_F(BasicComplianceTest, TransitionCompliance)
{
    std::string scxml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
      <state id="s1">
        <transition event="event1" target="s2" cond="x > 0"/>
        <transition event="event2" target="s3"/>
      </state>
      <state id="s2"/>
      <state id="s3"/>
    </scxml>)";
    
    auto model = parser->parseContent(scxml);
    
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());
    
    auto s1 = model->findStateById("s1");
    ASSERT_TRUE(s1 != nullptr);
    
    const auto& transitions = s1->getTransitions();
    EXPECT_EQ(2, transitions.size());
    
    // Verify conditional transition
    bool foundConditionalTransition = false;
    for (const auto& t : transitions) {
        if (t->getEvent() == "event1" && !t->getGuard().empty()) {
            foundConditionalTransition = true;
            EXPECT_EQ("x > 0", t->getGuard());
            EXPECT_EQ("s2", t->getTargets()[0]);
        }
    }
    EXPECT_TRUE(foundConditionalTransition);
}