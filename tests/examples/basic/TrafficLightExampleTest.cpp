#include "../ExampleTestCommon.h"

// Traffic light example tests
class TrafficLightExampleTest : public ExampleTestBase
{
};

// Test basic traffic light state machine
TEST_F(TrafficLightExampleTest, BasicTrafficLightCycle)
{
    std::string trafficLightSCXML = R"(<?xml version="1.0" encoding="UTF-8"?>
    <scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="red">
      <datamodel>
        <data id="timer" expr="0"/>
      </datamodel>
      
      <state id="red">
        <onentry>
          <assign location="timer" expr="30"/>
          <send event="timer" delay="30s"/>
        </onentry>
        <transition event="timer" target="green"/>
      </state>
      
      <state id="green">
        <onentry>
          <assign location="timer" expr="25"/>
          <send event="timer" delay="25s"/>
        </onentry>
        <transition event="timer" target="yellow"/>
      </state>
      
      <state id="yellow">
        <onentry>
          <assign location="timer" expr="5"/>
          <send event="timer" delay="5s"/>
        </onentry>
        <transition event="timer" target="red"/>
      </state>
    </scxml>)";
    
    auto model = parser->parseContent(trafficLightSCXML);
    
    ASSERT_TRUE(model != nullptr);
    EXPECT_FALSE(parser->hasErrors());
    EXPECT_EQ("red", model->getInitialState());
    
    // Verify all traffic light states exist
    auto redState = model->findStateById("red");
    auto greenState = model->findStateById("green");
    auto yellowState = model->findStateById("yellow");
    
    ASSERT_TRUE(redState != nullptr);
    ASSERT_TRUE(greenState != nullptr);
    ASSERT_TRUE(yellowState != nullptr);
    
    // Verify transitions form a cycle
    EXPECT_EQ(1, redState->getTransitions().size());
    EXPECT_EQ(1, greenState->getTransitions().size());
    EXPECT_EQ(1, yellowState->getTransitions().size());
    
    EXPECT_EQ("green", redState->getTransitions()[0]->getTargets()[0]);
    EXPECT_EQ("yellow", greenState->getTransitions()[0]->getTargets()[0]);
    EXPECT_EQ("red", yellowState->getTransitions()[0]->getTargets()[0]);
}