#include "impl/TXMLConverter.h"
#include "common/DisableStdOut.h"
#include <gtest/gtest.h>
#include <string>

using namespace RSM::W3C;

class TXMLConverterTest : public ::testing::Test {
protected:
    TXMLConverter converter;

    void SetUp() override {}

    void TearDown() override {}

    // Helper to create minimal valid SCXML with pass/fail targets
    std::string createValidTXML(const std::string &content) {
        return R"(<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" initial="test">
    <state id="test">)" +
               content + R"(
        <transition target="pass"/>
    </state>
    <final id="pass"/>
    <final id="fail"/>
</scxml>)";
    }
};

// ============================================================================
// W3C Test 207 Delay Bug Fix: conf:delay numeric to time unit conversion
// ============================================================================

// Test W3C Test 207: conf:delay numeric value conversion to proper time units
TEST_F(TXMLConverterTest, ConvertsNumericDelayToCSS2TimeFormat) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml initial="s0" version="1.0" conf:datamodel=""  xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<state id="s0">
  <invoke type="scxml">
    <content>
      <scxml initial="sub0" version="1.0" conf:datamodel=""  xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
        <state id="sub0">
          <onentry>
           <send event="event1" id="foo" conf:delay="1"/>
            <send event="event2" conf:delay="1.5"/>
            <send target="#_parent" event="childToParent"/>
          </onentry>
          <transition event="event1" target="subFinal">
            <send target="#_parent" event="pass"/>
          </transition>
          <transition event="*" target="subFinal">
            <send target="#_parent" event="fail"/>
          </transition>
        </state>
        <final id="subFinal"/>
      </scxml>
    </content>
  </invoke>
  <state id="s01">
    <transition event="childToParent" target="s02">
      <cancel sendid="foo"/>
    </transition>
  </state>
  <state id="s02">
    <transition event="pass" conf:targetpass=""/>
    <transition event="fail" conf:targetfail=""/>
  </state>
</state>
<conf:pass/>
<conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // **CRITICAL BUG FIX**: conf:delay="1" should convert to delay="1s" (CSS2 compliant)
    EXPECT_NE(result.find(R"(delay="1s")"), std::string::npos)
        << "conf:delay=\"1\" should convert to delay=\"1s\" (CSS2 time specification)";

    // **CRITICAL BUG FIX**: conf:delay="1.5" should convert to delay="1.5s" (CSS2 compliant)
    EXPECT_NE(result.find(R"(delay="1.5s")"), std::string::npos)
        << "conf:delay=\"1.5\" should convert to delay=\"1.5s\" (CSS2 time specification)";

    // **REGRESSION PREVENTION**: Verify unitless delay values are NOT generated (SCXML spec violation)
    EXPECT_EQ(result.find(R"(delay="1")"), std::string::npos)
        << "Should NOT generate delay=\"1\" (violates SCXML CSS2 time specification)";

    EXPECT_EQ(result.find(R"(delay="1.5")"), std::string::npos)
        << "Should NOT generate delay=\"1.5\" (violates SCXML CSS2 time specification)";

    // Verify other conversions work correctly
    EXPECT_NE(result.find(R"(datamodel="ecmascript")"), std::string::npos);
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos);
    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos);
    EXPECT_NE(result.find(R"(<final id="pass"/>)"), std::string::npos);
    EXPECT_NE(result.find(R"(<final id="fail"/>)"), std::string::npos);
    EXPECT_EQ(result.find("conf:"), std::string::npos);
}

// Test CSS2 time specification compliance for delay conversion
TEST_F(TXMLConverterTest, DelayConversionCSS2Compliance) {
    std::string txml = createValidTXML(R"(
        <onentry>
            <!-- Integer delay values (should become seconds) -->
            <send event="event1" conf:delay="2"/>
            <send event="event2" conf:delay="5"/>
            <!-- Decimal delay values (should become seconds) -->
            <send event="event3" conf:delay="0.5"/>
            <send event="event4" conf:delay="2.75"/>
            <!-- Already CSS2 compliant values should be preserved -->
            <send event="event5" conf:delay="1000ms"/>
            <send event="event6" conf:delay="3s"/>
        </onentry>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    // **CSS2 COMPLIANCE**: Integer seconds conversion
    EXPECT_NE(result.find(R"(delay="2s")"), std::string::npos)
        << "conf:delay=\"2\" should convert to CSS2 compliant delay=\"2s\"";
    EXPECT_NE(result.find(R"(delay="5s")"), std::string::npos)
        << "conf:delay=\"5\" should convert to CSS2 compliant delay=\"5s\"";

    // **CSS2 COMPLIANCE**: Decimal seconds conversion
    EXPECT_NE(result.find(R"(delay="0.5s")"), std::string::npos)
        << "conf:delay=\"0.5\" should convert to CSS2 compliant delay=\"0.5s\"";
    EXPECT_NE(result.find(R"(delay="2.75s")"), std::string::npos)
        << "conf:delay=\"2.75\" should convert to CSS2 compliant delay=\"2.75s\"";

    // **CSS2 COMPLIANCE**: Already compliant values preserved
    EXPECT_NE(result.find(R"(delay="1000ms")"), std::string::npos)
        << "CSS2 compliant conf:delay=\"1000ms\" should be preserved";
    EXPECT_NE(result.find(R"(delay="3s")"), std::string::npos)
        << "CSS2 compliant conf:delay=\"3s\" should be preserved";

    // **REGRESSION PREVENTION**: No unitless delay values
    EXPECT_EQ(result.find(R"(delay="2">)"), std::string::npos)
        << "Should NOT generate unitless delay values (CSS2 violation)";
    EXPECT_EQ(result.find(R"(delay="0.5">)"), std::string::npos)
        << "Should NOT generate unitless delay values (CSS2 violation)";

    EXPECT_EQ(result.find("conf:"), std::string::npos);
}

// ============================================================================
// Basic Namespace and Structure Tests
// ============================================================================

// Test basic namespace removal
TEST_F(TXMLConverterTest, RemovesConfNamespace) {
    std::string txml = createValidTXML("");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_EQ(result.find("xmlns:conf="), std::string::npos) << "conf namespace should be removed";
    EXPECT_NE(result.find("<scxml xmlns="), std::string::npos) << "main scxml namespace should remain";
}

// Test conf:datamodel attribute conversion
TEST_F(TXMLConverterTest, ConvertsDatamodelAttribute) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" conf:datamodel="" initial="test">
    <state id="test">
        <transition target="pass"/>
    </state>
    <final id="pass"/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(datamodel="ecmascript")"), std::string::npos)
        << "conf:datamodel should convert to datamodel=\"ecmascript\"";
    EXPECT_EQ(result.find("conf:datamodel"), std::string::npos) << "conf:datamodel should be removed";
}

// ============================================================================
// Variable Binding and Expression Tests (conf:isBound)
// ============================================================================

