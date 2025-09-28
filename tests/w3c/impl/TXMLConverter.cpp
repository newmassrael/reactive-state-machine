#include "TXMLConverter.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace RSM::W3C {

// Pre-compiled regex patterns for performance
const std::regex TXMLConverter::CONF_NAMESPACE_DECL{
    R"abc(\s+xmlns:conf="http://www\.w3\.org/2005/scxml-conformance")abc", std::regex::optimize};

const std::regex TXMLConverter::CONF_DATAMODEL_ATTR{R"abc(conf:datamodel="")abc", std::regex::optimize};

const std::regex TXMLConverter::CONF_TARGETPASS_ATTR{R"abc(conf:targetpass="")abc", std::regex::optimize};

const std::regex TXMLConverter::CONF_TARGETFAIL_ATTR{R"abc(conf:targetfail="")abc", std::regex::optimize};

const std::regex TXMLConverter::CONF_PASS_ELEMENT{R"abc(<conf:pass\s*/>)abc", std::regex::optimize};

const std::regex TXMLConverter::CONF_FAIL_ELEMENT{R"abc(<conf:fail\s*/>)abc", std::regex::optimize};

const std::regex TXMLConverter::CONF_ISBOUND_ATTR{R"xyz(conf:isBound="([^"]*)")xyz", std::regex::optimize};

// Variable and expression patterns
const std::regex TXMLConverter::CONF_ID_ATTR{R"def(conf:id="([^"]*)")def", std::regex::optimize};

const std::regex TXMLConverter::CONF_EXPR_ATTR{R"def(conf:expr="([^"]*)")def", std::regex::optimize};

const std::regex TXMLConverter::CONF_LOCATION_ATTR{R"def(conf:location="([^"]*)")def", std::regex::optimize};

const std::regex TXMLConverter::CONF_COND_ATTR{R"def(conf:cond="([^"]*)")def", std::regex::optimize};

// Test 147 specific patterns
const std::regex TXMLConverter::CONF_TRUE_ATTR{R"abc(conf:true="")abc", std::regex::optimize};

const std::regex TXMLConverter::CONF_FALSE_ATTR{R"def(conf:false="")def", std::regex::optimize};

const std::regex TXMLConverter::CONF_INCREMENT_ID_ELEMENT{R"ghi(<conf:incrementID id="([^"]*)"\s*/>)ghi",
                                                          std::regex::optimize};

// Test 153 specific patterns
const std::regex TXMLConverter::CONF_COMPARE_ID_VAL_ATTR{R"abc(conf:compareIDVal="([^"]*)")abc", std::regex::optimize};

const std::regex TXMLConverter::CONF_VAR_EXPR_ATTR{R"def(conf:varExpr="([^"]*)")def", std::regex::optimize};

const std::regex TXMLConverter::CONF_ID_VAL_ATTR{R"ghi(conf:idVal="([^"]*)")ghi", std::regex::optimize};

// Generic sumVars pattern - matches any attributes like id1/id2, var1/var2, etc.
const std::regex TXMLConverter::CONF_SUMVARS_ELEMENT{R"(<conf:sumVars.*?>)", std::regex::optimize};

// Event handling patterns
const std::regex TXMLConverter::CONF_EVENT_ATTR{R"def(conf:event="([^"]*)")def", std::regex::optimize};

const std::regex TXMLConverter::CONF_TYPE_ATTR{R"def(conf:type="([^"]*)")def", std::regex::optimize};

const std::regex TXMLConverter::CONF_SRC_ATTR{R"def(conf:src="([^"]*)")def", std::regex::optimize};

// Parameter and communication patterns
const std::regex TXMLConverter::CONF_NAME_ATTR{R"abc(conf:name="([^"]*)")abc", std::regex::optimize};

const std::regex TXMLConverter::CONF_NAMELIST_ATTR{R"def(conf:namelist="([^"]*)")def", std::regex::optimize};

const std::regex TXMLConverter::CONF_BASIC_HTTP_TARGET_ATTR{R"ghi(conf:basicHTTPAccessURITarget="")ghi",
                                                            std::regex::optimize};

const std::regex TXMLConverter::CONF_EVENT_RAW_ATTR{R"jkl(conf:eventRaw="")jkl", std::regex::optimize};

// Timing and delay patterns
const std::regex TXMLConverter::CONF_DELAY_ATTR{R"ghi(conf:delay="([^"]*)")ghi", std::regex::optimize};
const std::regex TXMLConverter::CONF_DELAY_FROM_VAR_ATTR{R"hjk(conf:delayFromVar="([^"]*)")hjk", std::regex::optimize};

// Error handling and validation patterns
const std::regex TXMLConverter::CONF_INVALID_LOCATION_ATTR{R"jkl(conf:invalidLocation="([^"]*)")jkl",
                                                           std::regex::optimize};

const std::regex TXMLConverter::CONF_INVALID_NAMELIST_ATTR{R"mno(conf:invalidNamelist="([^"]*)")mno",
                                                           std::regex::optimize};

const std::regex TXMLConverter::CONF_ILLEGAL_EXPR_ATTR{R"pqr(conf:illegalExpr="([^"]*)")pqr", std::regex::optimize};

const std::regex TXMLConverter::CONF_ILLEGAL_TARGET_ATTR{R"xyz(conf:illegalTarget="([^"]*)")xyz", std::regex::optimize};

const std::regex TXMLConverter::CONF_INVALID_SEND_TYPE_ATTR{R"abc(conf:invalidSendType="([^"]*)")abc",
                                                            std::regex::optimize};

// Value and data processing patterns
const std::regex TXMLConverter::CONF_SOME_INLINE_VAL_ATTR{R"pqr(conf:someInlineVal="([^"]*)")pqr",
                                                          std::regex::optimize};

const std::regex TXMLConverter::CONF_EVENTDATA_SOME_VAL_ATTR{R"stu(conf:eventdataSomeVal="([^"]*)")stu",
                                                             std::regex::optimize};

const std::regex TXMLConverter::CONF_EVENT_NAMED_PARAM_HAS_VALUE_ATTR{R"vwx(conf:eventNamedParamHasValue="([^"]*)")vwx",
                                                                      std::regex::optimize};

const std::regex TXMLConverter::CONF_QUOTE_EXPR_ATTR{R"abc(conf:quoteExpr="([^"]*)")abc", std::regex::optimize};

const std::regex TXMLConverter::CONF_EVENT_EXPR_ATTR{R"def(conf:eventExpr="([^"]*)")def", std::regex::optimize};

// Foreach element patterns
const std::regex TXMLConverter::CONF_ITEM_ATTR{R"xyz(conf:item="([^"]*)")xyz", std::regex::optimize};

const std::regex TXMLConverter::CONF_INDEX_ATTR{R"uvw(conf:index="([^"]*)")uvw", std::regex::optimize};

const std::regex TXMLConverter::CONF_ARRAYVAR_ATTR{R"rst(conf:arrayVar="([^"]*)")rst", std::regex::optimize};

// Array data patterns for W3C test data
const std::regex TXMLConverter::CONF_ARRAY123_PATTERN{R"(<conf:array123\s*/>)", std::regex::optimize};

const std::regex TXMLConverter::CONF_ARRAY456_PATTERN{R"(<conf:array456\s*/>)", std::regex::optimize};

// Test 176 specific patterns - event data field access
const std::regex TXMLConverter::CONF_EVENTDATA_FIELD_VALUE_ATTR{R"def(conf:eventDataFieldValue="([^"]*)")def",
                                                                std::regex::optimize};

const std::regex TXMLConverter::CONF_IDVAL_COMPARISON_ATTR{R"def(conf:idVal="([0-9]+)=([0-9]+)")def",
                                                           std::regex::optimize};

// Test 183 specific patterns - send idlocation and variable binding
const std::regex TXMLConverter::CONF_IDLOCATION_ATTR{R"def(conf:idlocation="([^"]*)")def", std::regex::optimize};

// General patterns to remove all conf: references
const std::regex TXMLConverter::CONF_ALL_ATTRIBUTES{R"abc(\s+conf:[^=\s>]+\s*=\s*"[^"]*")abc", std::regex::optimize};

const std::regex TXMLConverter::CONF_ALL_ELEMENTS{R"abc(<conf:[^>]*/>|<conf:[^>]*>.*?</conf:[^>]*>)abc",
                                                  std::regex::optimize};

std::string TXMLConverter::convertTXMLToSCXML(const std::string &txml) {
    if (txml.empty()) {
        throw std::invalid_argument("TXML content cannot be empty");
    }

    try {
        std::string scxml = applyTransformations(txml);
        validateSCXML(scxml);
        return scxml;
    } catch (const std::exception &e) {
        throw std::runtime_error("TXML to SCXML conversion failed: " + std::string(e.what()));
    }
}

std::string TXMLConverter::applyTransformations(const std::string &txml) {
    std::string result = txml;

    // Apply transformations in order
    result = removeConfNamespace(result);
    result = convertConfAttributes(result);
    result = convertConfElements(result);

    return result;
}

std::string TXMLConverter::removeConfNamespace(const std::string &content) {
    // Remove the conf: namespace declaration from <scxml> root element
    return std::regex_replace(content, CONF_NAMESPACE_DECL, "");
}

std::string TXMLConverter::convertConfAttributes(const std::string &content) {
    std::string result = content;

    // First, convert specific conf: attributes that have SCXML equivalents
    result = std::regex_replace(result, CONF_COND_ATTR, R"(cond="$1")");

    // Convert conf:datamodel to ecmascript datamodel
    result = std::regex_replace(result, CONF_DATAMODEL_ATTR, R"(datamodel="ecmascript")");

    // Convert conf:targetpass and conf:targetfail attributes
    result = std::regex_replace(result, CONF_TARGETPASS_ATTR, R"(target="pass")");
    result = std::regex_replace(result, CONF_TARGETFAIL_ATTR, R"(target="fail")");

    // Convert conf:isBound to typeof checks
    // Handle numeric variables: conf:isBound="1" -> cond="typeof var1 !== 'undefined'"
    std::regex isbound_numeric_pattern(R"xyz(conf:isBound="([0-9]+)")xyz");
    std::regex isbound_general_pattern(R"xyz(conf:isBound="([^"]*)")xyz");
    result = std::regex_replace(result, isbound_numeric_pattern, R"(cond="typeof var$1 !== 'undefined'")");
    result = std::regex_replace(result, isbound_general_pattern, R"(cond="typeof $1 !== 'undefined'")");

    // Handle numeric id attributes: conf:id="1" -> id="var1"
    std::regex id_numeric_pattern(R"def(conf:id="([0-9]+)")def");
    result = std::regex_replace(result, id_numeric_pattern, R"(id="var$1")");
    // Convert remaining conf:id attributes to standard id
    result = std::regex_replace(result, CONF_ID_ATTR, R"(id="$1")");

    // Handle literal numeric expr attributes: conf:expr="0" -> expr="0", conf:expr="1" -> expr="1", etc.
    // These are literal values, not variable references
    // FIXED: W3C test 153 bug - was incorrectly converting conf:expr="1" -> expr="var1"
    // Should be conf:expr="1" -> expr="1" (literal value, not variable reference)
    std::regex expr_literal_pattern(R"def(conf:expr="([0-9]+)")def");
    result = std::regex_replace(result, expr_literal_pattern, R"(expr="$1")");

    // Convert remaining conf:expr attributes to standard expr
    result = std::regex_replace(result, CONF_EXPR_ATTR, R"(expr="$1")");

    // Convert Test 147 specific boolean condition attributes
    result = std::regex_replace(result, CONF_TRUE_ATTR, R"(cond="true")");
    result = std::regex_replace(result, CONF_FALSE_ATTR, R"(cond="false")");

    // Convert event handling attributes
    result = std::regex_replace(result, CONF_EVENT_ATTR, R"(event="$1")");
    result = std::regex_replace(result, CONF_TYPE_ATTR, R"(type="$1")");
    result = std::regex_replace(result, CONF_SRC_ATTR, R"(src="$1")");

    // Convert parameter and communication attributes
    // Handle numeric name attributes: conf:name="1" -> name="var1"
    std::regex name_numeric_pattern(R"abc(conf:name="([0-9]+)")abc");
    result = std::regex_replace(result, name_numeric_pattern, R"(name="var$1")");
    // Convert remaining conf:name attributes to standard name
    result = std::regex_replace(result, CONF_NAME_ATTR, R"(name="$1")");

    // Handle numeric namelist attributes: conf:namelist="1" -> namelist="var1"
    std::regex namelist_numeric_pattern(R"def(conf:namelist="([0-9]+)")def");
    result = std::regex_replace(result, namelist_numeric_pattern, R"(namelist="var$1")");
    // Convert remaining conf:namelist attributes to standard namelist
    result = std::regex_replace(result, CONF_NAMELIST_ATTR, R"(namelist="$1")");

    // Convert HTTP target attributes (remove as they are test-specific)
    result = std::regex_replace(result, CONF_BASIC_HTTP_TARGET_ATTR, R"(target="http://localhost:8080/test")");

    // Convert event raw attributes (remove as they are test-specific)
    result = std::regex_replace(result, CONF_EVENT_RAW_ATTR, R"(expr="_event.raw")");

    // Convert timing and delay attributes
    result = std::regex_replace(result, CONF_DELAY_ATTR, R"(delay="$1")");

    // Convert conf:delayFromVar to delayexpr with variable prefix for numeric values
    // conf:delayFromVar="1" -> delayexpr="var1" (W3C Test 175)
    std::regex delay_from_var_numeric_pattern(R"hjk(conf:delayFromVar="([0-9]+)")hjk");
    result = std::regex_replace(result, delay_from_var_numeric_pattern, R"(delayexpr="var$1")");
    result = std::regex_replace(result, CONF_DELAY_FROM_VAR_ATTR, R"(delayexpr="$1")");

    // Convert error handling and validation attributes
    result = std::regex_replace(result, CONF_INVALID_LOCATION_ATTR, R"(location="$1")");
    result = std::regex_replace(result, CONF_INVALID_NAMELIST_ATTR, R"(namelist="$1")");

    // Convert conf:illegalExpr to expr with intentionally invalid JavaScript expression
    // W3C test 156: should cause error to stop foreach execution
    result = std::regex_replace(result, CONF_ILLEGAL_EXPR_ATTR, R"(expr="undefined.invalidProperty")");

    // Convert conf:illegalTarget by removing event attribute to cause send element error
    // W3C test 159: should cause error to stop executable content processing
    // Missing event attribute in send element will cause execution error
    // Pattern handles both: <send event="..." conf:illegalTarget=""/> and <send conf:illegalTarget="" event="..."/>
    std::regex illegal_target_pattern1(R"((<send[^>]*conf:illegalTarget="[^"]*"[^>]*) +event="[^"]*"([^>]*>))");
    std::regex illegal_target_pattern2(R"((<send[^>]*) +event="[^"]*"([^>]*conf:illegalTarget="[^"]*"[^>]*>))");
    result = std::regex_replace(result, illegal_target_pattern1, R"($1$2)");
    result = std::regex_replace(result, illegal_target_pattern2, R"($1$2)");
    // Then remove the conf:illegalTarget attribute itself
    result = std::regex_replace(result, CONF_ILLEGAL_TARGET_ATTR, "");

    // Convert conf:invalidSendType by adding invalid type attribute to create send type error
    // W3C test 199: should cause error to be generated for unsupported send type
    // Pattern: <send conf:invalidSendType="" event="..."/> -> <send type="unsupported_type" event="..."/>
    std::regex invalid_send_type_pattern(R"((<send[^>]*) +conf:invalidSendType="[^"]*"([^>]*>))");
    result = std::regex_replace(result, invalid_send_type_pattern, R"($1 type="unsupported_type"$2)");
    // Then remove the conf:invalidSendType attribute itself
    result = std::regex_replace(result, CONF_INVALID_SEND_TYPE_ATTR, "");

    // Convert value and data processing attributes
    // conf:eventdataSomeVal should be converted to platform-specific event data
    // For standard SCXML, map to 'name' attribute for param elements
    result = std::regex_replace(result, CONF_EVENTDATA_SOME_VAL_ATTR, R"(name="$1")");

    // conf:eventNamedParamHasValue should be converted to platform-specific event parameter validation
    // For ECMAScript datamodel, map to 'expr' attribute for conditional checks
    result = std::regex_replace(result, CONF_EVENT_NAMED_PARAM_HAS_VALUE_ATTR, R"(expr="$1")");

    // Convert conf:quoteExpr to quoted string expressions
    // conf:quoteExpr="event1" -> expr="'event1'" (string literal)
    result = std::regex_replace(result, CONF_QUOTE_EXPR_ATTR, R"(expr="'$1'")");

    // Convert conf:eventExpr to eventexpr for send elements
    // Handle numeric variables: conf:eventExpr="1" -> eventexpr="var1"
    std::regex eventexpr_numeric_pattern(R"def(conf:eventExpr="([0-9]+)")def");
    result = std::regex_replace(result, eventexpr_numeric_pattern, R"(eventexpr="var$1")");
    // Convert remaining conf:eventExpr attributes to standard eventexpr
    result = std::regex_replace(result, CONF_EVENT_EXPR_ATTR, R"(eventexpr="$1")");

    // Convert foreach element attributes with numeric variable name handling
    // JavaScript 호환성: 숫자 변수명은 var prefix 추가

    // Convert conf:item with numeric handling
    std::regex item_numeric_pattern(R"xyz(conf:item="([0-9]+)")xyz");
    std::regex item_general_pattern(R"xyz(conf:item="([^"]*)")xyz");
    result = std::regex_replace(result, item_numeric_pattern, R"(item="var$1")");
    result = std::regex_replace(result, item_general_pattern, R"(item="$1")");

    // Convert conf:index with numeric handling
    std::regex index_numeric_pattern(R"uvw(conf:index="([0-9]+)")uvw");
    std::regex index_general_pattern(R"uvw(conf:index="([^"]*)")uvw");
    result = std::regex_replace(result, index_numeric_pattern, R"(index="var$1")");
    result = std::regex_replace(result, index_general_pattern, R"(index="$1")");

    // Convert conf:arrayVar with numeric handling
    std::regex arrayvar_numeric_pattern(R"xyz(conf:arrayVar="([0-9]+)")xyz");
    std::regex arrayvar_general_pattern(R"xyz(conf:arrayVar="([^"]*)")xyz");
    result = std::regex_replace(result, arrayvar_numeric_pattern, R"(array="var$1")");
    result = std::regex_replace(result, arrayvar_general_pattern, R"(array="$1")");

    // Convert Test 153 specific attributes
    // First handle common comparison patterns
    std::regex compare_1_lt_2(R"abc(conf:compareIDVal="1&lt;2")abc");
    std::regex compare_3_gte_4(R"abc(conf:compareIDVal="3&gt;=4")abc");
    result = std::regex_replace(result, compare_1_lt_2, R"(cond="var1 &lt; var2")");
    result = std::regex_replace(result, compare_3_gte_4, R"(cond="var3 &gt;= var4")");

    // Generic conf:compareIDVal pattern for other cases
    result = std::regex_replace(result, CONF_COMPARE_ID_VAL_ATTR, R"(cond="$1")");

    // conf:varExpr="2" -> expr="var2" (for numeric IDs)
    std::regex varexpr_numeric_pattern(R"def(conf:varExpr="([0-9]+)")def");
    std::regex varexpr_general_pattern(R"def(conf:varExpr="([^"]*)")def");
    result = std::regex_replace(result, varexpr_numeric_pattern, R"(expr="var$1")");
    result = std::regex_replace(result, varexpr_general_pattern, R"(expr="$1")");

    // Test 176 specific patterns
    // conf:eventDataFieldValue="aParam" -> expr="_event.data.aParam"
    result = std::regex_replace(result, CONF_EVENTDATA_FIELD_VALUE_ATTR, R"(expr="_event.data.$1")");

    // conf:idVal common patterns
    std::regex idval_4_eq_0(R"ghi(conf:idVal="4=0")ghi");
    std::regex idval_1_ne_5(R"ghi(conf:idVal="1!=5")ghi");
    std::regex idval_1_eq_1(R"ghi(conf:idVal="1=1")ghi");
    std::regex idval_1_eq_0(R"ghi(conf:idVal="1=0")ghi");
    std::regex idval_1_eq_6(R"ghi(conf:idVal="1=6")ghi");  // W3C test 155
    std::regex idval_2_eq_2(R"ghi(conf:idVal="2=2")ghi");  // W3C test 176
    result = std::regex_replace(result, idval_4_eq_0, R"(cond="var4 == 0")");
    result = std::regex_replace(result, idval_1_ne_5, R"(cond="var1 != var5")");
    result = std::regex_replace(result, idval_1_eq_1, R"(cond="var1 == 1")");
    result = std::regex_replace(result, idval_1_eq_0, R"(cond="var1 == 0")");
    result = std::regex_replace(result, idval_1_eq_6, R"(cond="var1 == 6")");
    result = std::regex_replace(result, idval_2_eq_2, R"(cond="var2 == 2")");

    // Generic conf:idVal pattern with variable replacement using the new pattern
    result = std::regex_replace(result, CONF_IDVAL_COMPARISON_ATTR, R"(cond="var$1 == $2")");

    // Test 183 specific patterns
    // conf:idlocation="1" -> idlocation="var1" (for numeric IDs)
    std::regex idlocation_numeric_pattern(R"def(conf:idlocation="([0-9]+)")def");
    std::regex idlocation_general_pattern(R"def(conf:idlocation="([^"]*)")def");
    result = std::regex_replace(result, idlocation_numeric_pattern, R"(idlocation="var$1")");
    result = std::regex_replace(result, idlocation_general_pattern, R"(idlocation="$1")");

    // Legacy generic conf:idVal pattern for other cases
    result = std::regex_replace(result, CONF_ID_VAL_ATTR, R"(cond="$1")");

    // Convert numeric location attributes to use var prefix (if not already handled)
    std::regex location_numeric_pattern(R"def(conf:location="([0-9]+)")def");
    std::regex location_general_pattern(R"def(conf:location="([^"]*)")def");
    result = std::regex_replace(result, location_numeric_pattern, R"(location="var$1")");
    result = std::regex_replace(result, location_general_pattern, R"(location="$1")");

    // Then remove ALL remaining conf: attributes (test framework specific)
    result = std::regex_replace(result, CONF_ALL_ATTRIBUTES, "");

    return result;
}

std::string TXMLConverter::convertConfElements(const std::string &content) {
    std::string result = content;

    // First, convert specific conf: elements that have SCXML equivalents
    result = std::regex_replace(result, CONF_PASS_ELEMENT, R"(<final id="pass"/>)");
    result = std::regex_replace(result, CONF_FAIL_ELEMENT, R"(<final id="fail"/>)");

    // Convert W3C test data array elements to JavaScript arrays
    result = std::regex_replace(result, CONF_ARRAY123_PATTERN, "[1,2,3]");
    result = std::regex_replace(result, CONF_ARRAY456_PATTERN, "[4,5,6]");

    // Convert conf:incrementID elements to assign increment operations
    // Pattern: <conf:incrementID id="1"/> -> <assign location="var1" expr="var1 + 1"/>
    std::regex increment_numeric_pattern(R"ghi(<conf:incrementID id="([0-9]+)"\s*/>)ghi");
    std::regex increment_general_pattern(R"jkl(<conf:incrementID id="([^"]+)"\s*/>)jkl");
    result = std::regex_replace(result, increment_numeric_pattern, R"(<assign location="var$1" expr="var$1 + 1"/>)");
    result = std::regex_replace(result, increment_general_pattern, R"(<assign location="$1" expr="$1 + 1"/>)");

    // Convert conf:sumVars elements to assign sum operations
    // Handle conf:sumVars elements with simple pattern matching
    // W3C test 155 specific: <conf:sumVars id1="1" id2="2"/>
    std::regex sumvars_id1_id2(R"ZZZ(<conf:sumVars id1="([^"]*)" id2="([^"]*)" */>)ZZZ");
    result = std::regex_replace(result, sumvars_id1_id2, R"ZZZ(<assign location="var$1" expr="var$1 + var$2"/>)ZZZ");

    // Other common patterns can be added here as needed
    std::regex sumvars_dest_id(R"ZZZ(<conf:sumVars dest="([^"]*)" id="([^"]*)" */>)ZZZ");
    result = std::regex_replace(result, sumvars_dest_id, R"ZZZ(<assign location="var$1" expr="var$1 + var$2"/>)ZZZ");

    // Then remove ALL remaining conf: elements (test framework specific)
    result = std::regex_replace(result, CONF_ALL_ELEMENTS, "");

    return result;
}

void TXMLConverter::validateSCXML(const std::string &scxml) {
    // Basic validation checks
    if (scxml.find("<scxml") == std::string::npos) {
        throw std::invalid_argument("Converted content does not contain <scxml> element");
    }

    if (scxml.find("</scxml>") == std::string::npos) {
        throw std::invalid_argument("Converted content does not contain closing </scxml> tag");
    }

    // Check that conf: namespace references are removed (excluding comments)
    std::string contentWithoutComments = scxml;
    std::regex commentPattern(R"(<!--[\s\S]*?-->)", std::regex::optimize);
    contentWithoutComments = std::regex_replace(contentWithoutComments, commentPattern, "");

    if (contentWithoutComments.find("conf:") != std::string::npos) {
        throw std::runtime_error("Conversion incomplete: conf: namespace references still present");
    }

    // Ensure we have pass/fail targets for W3C test validation
    bool hasPassTarget =
        (scxml.find(R"(target="pass")") != std::string::npos) || (scxml.find(R"(id="pass")") != std::string::npos);
    bool hasFailTarget =
        (scxml.find(R"(target="fail")") != std::string::npos) || (scxml.find(R"(id="fail")") != std::string::npos);

    if (!hasPassTarget && !hasFailTarget) {
        throw std::runtime_error("Converted SCXML must have pass or fail targets for W3C compliance testing");
    }
}

}  // namespace RSM::W3C