// Test conf:isBound attribute conversion: 숫자는 var prefix 추가, 일반 변수명은 그대로
TEST_F(TXMLConverterTest, ConvertsIsBoundToTypeofCondition) {
    std::string txml = createValidTXML(R"(
        <transition conf:isBound="4" target="pass"/>
        <transition conf:isBound="variable_x" target="fail"/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(cond="typeof var4 !== 'undefined'")"), std::string::npos)
        << "conf:isBound with number should convert to typeof var[number] condition";
    EXPECT_NE(result.find(R"(cond="typeof variable_x !== 'undefined'")"), std::string::npos)
        << "conf:isBound with variable should convert properly";
    EXPECT_EQ(result.find("conf:isBound"), std::string::npos) << "conf:isBound attributes should be removed";
}

// Test variable binding with special characters
TEST_F(TXMLConverterTest, HandlesVariableNamesWithSpecialCharacters) {
    std::string txml = createValidTXML(R"(
        <transition conf:isBound="var_with_underscore" target="pass"/>
        <transition conf:isBound="123" target="fail"/>
        <transition conf:isBound="$special" target="pass"/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(cond="typeof var_with_underscore !== 'undefined'")"), std::string::npos);
    EXPECT_NE(result.find(R"(cond="typeof var123 !== 'undefined'")"), std::string::npos);
    EXPECT_NE(result.find(R"(cond="typeof $special !== 'undefined'")"), std::string::npos);
}

// Test multiple isBound conditions in complex expressions
TEST_F(TXMLConverterTest, HandlesMultipleVariableBindings) {
    std::string txml = createValidTXML(R"(
        <transition conf:isBound="firstVar" target="intermediate"/>
        <transition conf:isBound="secondVar" target="pass"/>
        <transition conf:isBound="thirdVar" target="fail"/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(cond="typeof firstVar !== 'undefined'")"), std::string::npos);
    EXPECT_NE(result.find(R"(cond="typeof secondVar !== 'undefined'")"), std::string::npos);
    EXPECT_NE(result.find(R"(cond="typeof thirdVar !== 'undefined'")"), std::string::npos);
}

// ============================================================================
// Target Attribute Tests (conf:targetpass/conf:targetfail)
// ============================================================================

// Test conf:targetpass/targetfail conversion
TEST_F(TXMLConverterTest, ConvertsConfTargetAttributes) {
    std::string txml = createValidTXML(R"(
        <transition conf:targetpass="" event="pass"/>
        <transition conf:targetfail="" event="fail"/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos)
        << "conf:targetpass should convert to target=\"pass\"";
    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos)
        << "conf:targetfail should convert to target=\"fail\"";
    EXPECT_EQ(result.find("conf:targetpass"), std::string::npos) << "conf:targetpass should be removed";
    EXPECT_EQ(result.find("conf:targetfail"), std::string::npos) << "conf:targetfail should be removed";
}

// ============================================================================
// Element Conversion Tests (conf:pass/conf:fail)
// ============================================================================

// Test conf:pass/fail element conversion
TEST_F(TXMLConverterTest, ConvertsConfElements) {
    std::string txml = createValidTXML(R"(
        <conf:pass/>
        <conf:fail/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(<final id="pass"/>)"), std::string::npos)
        << "conf:pass should convert to final id=\"pass\"";
    EXPECT_NE(result.find(R"(<final id="fail"/>)"), std::string::npos)
        << "conf:fail should convert to final id=\"fail\"";
    EXPECT_EQ(result.find("conf:pass"), std::string::npos) << "conf:pass elements should be removed";
    EXPECT_EQ(result.find("conf:fail"), std::string::npos) << "conf:fail elements should be removed";
}

// ============================================================================
// Cleanup and Removal Tests
// ============================================================================

// Test multiple conf: attribute removal
TEST_F(TXMLConverterTest, RemovesAllConfAttributes) {
    std::string txml = createValidTXML(R"(
        <transition conf:customAttr="value1" conf:anotherAttr="value2" target="pass"/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_EQ(result.find("conf:customAttr"), std::string::npos) << "All conf: attributes should be removed";
    EXPECT_EQ(result.find("conf:anotherAttr"), std::string::npos) << "All conf: attributes should be removed";
}

// Test conf: element removal
TEST_F(TXMLConverterTest, RemovesAllConfElements) {
    std::string txml = createValidTXML(R"(
        <conf:customElement>content</conf:customElement>
        <conf:anotherElement/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_EQ(result.find("conf:customElement"), std::string::npos) << "All conf: elements should be removed";
    EXPECT_EQ(result.find("conf:anotherElement"), std::string::npos) << "All conf: elements should be removed";
}

// ============================================================================
// Complex Integration Tests
// ============================================================================

// Test mixed conf: namespace conversions
TEST_F(TXMLConverterTest, HandlesMixedConfReferences) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" conf:datamodel="" initial="test">
    <state id="test">
        <transition conf:isBound="myVar" conf:targetpass=""/>
        <transition conf:targetfail=""/>
        <conf:customElement attr="value"/>
    </state>
    <conf:pass/>
    <conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(datamodel="ecmascript")"), std::string::npos);
    EXPECT_NE(result.find(R"(cond="typeof myVar !== 'undefined'")"), std::string::npos);
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos);
    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos);
    EXPECT_NE(result.find(R"(<final id="pass"/>)"), std::string::npos);
    EXPECT_NE(result.find(R"(<final id="fail"/>)"), std::string::npos);
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// Test comprehensive TXML to SCXML conversion with foreach and variable binding
TEST_F(TXMLConverterTest, ConvertsComplexForeachPattern) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" conf:datamodel="" initial="s0">
    <datamodel>
        <data id="Var1" expr="0"/>
    </datamodel>
    <state id="s0">
        <onentry>
            <foreach array="Var1" item="Var2" index="Var3">
                <assign location="Var4" expr="0"/>
            </foreach>
        </onentry>
        <transition conf:isBound="4" conf:targetpass=""/>
        <transition conf:targetfail=""/>
    </state>
    <conf:pass/>
    <conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // Check main conversions
    EXPECT_NE(result.find(R"(datamodel="ecmascript")"), std::string::npos) << "Should have ECMAScript datamodel";
    EXPECT_NE(result.find(R"(cond="typeof var4 !== 'undefined'")"), std::string::npos)
        << "Should convert conf:isBound to typeof condition";
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos) << "Should have pass target";
    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos) << "Should have fail target";
    EXPECT_NE(result.find(R"(<final id="pass"/>)"), std::string::npos) << "Should have pass final state";
    EXPECT_NE(result.find(R"(<final id="fail"/>)"), std::string::npos) << "Should have fail final state";

    // Check all conf: references removed
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// Test nested SCXML structures with conf: references
TEST_F(TXMLConverterTest, HandlesNestedStatesWithConfReferences) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" initial="outer">
    <state id="outer" initial="inner">
        <state id="inner">
            <transition conf:isBound="nestedVar" conf:targetpass=""/>
            <onentry>
                <conf:customAction/>
            </onentry>
        </state>
        <conf:pass/>
    </state>
    <final id="pass"/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(cond="typeof nestedVar !== 'undefined'")"), std::string::npos);
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos);
    EXPECT_EQ(result.find("conf:customAction"), std::string::npos);
    EXPECT_EQ(result.find("conf:pass"), std::string::npos);
}

// ============================================================================
// W3C Compliance and Edge Case Tests
// ============================================================================

// Test W3C compliance validation - comments preservation
TEST_F(TXMLConverterTest, PreservesCommentsWithConfReferences) {
    std::string txml = createValidTXML(R"(
        <!-- This comment mentions conf: namespace but should be preserved -->
        <transition target="pass"/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find("<!-- This comment mentions conf: namespace"), std::string::npos)
        << "Comments should be preserved even if they contain conf: references";
}

// Test portable test pattern with success/failure states
TEST_F(TXMLConverterTest, HandlesPortableTestPattern) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" conf:datamodel="" initial="start">
    <state id="start">
        <transition event="go" target="check"/>
    </state>
    <state id="check">
        <transition conf:isBound="result" conf:targetpass=""/>
        <transition conf:targetfail=""/>
    </state>
    <conf:pass/>
    <conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(<final id="pass"/>)"), std::string::npos) << "Should have success state";
    EXPECT_NE(result.find(R"(<final id="fail"/>)"), std::string::npos) << "Should have failure state";
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos) << "Should route to success on condition";
    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos) << "Should route to failure otherwise";
}

// ============================================================================
// W3C IRP Extended Attributes Tests (Timing, Error Handling, Data Processing)
// ============================================================================

// Test conf:delay attribute conversion for timing
TEST_F(TXMLConverterTest, ConvertsDelayAttribute) {
    std::string txml = createValidTXML(R"abc(
        <send conf:delay="5s" event="timeout" target="self"/>
    )abc");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"xyz(delay="5s")xyz"), std::string::npos)
        << "conf:delay should be converted to delay attribute";
    EXPECT_EQ(result.find(R"abc(conf:delay)abc"), std::string::npos) << "conf:delay references should be removed";
}

// Test conf:invalidLocation attribute conversion for error handling
TEST_F(TXMLConverterTest, ConvertsInvalidLocationAttribute) {
    std::string txml = createValidTXML(R"def(
        <assign conf:invalidLocation="invalidVar" expr="123"/>
    )def");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"ghi(location="invalidVar")ghi"), std::string::npos)
        << "conf:invalidLocation should be converted to location attribute";
    EXPECT_EQ(result.find(R"jkl(conf:invalidLocation)jkl"), std::string::npos)
        << "conf:invalidLocation references should be removed";
}

// Test conf:invalidNamelist attribute conversion for validation
TEST_F(TXMLConverterTest, ConvertsInvalidNamelistAttribute) {
    std::string txml = createValidTXML(R"mno(
        <send conf:invalidNamelist="var1 var2" event="data" target="self"/>
    )mno");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"pqr(namelist="var1 var2")pqr"), std::string::npos)
        << "conf:invalidNamelist should be converted to namelist attribute";
    EXPECT_EQ(result.find(R"stu(conf:invalidNamelist)stu"), std::string::npos)
        << "conf:invalidNamelist references should be removed";
}

// Test conf:someInlineVal attribute removal (test framework placeholder)
TEST_F(TXMLConverterTest, RemovesSomeInlineValAttribute) {
    std::string txml = createValidTXML(R"vwx(
        <log conf:someInlineVal="someValue + 42" expr="'log message'"/>
    )vwx");

    std::string result = converter.convertTXMLToSCXML(txml);

    // conf:someInlineVal is a test framework placeholder that should be removed
    EXPECT_EQ(result.find("conf:someInlineVal"), std::string::npos)
        << "conf:someInlineVal references should be removed";
    EXPECT_NE(result.find(R"abc(expr="'log message'")abc"), std::string::npos)
        << "Original expr attribute should be preserved";
    EXPECT_NE(result.find("<log"), std::string::npos) << "Log element should be preserved";
}

// Test conf:eventdataSomeVal attribute conversion for event data processing
TEST_F(TXMLConverterTest, ConvertsEventdataSomeValAttribute) {
    std::string txml = createValidTXML(R"567(
        <param conf:eventdataSomeVal="eventParam" expr="paramValue"/>
    )567");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"890(name="eventParam")890"), std::string::npos)
        << "conf:eventdataSomeVal should be converted to name attribute";
    EXPECT_EQ(result.find(R"abc(conf:eventdataSomeVal)abc"), std::string::npos)
        << "conf:eventdataSomeVal references should be removed";
}

// Test edge case: Empty attribute values for W3C IRP attributes
TEST_F(TXMLConverterTest, HandlesEmptyW3CIRPAttributeValues) {
    std::string txml = createValidTXML(R"abc(
        <send conf:delay="" event="empty" target="self"/>
        <assign conf:invalidLocation="" expr="42"/>
        <log conf:someInlineVal="" expr="'test'"/>
    )abc");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"def(delay="")def"), std::string::npos)
        << "Empty conf:delay should be converted to empty delay";
    EXPECT_NE(result.find(R"ghi(location="")ghi"), std::string::npos)
        << "Empty conf:invalidLocation should be converted to empty location";
    // conf:someInlineVal should be removed (test framework placeholder)
    EXPECT_NE(result.find(R"jkl(expr="'test'")jkl"), std::string::npos)
        << "Original expr attribute should be preserved";
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// Test W3C IRP attributes with special characters and escaping
TEST_F(TXMLConverterTest, HandlesW3CIRPAttributesWithSpecialCharacters) {
    std::string txml = createValidTXML(R"mno(
        <send conf:delay="'2s'" event="quoted" target="self"/>
        <assign conf:invalidLocation="var.with.dots" expr="123"/>
        <log conf:someInlineVal="value &amp; more" expr="'test'"/>
    )mno");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"pqr(delay="'2s'")pqr"), std::string::npos) << "conf:delay with quotes should be preserved";
    EXPECT_NE(result.find(R"stu(location="var.with.dots")stu"), std::string::npos)
        << "conf:invalidLocation with dots should be preserved";
    // conf:someInlineVal should be removed (test framework placeholder)
    EXPECT_NE(result.find(R"vwx(expr="'test'")vwx"), std::string::npos)
        << "Original expr attribute should be preserved";
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// Test conf:eventNamedParamHasValue attribute conversion for event parameter validation
TEST_F(TXMLConverterTest, ConvertsEventNamedParamHasValueAttribute) {
    std::string txml = createValidTXML(R"xyz(
        <if conf:eventNamedParamHasValue="event.data.param">
            <transition target="pass"/>
        </if>
    )xyz");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"abc(expr="event.data.param")abc"), std::string::npos)
        << "conf:eventNamedParamHasValue should be converted to expr attribute";
    EXPECT_EQ(result.find("conf:eventNamedParamHasValue"), std::string::npos)
        << "conf:eventNamedParamHasValue references should be removed";
    EXPECT_NE(result.find("<if"), std::string::npos) << "If element should be preserved";
}

// Test comprehensive W3C IRP attributes including eventNamedParamHasValue
TEST_F(TXMLConverterTest, ConvertsAllW3CIRPAttributesComprehensive) {
    std::string txml = R"def(<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" initial="test">
    <state id="test">
        <onentry>
            <assign conf:invalidLocation="testVar" expr="42"/>
            <send conf:delay="1s" conf:invalidNamelist="var1 var2" event="timer" target="self"/>
        </onentry>
        <transition event="timer" target="checkParam">
            <if conf:eventNamedParamHasValue="event.data.hasParam">
                <transition target="pass"/>
            </if>
            <else>
                <transition target="fail"/>
            </else>
        </transition>
        <transition event="result" target="pass">
            <send event="complete" target="external">
                <param conf:eventdataSomeVal="resultParam" expr="testVar"/>
            </send>
        </transition>
    </state>
    <state id="checkParam"/>
    <final id="pass"/>
    <final id="fail"/>
</scxml>)def";

    std::string result = converter.convertTXMLToSCXML(txml);

    // Verify all W3C IRP attributes are correctly converted
    EXPECT_NE(result.find(R"ghi(location="testVar")ghi"), std::string::npos)
        << "conf:invalidLocation should be converted to location";
    EXPECT_NE(result.find(R"jkl(delay="1s")jkl"), std::string::npos) << "conf:delay should be converted to delay";
    EXPECT_NE(result.find(R"mno(namelist="var1 var2")mno"), std::string::npos)
        << "conf:invalidNamelist should be converted to namelist";
    EXPECT_NE(result.find(R"pqr(expr="event.data.hasParam")pqr"), std::string::npos)
        << "conf:eventNamedParamHasValue should be converted to expr";
    EXPECT_NE(result.find(R"stu(name="resultParam")stu"), std::string::npos)
        << "conf:eventdataSomeVal should be converted to name";

    // Verify XML structure integrity
    EXPECT_NE(result.find("<if expr="), std::string::npos) << "If element with expr should be properly formed";
    EXPECT_NE(result.find("<else>"), std::string::npos) << "Else element should be preserved";
    EXPECT_NE(result.find("<param name="), std::string::npos) << "Param element with name should be properly formed";

    // Verify complete cleanup
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be completely removed";
}

// Test W3C IRP edge cases and error conditions
TEST_F(TXMLConverterTest, HandlesW3CIRPEdgeCases) {
    std::string txml = createValidTXML(R"vwx(
        <send conf:delay="" conf:invalidNamelist="" event="test" target="self"/>
        <if conf:eventNamedParamHasValue="">
            <transition target="pass"/>
        </if>
        <param conf:eventdataSomeVal="" expr="value"/>
        <assign conf:invalidLocation="" expr="null"/>
    )vwx");

    std::string result = converter.convertTXMLToSCXML(txml);

    // Check empty value handling
    EXPECT_NE(result.find(R"yz1(delay="")yz1"), std::string::npos) << "Empty conf:delay should convert to empty delay";
    EXPECT_NE(result.find(R"234(namelist="")234"), std::string::npos)
        << "Empty conf:invalidNamelist should convert to empty namelist";
    EXPECT_NE(result.find(R"567(expr="")567"), std::string::npos)
        << "Empty conf:eventNamedParamHasValue should convert to empty expr";
    EXPECT_NE(result.find(R"890(name="")890"), std::string::npos)
        << "Empty conf:eventdataSomeVal should convert to empty name";
    EXPECT_NE(result.find(R"abc(location="")abc"), std::string::npos)
        << "Empty conf:invalidLocation should convert to empty location";

    // Verify cleanup
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// ============================================================================
// Error Handling and Validation Tests
// ============================================================================

// Test empty content validation
TEST_F(TXMLConverterTest, ThrowsOnEmptyContent) {
    EXPECT_THROW(converter.convertTXMLToSCXML(""), std::invalid_argument)
        << "Empty content should throw invalid_argument";
}

// Test invalid SCXML validation
TEST_F(TXMLConverterTest, ThrowsOnInvalidSCXML) {
    std::string invalid_txml = R"(<invalid>not scxml</invalid>)";

    EXPECT_THROW(converter.convertTXMLToSCXML(invalid_txml), std::runtime_error)
        << "Invalid SCXML should throw runtime_error";
}

// Test that pass/fail targets are required
TEST_F(TXMLConverterTest, RequiresPassOrFailTargets) {
    std::string txml_no_targets = R"(<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" initial="test">
    <state id="test"/>
</scxml>)";

    EXPECT_THROW(converter.convertTXMLToSCXML(txml_no_targets), std::runtime_error)
        << "SCXML without pass/fail targets should throw runtime_error";
}

// Test conversion preserves SCXML structure
TEST_F(TXMLConverterTest, PreservesSCXMLStructure) {
    std::string txml = createValidTXML(R"(
        <onentry>
            <log expr="'entering test state'"/>
        </onentry>
        <onexit>
            <log expr="'exiting test state'"/>
        </onexit>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find("<onentry>"), std::string::npos) << "Should preserve onentry elements";
    EXPECT_NE(result.find("<onexit>"), std::string::npos) << "Should preserve onexit elements";
    EXPECT_NE(result.find("<log"), std::string::npos) << "Should preserve log elements";
}

// ============================================================================
// Test 150 Specific: Foreach Conf Attributes Conversion
// ============================================================================

// Test exact Test 150 foreach conf: attributes conversion pattern
TEST_F(TXMLConverterTest, ConvertsForeachConfAttributesWithNumericVariables) {
    // This is the exact pattern from Test 150 that was failing
    std::string txml = R"(<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" initial="test">
    <state id="test">
        <onentry>
            <foreach conf:item="4" conf:index="5" conf:arrayVar="3">
                <assign location="tempVar" expr="item + index"/>
            </foreach>
        </onentry>
        <transition target="pass"/>
    </state>
    <final id="pass"/>
    <final id="fail"/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // Critical validation: conf:item="4" -> item="var4" (numeric variables need var prefix)
    EXPECT_NE(result.find(R"(item="var4")"), std::string::npos)
        << "conf:item with numeric value should convert to item attribute with var prefix";

    // Critical validation: conf:index="5" -> index="var5" (numeric variables need var prefix)
    EXPECT_NE(result.find(R"(index="var5")"), std::string::npos)
        << "conf:index with numeric value should convert to index attribute with var prefix";

    // Critical validation: conf:arrayVar="3" -> array="var3" (numeric variables need var prefix)
    EXPECT_NE(result.find(R"(array="var3")"), std::string::npos) << "conf:arrayVar should convert to array attribute";

    // Verify complete foreach element structure with var prefixes for numeric values
    EXPECT_NE(result.find(R"(<foreach item="var4" index="var5" array="var3">)"), std::string::npos)
        << "Complete foreach element should have all converted attributes with var prefix for numeric values";

    // Verify no conf: attributes remain
    EXPECT_EQ(result.find("conf:item"), std::string::npos) << "conf:item should be completely removed";
    EXPECT_EQ(result.find("conf:index"), std::string::npos) << "conf:index should be completely removed";
    EXPECT_EQ(result.find("conf:arrayVar"), std::string::npos) << "conf:arrayVar should be completely removed";

    // Verify no conf: references remain anywhere
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// Test foreach with mixed conf: and regular attributes
TEST_F(TXMLConverterTest, ConvertsForeachMixedAttributes) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" initial="test">
    <state id="test">
        <onentry>
            <foreach conf:item="varItem" conf:index="varIndex" conf:arrayVar="myArray" id="foreachLoop">
                <log expr="'Processing item: ' + item"/>
            </foreach>
        </onentry>
        <transition target="pass"/>
    </state>
    <final id="pass"/>
    <final id="fail"/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // Verify conf: attributes are converted
    EXPECT_NE(result.find(R"(item="varItem")"), std::string::npos);
    EXPECT_NE(result.find(R"(index="varIndex")"), std::string::npos);
    EXPECT_NE(result.find(R"(array="myArray")"), std::string::npos);

    // Verify regular attributes are preserved
    EXPECT_NE(result.find(R"(id="foreachLoop")"), std::string::npos);

    // Verify no conf: remains
    EXPECT_EQ(result.find("conf:"), std::string::npos);
}

// **리그레이션 방지 테스트**: JavaScript 문법 유효성 검증
TEST_F(TXMLConverterTest, ValidatesJavaScriptSyntaxForNumericVariables) {
    std::string txml = R"abc(<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" initial="test">
    <state id="test">
        <!-- Test numeric variable names in isBound -->
        <transition conf:isBound="4" target="pass"/>
        <transition conf:isBound="123" target="pass"/>
        <transition conf:isBound="variableName" target="pass"/>
    </state>
    <final id="pass"/>
</scxml>)abc";

    std::string result = converter.convertTXMLToSCXML(txml);

    // **핵심 검증**: 숫자 변수명도 올바른 JavaScript 구문으로 변환되어야 함
    EXPECT_NE(result.find(R"abc(cond="typeof var4 !== 'undefined'")abc"), std::string::npos)
        << "conf:isBound=\"4\" should convert to valid JavaScript: typeof var4 !== 'undefined'";

    EXPECT_NE(result.find(R"abc(cond="typeof var123 !== 'undefined'")abc"), std::string::npos)
        << "conf:isBound=\"123\" should convert to valid JavaScript: typeof var123 !== 'undefined'";

    EXPECT_NE(result.find(R"abc(cond="typeof variableName !== 'undefined'")abc"), std::string::npos)
        << "conf:isBound=\"variableName\" should convert to valid JavaScript: typeof variableName !== 'undefined'";

    // **리그레이션 방지**: 잘못된 JavaScript 구문이 생성되지 않았는지 확인
    EXPECT_EQ(result.find("typeof 4 !== 'undefined'"), std::string::npos)
        << "Should NOT generate invalid JavaScript: typeof 4 !== 'undefined'";

    EXPECT_EQ(result.find("typeof 123 !== 'undefined'"), std::string::npos)
        << "Should NOT generate invalid JavaScript: typeof 123 !== 'undefined'";

    // Verify no conf: remains
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// **단위 테스트**: conf 배열 요소 변환 검증
TEST_F(TXMLConverterTest, ConvertsConfArrayElementsInDataModel) {
    std::string txml = R"abc(<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" initial="test">
<datamodel>
  <data id="testArray1">
    <conf:array123/>
  </data>
  <data id="testArray2">
    <conf:array456/>
  </data>
  <data id="emptyData"/>
</datamodel>
<state id="test">
  <transition target="pass"/>
</state>
<final id="pass"/>
</scxml>)abc";

    std::string result = converter.convertTXMLToSCXML(txml);

    // **핵심 검증**: conf:array123이 JavaScript 배열로 변환되는지 확인
    EXPECT_NE(result.find(R"([1,2,3])"), std::string::npos) << "<conf:array123/> should convert to [1,2,3]";

    EXPECT_NE(result.find(R"([4,5,6])"), std::string::npos) << "<conf:array456/> should convert to [4,5,6]";

    // **검증**: conf 요소가 완전히 제거되었는지 확인
    EXPECT_EQ(result.find("<conf:array123/>"), std::string::npos)
        << "conf:array123 element should be completely removed";

    EXPECT_EQ(result.find("<conf:array456/>"), std::string::npos)
        << "conf:array456 element should be completely removed";

    // **검증**: 다른 요소는 영향받지 않는지 확인
    EXPECT_NE(result.find(R"(id="emptyData")"), std::string::npos) << "Other data elements should remain unchanged";

    // **최종 검증**: 모든 conf 참조가 제거되었는지 확인
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// **통합 테스트**: 완전한 W3C 테스트 패턴 변환 검증
TEST_F(TXMLConverterTest, ConvertsCompleteW3CTestPattern) {
    // 실제 W3C 테스트 150의 완전한 패턴 (conf:array123 포함)
    std::string txml = R"abc(<?xml version="1.0"?>
<scxml initial="s0" conf:datamodel="" version="1.0" xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1"/>
  <data conf:id="2"/>
  <data conf:id="3">
    <conf:array123/>
  </data>
</datamodel>
<state id="s0">
  <onentry>
    <foreach conf:item="1" conf:index="2" conf:arrayVar="3"/>
  </onentry>
  <transition event="*" target="s1"/>
</state>
<state id="s1">
  <onentry>
    <foreach conf:item="4" conf:index="5" conf:arrayVar="3"/>
  </onentry>
  <transition event="*" target="s2"/>
</state>
<state id="s2">
  <transition conf:isBound="4" conf:targetpass=""/>
  <transition conf:targetfail=""/>
</state>
<conf:pass/>
<conf:fail/>
</scxml>)abc";

    std::string result = converter.convertTXMLToSCXML(txml);

    // **핵심 검증**: 데이터모델이 올바르게 변환되는지 확인
    EXPECT_NE(result.find(R"abc(datamodel="ecmascript")abc"), std::string::npos)
        << "conf:datamodel should convert to datamodel=\"ecmascript\"";

    EXPECT_NE(result.find(R"abc(id="var1")abc"), std::string::npos) << "conf:id=\"1\" should convert to id=\"var1\"";

    EXPECT_NE(result.find(R"abc(id="var3")abc"), std::string::npos) << "conf:id=\"3\" should convert to id=\"var3\"";

    // **핵심 검증**: conf:array123이 JavaScript 배열로 변환되는지 확인
    EXPECT_NE(result.find(R"([1,2,3])"), std::string::npos)
        << "<conf:array123/> should convert to [1,2,3] inside data element";

    // **검증**: foreach 속성들이 올바르게 변환되는지 확인
    EXPECT_NE(result.find(R"abc(item="var4")abc"), std::string::npos)
        << "conf:item=\"4\" should convert to item=\"var4\" for valid JavaScript variable name";

    EXPECT_NE(result.find(R"abc(index="var5")abc"), std::string::npos)
        << "conf:index=\"5\" should convert to index=\"var5\" for valid JavaScript variable name";

    EXPECT_NE(result.find(R"abc(array="var3")abc"), std::string::npos)
        << "conf:arrayVar=\"3\" should convert to array=\"var3\" for valid JavaScript variable name";

    // **검증**: isBound가 올바른 JavaScript 구문으로 변환되는지 확인
    EXPECT_NE(result.find(R"abc(cond="typeof var4 !== 'undefined'")abc"), std::string::npos)
        << "conf:isBound=\"4\" should convert to valid JavaScript: typeof var4 !== 'undefined'";

    // **검증**: pass/fail 타겟이 올바르게 변환되는지 확인
    EXPECT_NE(result.find(R"abc(target="pass")abc"), std::string::npos)
        << "conf:targetpass should convert to target=\"pass\"";

    EXPECT_NE(result.find(R"abc(target="fail")abc"), std::string::npos)
        << "conf:targetfail should convert to target=\"fail\"";

    EXPECT_NE(result.find(R"abc(<final id="pass"/>)abc"), std::string::npos)
        << "conf:pass should convert to final id=\"pass\"";

    EXPECT_NE(result.find(R"abc(<final id="fail"/>)abc"), std::string::npos)
        << "conf:fail should convert to final id=\"fail\"";

    // **리그레이션 방지**: 잘못된 JavaScript 구문이 생성되지 않았는지 확인
    EXPECT_EQ(result.find("typeof 4 !== 'undefined'"), std::string::npos)
        << "Should NOT generate invalid JavaScript: typeof 4 !== 'undefined'";

    EXPECT_EQ(result.find("typeof 1 !== 'undefined'"), std::string::npos)
        << "Should NOT generate invalid JavaScript: typeof 1 !== 'undefined'";

    // **최종 검증**: 모든 conf 참조가 제거되었는지 확인
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// ============================================================================
// Advanced TXML Attribute Conversion Tests: Conditions and Expressions
// ============================================================================

// Test conf:compareIDVal attribute conversion for comparison expressions
TEST_F(TXMLConverterTest, ConvertsComparisonExpressions) {
    std::string txml = createValidTXML(R"(
        <if conf:compareIDVal="1&lt;2">
            <assign conf:location="1" conf:varExpr="2"/>
        </if>
        <if conf:compareIDVal="3&gt;=4">
            <assign conf:location="3" conf:expr="0"/>
        </if>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    // Critical validation: conf:compareIDVal="1&lt;2" -> cond="var1 &lt; var2"
    EXPECT_NE(result.find(R"(cond="var1 &lt; var2")"), std::string::npos)
        << "conf:compareIDVal=\"1&lt;2\" should convert to cond=\"var1 &lt; var2\"";

    // Critical validation: conf:compareIDVal="3&gt;=4" -> cond="var3 &gt;= var4"
    EXPECT_NE(result.find(R"(cond="var3 &gt;= var4")"), std::string::npos)
        << "conf:compareIDVal=\"3&gt;=4\" should convert to cond=\"var3 &gt;= var4\"";

    // Verify conf:compareIDVal is removed
    EXPECT_EQ(result.find("conf:compareIDVal"), std::string::npos) << "conf:compareIDVal should be completely removed";
}

// Test conf:location and conf:varExpr for assignment statements
TEST_F(TXMLConverterTest, ConvertsAssignmentExpressions) {
    std::string txml = createValidTXML(R"(
        <assign conf:location="var1" conf:expr="var2"/>
        <assign conf:location="var4" conf:expr="0"/>
        <assign conf:location="varName" conf:expr="otherVar"/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    // Verify conf:location and conf:expr conversions (already implemented)
    EXPECT_NE(result.find(R"(location="var1" expr="var2")"), std::string::npos)
        << "conf:location and conf:expr should convert to location and expr attributes";

    EXPECT_NE(result.find(R"(location="var4" expr="0")"), std::string::npos)
        << "conf:location and conf:expr should convert with literal values";

    EXPECT_NE(result.find(R"(location="varName" expr="otherVar")"), std::string::npos)
        << "conf:location and conf:expr should convert variable names properly";

    // Verify cleanup
    EXPECT_EQ(result.find("conf:location"), std::string::npos) << "conf:location should be removed";
    EXPECT_EQ(result.find("conf:expr"), std::string::npos) << "conf:expr should be removed";
}

// Test conf:idVal attribute conversion for transition conditions
TEST_F(TXMLConverterTest, ConvertsTransitionConditions) {
    std::string txml = createValidTXML(R"(
        <transition conf:cond="var4 == 0" conf:targetfail=""/>
        <transition conf:cond="var1 != var5" conf:targetpass=""/>
        <transition conf:targetfail=""/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    // Verify conf:cond and conf:target conversions (already implemented)
    EXPECT_NE(result.find(R"(cond="var4 == 0" target="fail")"), std::string::npos)
        << "conf:cond and conf:targetfail should convert to cond and target attributes";

    EXPECT_NE(result.find(R"(cond="var1 != var5" target="pass")"), std::string::npos)
        << "conf:cond and conf:targetpass should convert to cond and target attributes";

    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos)
        << "conf:targetfail should convert to target attribute";

    // Verify cleanup
    EXPECT_EQ(result.find("conf:cond"), std::string::npos) << "conf:cond should be removed";
    EXPECT_EQ(result.find("conf:targetfail"), std::string::npos) << "conf:targetfail should be removed";
    EXPECT_EQ(result.find("conf:targetpass"), std::string::npos) << "conf:targetpass should be removed";
}

// Test complex TXML pattern with foreach, conditions, and assignments
TEST_F(TXMLConverterTest, ConvertsForeachWithConditionsAndAssignments) {
    // This tests a complex pattern with foreach loops, conditional logic, and variable assignments
    std::string txml = R"(<?xml version="1.0"?>
<scxml initial="s0" version="1.0" conf:datamodel=""  xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1" conf:expr="0"/>
  <data conf:id="2"/>
  <data conf:id="3">
    <conf:array123/>
  </data>
  <data conf:id="4" conf:expr="1"/>
</datamodel>
<state id="s0">
  <onentry>
    <foreach conf:item="2" conf:arrayVar="3">
      <if conf:cond="var1 &lt; var2">
        <assign conf:location="var1" conf:expr="var2"/>
      <else/>
        <assign conf:location="var4" conf:expr="0"/>
      </if>
    </foreach>
  </onentry>
  <transition conf:cond="var4 == 0" conf:targetfail=""/>
  <transition conf:targetpass=""/>
</state>
<conf:pass/>
<conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // **Data model conversions**
    EXPECT_NE(result.find(R"(datamodel="ecmascript")"), std::string::npos)
        << "conf:datamodel should convert to datamodel=\"ecmascript\"";

    EXPECT_NE(result.find(R"(id="var1" expr="0")"), std::string::npos) << "conf:id=\"1\" should convert to id=\"var1\"";

    EXPECT_NE(result.find(R"(id="var2")"), std::string::npos) << "conf:id=\"2\" should convert to id=\"var2\"";

    EXPECT_NE(result.find(R"(id="var3")"), std::string::npos) << "conf:id=\"3\" should convert to id=\"var3\"";

    // FIXED: W3C test 153 bug - was expecting expr="var1" instead of expr="1"
    // conf:expr="1" should convert to literal expr="1", not variable reference expr="var1"
    EXPECT_NE(result.find(R"(id="var4" expr="1")"), std::string::npos)
        << "conf:id=\"4\" conf:expr=\"1\" should convert to id=\"var4\" expr=\"1\"";

    // **Array conversion**
    EXPECT_NE(result.find(R"([1,2,3])"), std::string::npos) << "conf:array123 should convert to [1,2,3]";

    // **Foreach attribute conversions**
    EXPECT_NE(result.find(R"(item="var2" array="var3")"), std::string::npos)
        << "conf:item=\"2\" conf:arrayVar=\"3\" should convert with var prefix";

    // **IF condition conversion - Using already implemented conf:cond (with HTML entities)**
    EXPECT_NE(result.find(R"(cond="var1 &lt; var2")"), std::string::npos)
        << "conf:cond should convert comparison expressions to proper SCXML conditions";

    // **Assignment conversions - Using already implemented conf:location and conf:expr**
    EXPECT_NE(result.find(R"(location="var1" expr="var2")"), std::string::npos)
        << "conf:location and conf:expr should convert to location and expr attributes";

    EXPECT_NE(result.find(R"(location="var4" expr="0")"), std::string::npos)
        << "conf:location and conf:expr should convert with literal values";

    // **Transition condition conversion - Using already implemented conf:cond and conf:target**
    EXPECT_NE(result.find(R"(cond="var4 == 0" target="fail")"), std::string::npos)
        << "conf:cond and conf:targetfail should convert to cond and target attributes";

    // **Final states**
    EXPECT_NE(result.find(R"(<final id="pass"/>)"), std::string::npos)
        << "conf:pass should convert to final id=\"pass\"";

    EXPECT_NE(result.find(R"(<final id="fail"/>)"), std::string::npos)
        << "conf:fail should convert to final id=\"fail\"";

    // **Complete cleanup verification**
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be completely removed";

    // **Regression prevention: verify proper conditional element structure**
    EXPECT_NE(result.find(R"(<if cond="var1 &lt; var2">)"), std::string::npos)
        << "IF element should have proper cond attribute for conditional logic";

    // Note: We're using conf:cond now, so we don't expect empty <if> elements
    EXPECT_EQ(result.find("conf:cond"), std::string::npos) << "conf:cond should be removed";
}

// ============================================================================
// Comprehensive TXML Pattern Tests (All W3C Test Scenarios)
// ============================================================================

// Test boolean condition attributes (conf:true/conf:false)
TEST_F(TXMLConverterTest, ConvertsBooleanConditionAttributes) {
    std::string txml = createValidTXML(R"(
        <onentry>
            <if conf:false="">
                <raise event="foo"/>
            <elseif conf:true=""/>
                <raise event="bar"/>
            <else/>
                <raise event="baz"/>
            </if>
        </onentry>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(cond="false")"), std::string::npos) << "conf:false should convert to cond=\"false\"";
    EXPECT_NE(result.find(R"(cond="true")"), std::string::npos) << "conf:true should convert to cond=\"true\"";
    EXPECT_EQ(result.find("conf:false"), std::string::npos) << "conf:false attribute should be removed";
    EXPECT_EQ(result.find("conf:true"), std::string::npos) << "conf:true attribute should be removed";
}

// Test increment counter elements
TEST_F(TXMLConverterTest, ConvertsIncrementCounterElements) {
    std::string txml = createValidTXML(R"(
        <onentry>
            <conf:incrementID id="1"/>
            <conf:incrementID id="5"/>
        </onentry>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(<assign location="var1" expr="var1 + 1"/>)"), std::string::npos)
        << "conf:incrementID id=\"1\" should convert to assign increment for var1";
    EXPECT_NE(result.find(R"(<assign location="var5" expr="var5 + 1"/>)"), std::string::npos)
        << "conf:incrementID id=\"5\" should convert to assign increment for var5";
    EXPECT_EQ(result.find("conf:incrementID"), std::string::npos) << "conf:incrementID elements should be removed";
}

// Test variable value comparison conditions
TEST_F(TXMLConverterTest, ConvertsVariableValueComparisons) {
    std::string txml = createValidTXML(R"(
        <transition event="test1" conf:idVal="1=1" target="pass"/>
        <transition event="test2" conf:idVal="4=0" target="pass"/>
        <transition event="test3" conf:idVal="1!=5" target="pass"/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(cond="var1 == 1")"), std::string::npos)
        << "conf:idVal=\"1=1\" should convert to cond=\"var1 == 1\"";
    EXPECT_NE(result.find(R"(cond="var4 == 0")"), std::string::npos)
        << "conf:idVal=\"4=0\" should convert to cond=\"var4 == 0\"";
    EXPECT_NE(result.find(R"(cond="var1 != var5")"), std::string::npos)
        << "conf:idVal=\"1!=5\" should convert to cond=\"var1 != var5\"";
    EXPECT_EQ(result.find("conf:idVal"), std::string::npos) << "conf:idVal attributes should be removed";
}

// Test variable expression assignments
TEST_F(TXMLConverterTest, ConvertsVariableExpressionAssignments) {
    std::string txml = createValidTXML(R"(
        <onentry>
            <assign conf:location="1" conf:varExpr="2"/>
            <assign conf:location="3" conf:varExpr="4"/>
        </onentry>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(location="var1" expr="var2")"), std::string::npos)
        << "conf:location=\"1\" and conf:varExpr=\"2\" should convert to location=\"var1\" expr=\"var2\"";
    EXPECT_NE(result.find(R"(location="var3" expr="var4")"), std::string::npos)
        << "conf:location=\"3\" and conf:varExpr=\"4\" should convert to location=\"var3\" expr=\"var4\"";
    EXPECT_EQ(result.find("conf:varExpr"), std::string::npos) << "conf:varExpr attributes should be removed";
}

// Test variable comparison expressions
TEST_F(TXMLConverterTest, ConvertsVariableComparisonExpressions) {
    std::string txml = createValidTXML(R"(
        <transition event="test1" conf:compareIDVal="1&lt;2" target="pass"/>
        <transition event="test2" conf:compareIDVal="3&gt;=4" target="pass"/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(cond="var1 &lt; var2")"), std::string::npos)
        << "conf:compareIDVal=\"1&lt;2\" should convert to cond=\"var1 &lt; var2\"";
    EXPECT_NE(result.find(R"(cond="var3 &gt;= var4")"), std::string::npos)
        << "conf:compareIDVal=\"3&gt;=4\" should convert to cond=\"var3 &gt;= var4\"";
    EXPECT_EQ(result.find("conf:compareIDVal"), std::string::npos) << "conf:compareIDVal attributes should be removed";
}

// Test timing and delay attributes
TEST_F(TXMLConverterTest, ConvertsTimingAndDelayAttributes) {
    std::string txml = createValidTXML(R"(
        <onentry>
            <send event="timeout" conf:delay="2s"/>
            <send event="delayed" conf:delay="1000ms"/>
        </onentry>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(delay="2s")"), std::string::npos) << "conf:delay should convert to delay attribute";
    EXPECT_NE(result.find(R"(delay="1000ms")"), std::string::npos) << "conf:delay should convert to delay attribute";
    EXPECT_EQ(result.find("conf:delay"), std::string::npos) << "conf:delay attributes should be removed";
}

// Test variable existence checks
TEST_F(TXMLConverterTest, ConvertsVariableExistenceChecks) {
    std::string txml = createValidTXML(R"(
        <transition conf:isBound="1" target="pass"/>
        <transition conf:isBound="someVar" target="pass"/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find(R"(cond="typeof var1 !== 'undefined'")"), std::string::npos)
        << "conf:isBound=\"1\" should convert to typeof check for var1";
    EXPECT_NE(result.find(R"(cond="typeof someVar !== 'undefined'")"), std::string::npos)
        << "conf:isBound=\"someVar\" should convert to typeof check for someVar";
    EXPECT_EQ(result.find("conf:isBound"), std::string::npos) << "conf:isBound attributes should be removed";
}

// Test array data elements
TEST_F(TXMLConverterTest, ConvertsArrayDataElements) {
    std::string txml = createValidTXML(R"(
        <onentry>
            <assign location="arr1">
                <conf:array123/>
            </assign>
            <assign location="arr2">
                <conf:array456/>
            </assign>
        </onentry>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    EXPECT_NE(result.find("[1,2,3]"), std::string::npos) << "conf:array123 should convert to [1,2,3]";
    EXPECT_NE(result.find("[4,5,6]"), std::string::npos) << "conf:array456 should convert to [4,5,6]";
    EXPECT_EQ(result.find("conf:array123"), std::string::npos) << "conf:array123 elements should be removed";
}

// Test complete W3C conditional logic scenario
TEST_F(TXMLConverterTest, ConvertsCompleteConditionalLogicScenario) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml initial="s0" version="1.0" conf:datamodel="" xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1" conf:expr="0"/>
  <data conf:id="2"/>
  <data conf:id="3">
    <conf:array123/>
  </data>
</datamodel>
<state id="s0">
  <onentry>
    <if conf:false="">
      <raise event="foo"/>
      <conf:incrementID id="1"/>
    <elseif conf:true=""/>
      <raise event="bar"/>
      <conf:incrementID id="1"/>
    <else/>
      <raise event="baz"/>
      <conf:incrementID id="1"/>
    </if>
    <foreach conf:item="2" conf:arrayVar="3">
      <if conf:compareIDVal="1&lt;2">
        <assign conf:location="1" conf:varExpr="2"/>
      </if>
    </foreach>
  </onentry>
  <transition event="bar" conf:idVal="1=1" conf:targetpass=""/>
  <transition event="*" conf:targetfail=""/>
</state>
<conf:pass/>
<conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // Verify all transformations work together
    EXPECT_NE(result.find(R"(datamodel="ecmascript")"), std::string::npos);
    EXPECT_NE(result.find(R"(id="var1")"), std::string::npos);
    EXPECT_NE(result.find(R"(cond="false")"), std::string::npos);
    EXPECT_NE(result.find(R"(cond="true")"), std::string::npos);
    EXPECT_NE(result.find(R"(<assign location="var1" expr="var1 + 1"/>)"), std::string::npos);
    EXPECT_NE(result.find(R"(item="var2" array="var3")"), std::string::npos);
    EXPECT_NE(result.find(R"(cond="var1 &lt; var2")"), std::string::npos);
    EXPECT_NE(result.find(R"(location="var1" expr="var2")"), std::string::npos);
    EXPECT_NE(result.find(R"(cond="var1 == 1")"), std::string::npos);
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos);
    EXPECT_NE(result.find(R"(<final id="pass"/>)"), std::string::npos);
    EXPECT_EQ(result.find("conf:"), std::string::npos);
}

// Test 147 regression scenario - if-elseif-else conditional logic transformation
TEST_F(TXMLConverterTest, ConvertsConditionalLogicRegressionScenario) {
    std::string txml = createValidTXML(R"(
        <state id="s0">
            <onentry>
                <if conf:false="">
                    <assign location="result" expr="'fail_if'"/>
                <elseif conf:true="">
                    <assign location="result" expr="'pass_elseif'"/>
                <else/>
                    <assign location="result" expr="'fail_else'"/>
                </if>
            </onentry>
            <transition target="end"/>
        </state>
        <final id="end"/>
    )");

    std::string result = converter.convertTXMLToSCXML(txml);

    // Critical regression checks: ensure elseif gets explicit condition
    EXPECT_NE(result.find(R"(<if cond="false">)"), std::string::npos)
        << "if conf:false should convert to explicit cond=\"false\"";
    EXPECT_NE(result.find(R"(<elseif cond="true">)"), std::string::npos)
        << "elseif conf:true should convert to explicit cond=\"true\" (not empty elseif)";
    EXPECT_NE(result.find(R"(<else/>)"), std::string::npos) << "else should remain unchanged";

    // Verify complete conditional structure
    EXPECT_NE(result.find(R"(expr="'pass_elseif'")"), std::string::npos) << "elseif content should be preserved";
    EXPECT_NE(result.find(R"(expr="'fail_if'")"), std::string::npos) << "if content should be preserved";
    EXPECT_NE(result.find(R"(expr="'fail_else'")"), std::string::npos) << "else content should be preserved";

    // Ensure no conf: namespace remains
    EXPECT_EQ(result.find("conf:true"), std::string::npos) << "conf:true attribute should be completely removed";
    EXPECT_EQ(result.find("conf:false"), std::string::npos) << "conf:false attribute should be completely removed";

    // Critical: ensure elseif is NOT empty (regression prevention)
    EXPECT_EQ(result.find("<elseif>"), std::string::npos) << "elseif should never be empty (regression check)";
    EXPECT_EQ(result.find("<elseif/>"), std::string::npos)
        << "elseif should never be self-closing without condition (regression check)";
}

// ============================================================================
// New W3C Test 155 and 153 Bug Fix Tests
// ============================================================================

TEST_F(TXMLConverterTest, ConfSumVarsConversion) {
    // Test W3C 155 specific pattern: <conf:sumVars id1="1" id2="2"/>
    std::string input = R"(
        <scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
            <state>
                <onentry>
                    <foreach>
                        <conf:sumVars id1="1" id2="2"/>
                    </foreach>
                </onentry>
                <transition conf:targetpass=""/>
            </state>
            <conf:pass/>
            <conf:fail/>
        </scxml>
    )";

    std::string result = converter.convertTXMLToSCXML(input);

    // Verify conf:sumVars converts to assign with proper variable prefixes
    EXPECT_NE(result.find(R"(<assign location="var1" expr="var1 + var2"/>)"), std::string::npos)
        << "conf:sumVars id1=\"1\" id2=\"2\" should convert to assign location=\"var1\" expr=\"var1 + var2\"";

    // Verify no conf: elements remain
    EXPECT_EQ(result.find("conf:sumVars"), std::string::npos) << "conf:sumVars should be completely removed";
}

TEST_F(TXMLConverterTest, ConfIdValNumericComparison) {
    // Test W3C 155 specific pattern: conf:idVal="1=6"
    std::string input = R"(
        <scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
            <state>
                <transition conf:idVal="1=6" conf:targetpass=""/>
            </state>
        </scxml>
    )";

    std::string result = converter.convertTXMLToSCXML(input);

    // Verify conf:idVal="1=6" converts to cond="var1 == 6"
    EXPECT_NE(result.find(R"(cond="var1 == 6")"), std::string::npos)
        << "conf:idVal=\"1=6\" should convert to cond=\"var1 == 6\"";

    // Verify target pass conversion
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos)
        << "conf:targetpass should convert to target=\"pass\"";

    // Verify no conf: elements remain
    EXPECT_EQ(result.find("conf:idVal"), std::string::npos) << "conf:idVal should be completely removed";
}

TEST_F(TXMLConverterTest, ConfExprLiteralValues) {
    // Test W3C 153 bug fix: conf:expr="1" should remain as literal expr="1"
    std::string input = R"(
        <scxml xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
            <datamodel>
                <data conf:id="4" conf:expr="1"/>
                <data conf:id="5" conf:expr="0"/>
            </datamodel>
            <state>
                <transition conf:targetpass=""/>
            </state>
            <conf:pass/>
            <conf:fail/>
        </scxml>
    )";

    std::string result = converter.convertTXMLToSCXML(input);

    // CRITICAL: conf:expr="1" should convert to expr="1" (literal), NOT expr="var1"
    EXPECT_NE(result.find(R"(id="var4" expr="1")"), std::string::npos)
        << "W3C test 153 bug fix: conf:expr=\"1\" should convert to literal expr=\"1\"";

    EXPECT_NE(result.find(R"(id="var5" expr="0")"), std::string::npos)
        << "conf:expr=\"0\" should convert to literal expr=\"0\"";

    // Verify we don't incorrectly convert literals to variable references
    EXPECT_EQ(result.find(R"(expr="var1")"), std::string::npos)
        << "Literal values should NOT be converted to variable references";
}

TEST_F(TXMLConverterTest, W3CTest155FullConversion) {
    // Complete W3C test 155 conversion test
    std::string input = R"(
        <scxml initial="s0" xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
            <datamodel>
                <data conf:id="1" conf:expr="0"/>
                <data conf:id="2"/>
                <data conf:id="3"><conf:array123/></data>
            </datamodel>
            <state id="s0">
                <onentry>
                    <foreach conf:item="2" conf:arrayVar="3">
                        <conf:sumVars id1="1" id2="2"/>
                    </foreach>
                </onentry>
                <transition conf:idVal="1=6" conf:targetpass=""/>
                <transition conf:targetfail=""/>
            </state>
            <conf:pass/>
            <conf:fail/>
        </scxml>
    )";

    std::string result = converter.convertTXMLToSCXML(input);

    // Verify all key conversions for W3C test 155
    EXPECT_NE(result.find(R"(id="var1" expr="0")"), std::string::npos) << "var1 should be initialized to 0";

    EXPECT_NE(result.find(R"([1,2,3])"), std::string::npos) << "Array should be converted to JavaScript array format";

    EXPECT_NE(result.find(R"(item="var2" array="var3")"), std::string::npos)
        << "foreach attributes should use var prefixes";

    EXPECT_NE(result.find(R"(<assign location="var1" expr="var1 + var2"/>)"), std::string::npos)
        << "sumVars should create accumulation assignment";

    EXPECT_NE(result.find(R"(cond="var1 == 6" target="pass")"), std::string::npos)
        << "Success condition should check if sum equals 6";

    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos) << "Fallback transition to fail state";

    EXPECT_NE(result.find(R"(<final id="pass"/>)"), std::string::npos)
        << "Pass state should be converted to final state";
    EXPECT_NE(result.find(R"(<final id="fail"/>)"), std::string::npos)
        << "Fail state should be converted to final state";

    // Ensure complete cleanup
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// W3C Test 156: Error handling in foreach execution
TEST_F(TXMLConverterTest, W3CTest156ErrorHandlingConversion) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml initial="s0" version="1.0" conf:datamodel=""  xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1" conf:expr="0"/>
  <data conf:id="2"/>
  <data conf:id="3">
  <conf:array123/>
  </data>
</datamodel>
<state id="s0">
  <onentry>
    <foreach conf:item="2"  conf:arrayVar="3">
      <conf:incrementID id="1"/>
      <assign conf:location="5" conf:illegalExpr=""/>
    </foreach>
  </onentry>
  <transition conf:idVal="1=1" conf:targetpass=""/>
  <transition conf:targetfail=""/>
</state>
<conf:pass/>
<conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // Test conf:incrementID conversion
    EXPECT_NE(result.find(R"(<assign location="var1" expr="var1 + 1"/>)"), std::string::npos)
        << "conf:incrementID id=\"1\" should convert to assign increment for var1";

    // Test conf:illegalExpr conversion - should create intentional error
    EXPECT_NE(result.find(R"(expr="undefined.invalidProperty")"), std::string::npos)
        << "conf:illegalExpr should convert to expr with invalid JavaScript expression";

    // Test conf:idVal="1=1" conversion
    EXPECT_NE(result.find(R"(cond="var1 == 1")"), std::string::npos)
        << "conf:idVal=\"1=1\" should convert to cond=\"var1 == 1\"";

    // Test array data conversion
    EXPECT_NE(result.find(R"([1,2,3])"), std::string::npos) << "conf:array123 should convert to [1,2,3]";

    // Test location conversion
    EXPECT_NE(result.find(R"(location="var5")"), std::string::npos)
        << "conf:location=\"5\" should convert to location=\"var5\"";
}

// W3C Test 159: Error handling in executable content blocks
TEST_F(TXMLConverterTest, W3CTest159ExecutableContentErrorHandling) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml initial="s0" conf:datamodel="" version="1.0" xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1" conf:expr="0"/>
</datamodel>
<state id="s0">
  <onentry>
   <send event="thisWillFail" conf:illegalTarget=""/>
   <conf:incrementID id="1"/>
  </onentry>
  <transition conf:idVal="1=1" conf:targetfail=""/>
  <transition conf:targetpass=""/>
</state>
<conf:pass/>
<conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // Test conf:illegalTarget conversion - should remove event attribute to create error
    EXPECT_EQ(result.find(R"(conf:illegalTarget)"), std::string::npos)
        << "conf:illegalTarget should be completely removed";

    // Test that event attribute is removed from send element with conf:illegalTarget
    EXPECT_NE(result.find(R"(<send />)"), std::string::npos)
        << "send element should have event attribute removed to cause error";

    // Test conf:incrementID conversion
    EXPECT_NE(result.find(R"(<assign location="var1" expr="var1 + 1"/>)"), std::string::npos)
        << "conf:incrementID id=\"1\" should convert to assign increment for var1";

    // Test conf:idVal="1=1" conversion
    EXPECT_NE(result.find(R"(cond="var1 == 1")"), std::string::npos)
        << "conf:idVal=\"1=1\" should convert to cond=\"var1 == 1\"";

    // Test target conversions
    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos)
        << "conf:targetfail should convert to target=\"fail\"";
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos)
        << "conf:targetpass should convert to target=\"pass\"";
}

// W3C Test 176: Event data parameter handling and idVal condition conversion
TEST_F(TXMLConverterTest, W3CTest176EventDataAndIdValCondition) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml initial="s0" version="1.0" conf:datamodel=""  xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1" conf:expr="1"/>
  <data conf:id="2"/>
  </datamodel>

<state id="s0">
  <onentry>
   <assign conf:location="1" conf:expr="2"/>
   <send event="event1">
     <param name="aParam" conf:varExpr="1"/>
     </send>
    </onentry>

  <transition event="event1"  target="s1">
  <assign conf:location="2" conf:eventDataFieldValue="aParam"/>
  </transition>
  <transition event="*" conf:targetfail=""/>
 </state>

<state id="s1">
  <transition conf:idVal="2=2" conf:targetpass=""/>
  <transition conf:targetfail=""/>
  </state>

   <conf:pass/>
   <conf:fail/>

</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // Test variable ID conversions
    EXPECT_NE(result.find(R"(id="var1")"), std::string::npos) << "conf:id=\"1\" should convert to id=\"var1\"";
    EXPECT_NE(result.find(R"(id="var2")"), std::string::npos) << "conf:id=\"2\" should convert to id=\"var2\"";

    // Test location conversions
    EXPECT_NE(result.find(R"(location="var1")"), std::string::npos)
        << "conf:location=\"1\" should convert to location=\"var1\"";

    // Test param varExpr conversion
    EXPECT_NE(result.find(R"(expr="var1")"), std::string::npos) << "conf:varExpr=\"1\" should convert to expr=\"var1\"";

    // **KEY TEST**: conf:eventDataFieldValue should convert to _event.data access
    EXPECT_NE(result.find(R"(expr="_event.data.aParam")"), std::string::npos)
        << "conf:eventDataFieldValue=\"aParam\" should convert to expr=\"_event.data.aParam\"";

    // **KEY TEST**: conf:idVal should convert to proper comparison
    EXPECT_NE(result.find(R"(cond="var2 == 2")"), std::string::npos)
        << "conf:idVal=\"2=2\" should convert to cond=\"var2 == 2\"";

    // Test target conversions
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos)
        << "conf:targetpass should convert to target=\"pass\"";
    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos)
        << "conf:targetfail should convert to target=\"fail\"";

    // Verify final states
    EXPECT_NE(result.find(R"(<final id="pass"/>)"), std::string::npos)
        << "conf:pass should convert to final id=\"pass\"";
    EXPECT_NE(result.find(R"(<final id="fail"/>)"), std::string::npos)
        << "conf:fail should convert to final id=\"fail\"";

    // Ensure all conf: references are removed
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// ============================================================================
// New Test: W3C Test 240 conf:namelistIdVal and invoke namelist/param
// ============================================================================

// Test W3C Test 240: conf:namelistIdVal condition and invoke data passing
TEST_F(TXMLConverterTest, W3CTest240NamelistIdValAndInvokeDataPassing) {
    std::string txml = R"(<?xml version="1.0"?>
<!-- Test namelist and param for invoke -->
<scxml initial="s0" version="1.0" conf:datamodel="" xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1" conf:expr="1"/>
</datamodel>

<state id="s0" initial="s01">
  <state id="s01">
    <invoke type="http://www.w3.org/TR/scxml/" conf:namelist="1">
      <content>
        <scxml initial="sub01" version="1.0" conf:datamodel="" xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
          <datamodel>
            <data conf:id="1" conf:expr="0"/>
          </datamodel>
          <state id="sub01">
            <transition conf:namelistIdVal="1=1" target="subFinal1">
              <send target="#_parent" event="success"/>
            </transition>
            <transition target="subFinal1">
              <send target="#_parent" event="failure"/>
            </transition>
          </state>
          <final id="subFinal1"/>
        </scxml>
      </content>
    </invoke>
    <transition event="success" target="s02"/>
    <transition event="failure" conf:targetfail=""/>
  </state>

  <state id="s02">
    <invoke type="http://www.w3.org/TR/scxml/">
      <param conf:name="1" conf:expr="1"/>
      <content>
        <scxml initial="sub02" version="1.0" conf:datamodel="" xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
          <datamodel>
            <data conf:id="1" conf:expr="0"/>
          </datamodel>
          <state id="sub02">
            <transition conf:idVal="1=1" target="subFinal2">
              <send target="#_parent" event="success"/>
            </transition>
            <transition target="subFinal2">
              <send target="#_parent" event="failure"/>
            </transition>
          </state>
          <final id="subFinal2"/>
        </scxml>
      </content>
    </invoke>
    <transition event="success" conf:targetpass=""/>
    <transition event="failure" conf:targetfail=""/>
  </state>
</state>

<conf:pass/>
<conf:fail/>
</scxml>)";

    TXMLConverter converter;
    std::string result = converter.convertTXMLToSCXML(txml);

    // Test datamodel conversions
    EXPECT_NE(result.find(R"(id="var1")"), std::string::npos) << "conf:id=\"1\" should convert to id=\"var1\"";

    // Test namelist conversion
    EXPECT_NE(result.find(R"(namelist="var1")"), std::string::npos)
        << "conf:namelist=\"1\" should convert to namelist=\"var1\"";

    // Test param name conversion
    EXPECT_NE(result.find(R"(name="var1")"), std::string::npos) << "conf:name=\"1\" should convert to name=\"var1\"";

    // **KEY TEST**: conf:namelistIdVal should convert to proper comparison
    EXPECT_NE(result.find(R"(cond="var1 == 1")"), std::string::npos)
        << "conf:namelistIdVal=\"1=1\" should convert to cond=\"var1 == 1\"";

    // Verify conf:idVal also works (used in second invoke)
    std::string::size_type pos = 0;
    int count = 0;
    while ((pos = result.find(R"(cond="var1 == 1")", pos)) != std::string::npos) {
        count++;
        pos++;
    }
    EXPECT_EQ(count, 2) << "Should find 2 occurrences of cond=\"var1 == 1\" (namelistIdVal + idVal)";

    // Test target conversions
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos)
        << "conf:targetpass should convert to target=\"pass\"";
    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos)
        << "conf:targetfail should convert to target=\"fail\"";

    // Ensure all conf: references are removed
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// ============================================================================
// New Test: W3C Test 175 conf:delayFromVar Pattern Conversion
// ============================================================================

// Test W3C Test 175: conf:delayFromVar conversion for delayed send actions
TEST_F(TXMLConverterTest, W3CTest175DelayFromVarConversion) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml initial="s0" conf:datamodel="" version="1.0" xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1" conf:expr="1"/>
</datamodel>

<state id="s0">
  <onentry>
    <send conf:delayFromVar="1" event="event2"/>
    <send event="timeout" delay="2s"/>
  </onentry>
  
  <transition event="event2" conf:targetpass=""/>
  <transition event="timeout" conf:targetfail=""/>
  <transition event="*" conf:targetfail=""/>
</state>

<conf:pass/>
<conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // **CRITICAL TEST**: conf:delayFromVar="1" should convert to delayexpr="var1"
    EXPECT_NE(result.find(R"(delayexpr="var1")"), std::string::npos)
        << "conf:delayFromVar=\"1\" should convert to delayexpr=\"var1\" for dynamic delay evaluation";

    // Test that send element has proper event attribute preserved
    EXPECT_NE(result.find(R"(event="event2")"), std::string::npos) << "Send element should preserve event attribute";

    // Test data model conversion
    EXPECT_NE(result.find(R"(id="var1" expr="1")"), std::string::npos)
        << "conf:id=\"1\" conf:expr=\"1\" should convert to id=\"var1\" expr=\"1\"";

    // Test target conversions
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos)
        << "conf:targetpass should convert to target=\"pass\"";
    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos)
        << "conf:targetfail should convert to target=\"fail\"";

    // Verify final states
    EXPECT_NE(result.find(R"(<final id="pass"/>)"), std::string::npos)
        << "conf:pass should convert to final id=\"pass\"";
    EXPECT_NE(result.find(R"(<final id="fail"/>)"), std::string::npos)
        << "conf:fail should convert to final id=\"fail\"";

    // Ensure all conf: references are removed
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

// Test W3C Test 183: send idlocation and isBound pattern conversion
TEST_F(TXMLConverterTest, W3CTest183SendIdLocationHandling) {
    std::string txml = R"(<?xml version="1.0"?>
<!-- we test that <send> stores the value of the sendid in idlocation.  If it does,
var1 has a value and we pass.  Otherwise we fail  -->

<scxml initial="s0" conf:datamodel=""  version="1.0" xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1"/>
  </datamodel>

<state id="s0">
  <onentry>
   <send event="event1" conf:idlocation="1"/>
    </onentry>

  <transition conf:isBound="1" conf:targetpass=""/>
  <transition conf:targetfail=""/>
 </state>


   <conf:pass/>
   <conf:fail/>

</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // Test basic structure conversion
    EXPECT_NE(result.find(R"(datamodel="ecmascript")"), std::string::npos)
        << "conf:datamodel should convert to datamodel=\"ecmascript\"";

    // Test data model conversion
    EXPECT_NE(result.find(R"(id="var1")"), std::string::npos) << "conf:id=\"1\" should convert to id=\"var1\"";

    // **KEY TEST**: conf:idlocation should convert to standard SCXML idlocation
    EXPECT_NE(result.find(R"(idlocation="var1")"), std::string::npos)
        << "conf:idlocation=\"1\" should convert to idlocation=\"var1\"";

    // **KEY TEST**: conf:isBound should convert to proper undefined check
    EXPECT_NE(result.find(R"(cond="typeof var1 !== 'undefined'")"), std::string::npos)
        << "conf:isBound=\"1\" should convert to cond=\"typeof var1 !== 'undefined'\"";

    // Test target conversions
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos)
        << "conf:targetpass should convert to target=\"pass\"";
    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos)
        << "conf:targetfail should convert to target=\"fail\"";

    // Verify final states
    EXPECT_NE(result.find(R"(<final id="pass"/>)"), std::string::npos)
        << "conf:pass should convert to final id=\"pass\"";
    EXPECT_NE(result.find(R"(<final id="fail"/>)"), std::string::npos)
        << "conf:fail should convert to final id=\"fail\"";

    // Ensure all conf: references are removed
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

TEST_F(TXMLConverterTest, ConvertInvalidSendTypeTest) {
    TXMLConverter converter;

    std::string txml = R"(<?xml version="1.0"?>
<scxml initial="s0" version="1.0" conf:datamodel=""  xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">

<state id="s0">
  <onentry>
   <send conf:invalidSendType="" event="event1"/>
   <send event="timeout"/>
    </onentry>

  <transition event="error.execution" conf:targetpass=""/>
  <transition event="*" conf:targetfail=""/>
 </state>

<conf:pass/>
<conf:fail/>

</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // Test conf:invalidSendType conversion - should add invalid type attribute
    EXPECT_NE(result.find(R"(type="unsupported_type")"), std::string::npos)
        << "conf:invalidSendType should convert to type=\"unsupported_type\"";

    // Test that conf:invalidSendType attribute is removed
    EXPECT_EQ(result.find(R"(conf:invalidSendType)"), std::string::npos)
        << "conf:invalidSendType should be completely removed";

    // Test that event attribute is preserved (unlike conf:illegalTarget)
    EXPECT_NE(result.find(R"(event="event1")"), std::string::npos)
        << "event attribute should be preserved for conf:invalidSendType";

    // Test target conversions
    EXPECT_NE(result.find(R"(target="pass")"), std::string::npos)
        << "conf:targetpass should convert to target=\"pass\"";
    EXPECT_NE(result.find(R"(target="fail")"), std::string::npos)
        << "conf:targetfail should convert to target=\"fail\"";

    // Verify final states
    EXPECT_NE(result.find(R"(<final id="pass"/>)"), std::string::npos)
        << "conf:pass should convert to final id=\"pass\"";
    EXPECT_NE(result.find(R"(<final id="fail"/>)"), std::string::npos)
        << "conf:fail should convert to final id=\"fail\"";

    // Ensure all conf: references are removed
    EXPECT_EQ(result.find("conf:"), std::string::npos) << "All conf: references should be removed";
}

TEST_F(TXMLConverterTest, ConvertsCancelSendIDExprAttribute) {
    std::string input = R"(<?xml version="1.0"?>
<scxml initial="s0" version="1.0" conf:datamodel=""  xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1" conf:quoteExpr="bar"/>
</datamodel>
<state id="s0">
  <onentry>
   <send  id="foo" event="event1" conf:delay="1"/>
   <assign conf:location="1" conf:quoteExpr="foo"/>
   <cancel conf:sendIDExpr="1"/>
  </onentry>
</state>
<conf:pass/>
<conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(input);

    // Check that conf:sendIDExpr="1" is converted to sendidexpr attribute with variable reference
    EXPECT_TRUE(result.find(R"(sendidexpr="var1")") != std::string::npos);
    EXPECT_TRUE(result.find("conf:sendIDExpr") == std::string::npos);
}

TEST_F(TXMLConverterTest, ConvertsInvokeTypeExprAttribute) {
    std::string input = R"(<?xml version="1.0"?>
<scxml initial="s0" conf:datamodel="" version="1.0"  xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1" conf:quoteExpr="foo"/>
</datamodel>
<state id="s0">
  <onentry>
    <assign conf:location="1" conf:quoteExpr="http://www.w3.org/TR/scxml/"/>
  </onentry>
  <invoke conf:typeExpr="1">
    <content>
        <scxml initial="subFinal" conf:datamodel="" version="1.0" xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
      <final id="subFinal"/>
        </scxml>
    </content>
  </invoke>
  <transition event="done.invoke" conf:targetpass=""/>
  <transition event="*" conf:targetfail=""/>
</state>
<conf:pass/>
<conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(input);

    // Check that conf:typeExpr="1" is converted to typeexpr attribute with variable reference
    EXPECT_TRUE(result.find(R"(typeexpr="var1")") != std::string::npos);
    EXPECT_TRUE(result.find("conf:typeExpr") == std::string::npos);
}

TEST_F(TXMLConverterTest, ConvertsInvokeSrcExprAttribute) {
    std::string input = R"(<?xml version="1.0"?>
<scxml initial="s0" version="1.0" conf:datamodel=""  xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1" conf:quoteExpr="foo"/>
</datamodel>
<state id="s0">
  <onentry>
    <send event="timeout" delay="5s"/>
   <assign conf:location="1" conf:quoteExpr="file:test216sub1.scxml"/>
  </onentry>
  <invoke conf:srcExpr="1" type="http://www.w3.org/TR/scxml"/>
  <transition event="done.invoke" conf:targetpass=""/>
  <transition event="*" conf:targetfail=""/>
</state>
<conf:pass/>
<conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(input);

    EXPECT_TRUE(result.find("srcexpr=\"var1\"") != std::string::npos);
    EXPECT_TRUE(result.find("type=\"http://www.w3.org/TR/scxml\"") != std::string::npos);
    EXPECT_TRUE(result.find("conf:srcExpr") == std::string::npos);
    EXPECT_TRUE(result.find("id=\"var1\"") != std::string::npos);
    EXPECT_TRUE(result.find("expr=\"'foo'\"") != std::string::npos);
    EXPECT_TRUE(result.find("location=\"var1\"") != std::string::npos);
    EXPECT_TRUE(result.find("expr=\"'file:test216sub1.scxml'\"") != std::string::npos);
}

// ============================================================================
// W3C Test 225: Variable Equality Comparison (conf:VarEqVar)
// ============================================================================

TEST_F(TXMLConverterTest, ConvertsVarEqVarAttribute) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml initial="s0" version="1.0" conf:datamodel=""  xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance">
<datamodel>
  <data conf:id="1"/>
  <data conf:id="2"/>
</datamodel>
<state id="s0">
  <invoke type="http://www.w3.org/TR/scxml/" conf:idlocation="1">
    <content><scxml initial="subFinal1" version="1.0" conf:datamodel=""  xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance"><final id="subFinal1"/></scxml></content>
  </invoke>
  <invoke type="http://www.w3.org/TR/scxml/" conf:idlocation="2">
    <content><scxml initial="subFinal2" version="1.0" conf:datamodel=""  xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance"><final id="subFinal2"/></scxml></content>
  </invoke>
  <transition event="*" target="s1"/>
</state>
<state id="s1">
  <transition conf:VarEqVar="1 2" conf:targetfail=""/>
  <transition conf:targetpass=""/>
</state>
<conf:pass/>
<conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // VarEqVar="1 2" should convert to cond="var1 === var2" and target="fail"
    EXPECT_TRUE(result.find("cond=\"var1 === var2\"") != std::string::npos);
    EXPECT_TRUE(result.find("target=\"fail\"") != std::string::npos);
    EXPECT_TRUE(result.find("target=\"pass\"") != std::string::npos);
    EXPECT_TRUE(result.find("conf:VarEqVar") == std::string::npos);
}

// ============================================================================
// W3C Test 309: conf:nonBoolean attribute conversion (W3C SCXML 5.9)
// ============================================================================

// Test W3C SCXML 5.9: Non-boolean expressions must be treated as false
// conf:nonBoolean="" should convert to cond="return" which causes evaluation error → false
TEST_F(TXMLConverterTest, ConvertsNonBooleanAttributeToReturnStatement) {
    std::string txml = R"(<?xml version="1.0"?>
<scxml version="1.0" conf:datamodel="" xmlns="http://www.w3.org/2005/07/scxml" xmlns:conf="http://www.w3.org/2005/scxml-conformance" initial="s0">
<state id="s0">
  <transition conf:nonBoolean="" conf:targetfail=""/>
  <transition conf:targetpass=""/>
</state>
<conf:pass/>
<conf:fail/>
</scxml>)";

    std::string result = converter.convertTXMLToSCXML(txml);

    // conf:nonBoolean="" should convert to cond="return"
    // "return" statement causes JavaScript syntax error → evaluates to false
    EXPECT_TRUE(result.find("cond=\"return\"") != std::string::npos)
        << "Expected cond=\"return\" for non-boolean expression (W3C SCXML 5.9)";
    EXPECT_TRUE(result.find("target=\"fail\"") != std::string::npos);
    EXPECT_TRUE(result.find("target=\"pass\"") != std::string::npos);
    EXPECT_TRUE(result.find("conf:nonBoolean") == std::string::npos) << "conf:nonBoolean attribute should be removed";
}
