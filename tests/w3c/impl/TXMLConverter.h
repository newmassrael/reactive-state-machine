#pragma once

#include "../interfaces/ITestConverter.h"
#include <regex>
#include <unordered_map>

namespace RSM::W3C {

/**
 * @brief Comprehensive TXML to SCXML converter for W3C SCXML Test Suite
 *
 * Transforms Test XML (TXML) format used in W3C SCXML conformance tests into
 * standard SCXML by converting conf: namespace attributes to their SCXML equivalents.
 *
 * ## Purpose
 * W3C SCXML tests use TXML format with conf: namespace to:
 * - Abstract test requirements from implementation details
 * - Provide datamodel-independent test specifications
 * - Simplify test authoring with high-level assertions
 *
 * ## Conversion Process
 * 1. **Namespace Cleanup**: Remove conf: namespace declarations
 * 2. **Pattern Matching**: Apply 50+ regex transformations for specific test patterns
 * 3. **Attribute Conversion**: Transform conf: attributes to standard SCXML
 * 4. **Element Replacement**: Convert conf: elements (pass/fail/script/etc.)
 * 5. **Validation**: Ensure output is valid SCXML
 *
 * ## Pattern Categories
 *
 * ### Core Test Infrastructure (Tests: all)
 * - **conf:datamodel**: Empty datamodel attribute → removed
 * - **conf:targetpass**: Target to pass state → target="pass"
 * - **conf:targetfail**: Target to fail state → target="fail"
 * - **<conf:pass/>**: Test pass state → <final id="pass"/>
 * - **<conf:fail/>**: Test fail state → <final id="fail"/>
 *
 * ### Variable Operations (Tests: 147, 153, 155)
 * - **conf:id**: Variable identifier → id="Var{N}"
 * - **conf:expr**: Variable expression → expr="{value}"
 * - **conf:location**: Variable location → location="Var{N}"
 * - **<conf:incrementID id="1"/>**: Increment variable → <assign location="Var1" expr="Var1 + 1"/>
 * - **<conf:sumVars/>**: Sum variables → dynamic generation based on attributes
 *
 * ### Event System (Tests: 176, 318, 331, 332, 336, 342)
 * - **conf:event**: Event name → event="{name}"
 * - **conf:eventExpr**: Event expression → eventexpr="{expr}"
 * - **conf:eventField**: Access event field → expr="_event.{field}"
 * - **conf:eventName**: Access event name → expr="_event.name"
 * - **conf:eventType**: Access event type → expr="_event.type"
 * - **conf:eventSendid**: Access send ID → expr="_event.sendid"
 * - **<conf:sendToSender name="bar"/>**: Reply to sender → <send event="bar" targetexpr="_event.origin"/>
 *
 * ### System Variables (Tests: 319, 321, 329, 500)
 * - **conf:systemVarIsBound**: Check binding → cond="typeof {var} !== 'undefined'"
 * - **conf:systemVarExpr**: System var expr → expr="{var}"
 * - **conf:systemVarLocation**: System var loc → location="{var}"
 * - **conf:scxmlEventIOLocation**: I/O processor location → expr="_ioprocessors['scxml']['location']"
 *
 * ### Communication (Tests: 183, 210, 240, 354, 496)
 * - **conf:sendIDExpr**: Send ID expression → sendidexpr="{expr}"
 * - **conf:basicHTTPAccessURITarget**: HTTP target → target="{uri}"
 * - **conf:unreachableTarget**: Invalid target → targetexpr="undefined"
 * - **conf:eventDataNamelistValue**: Namelist data → expr="_event.data.Var{N}"
 * - **conf:eventDataParamValue**: Param data → expr="_event.data.{param}"
 *
 * ### Control Flow (Tests: 147, 309, 310, 445, 446)
 * - **conf:true**: True condition → cond="true"
 * - **conf:false**: False condition → cond="false"
 * - **conf:nonBoolean**: Non-boolean cond → cond="return" (syntax error → false)
 * - **conf:inState**: In() predicate → cond="In('{state}')"
 * - **conf:item/index/arrayVar**: Foreach attrs → item="{var}" index="{idx}" array="{arr}"
 *
 * ### Error Handling (Tests: 296, 298, 300, etc.)
 * - **conf:invalidLocation**: Invalid location → location="!@#$%"
 * - **conf:illegalExpr**: Illegal expression → expr="return"
 * - **conf:illegalTarget**: Illegal target → target="!invalid"
 * - **conf:invalidSendType**: Invalid send type → type="invalid://type"
 *
 * ### Timing (Tests: 175, 185-187, etc.)
 * - **conf:delay**: Delay value → delay="{duration}"
 * - **conf:delayFromVar**: Delay from variable → delayexpr="Var{N}"
 *
 * ## Implementation Notes
 * - Uses pre-compiled std::regex patterns with std::regex::optimize for performance
 * - Raw string literals with unique delimiters prevent escaping conflicts
 * - Patterns applied in specific order to handle dependencies
 * - Validates output SCXML structure after transformation
 *
 * @see W3C SCXML 1.0 Specification: https://www.w3.org/TR/scxml/
 * @see SCXML Test Suite: https://www.w3.org/Voice/2013/scxml-irp/
 */
class TXMLConverter : public ITestConverter {
private:
    // ========================================================================
    // Test Environment Configuration Constants
    // ========================================================================

    /**
     * @brief Default HTTP test server URL for BasicHTTPEventProcessor tests
     * @see W3C Test 201: BasicHTTPEventProcessor target URL
     */
    static constexpr const char *HTTP_TEST_SERVER_URL = "http://localhost:8080/test";

    // ========================================================================
    // Regex Pattern Definitions
    // ========================================================================

    // Namespace and structural patterns
    static const std::regex CONF_NAMESPACE_DECL;
    static const std::regex CONF_DATAMODEL_ATTR;

    // Target and state conversion patterns
    static const std::regex CONF_TARGETPASS_ATTR;
    static const std::regex CONF_TARGETFAIL_ATTR;
    static const std::regex CONF_PASS_ELEMENT;
    static const std::regex CONF_FAIL_ELEMENT;

    // Variable and expression patterns
    static const std::regex CONF_ID_ATTR;
    static const std::regex CONF_EXPR_ATTR;
    static const std::regex CONF_LOCATION_ATTR;
    static const std::regex CONF_COND_ATTR;

    // Test 147 specific patterns
    static const std::regex CONF_TRUE_ATTR;
    static const std::regex CONF_FALSE_ATTR;
    static const std::regex CONF_INCREMENT_ID_ELEMENT;

    // Test 153 specific patterns
    static const std::regex CONF_COMPARE_ID_VAL_ATTR;
    // conf:varExpr="1" -> expr="Var1" (numeric IDs for Tests 153, 186)
    static const std::regex CONF_VAR_EXPR_NUMERIC_ATTR;
    // conf:varExpr="varname" -> expr="varname" (general)
    static const std::regex CONF_VAR_EXPR_ATTR;
    static const std::regex CONF_ID_VAL_ATTR;

    // Test 155 specific patterns
    static const std::regex CONF_SUMVARS_ELEMENT;

    // Test 530 specific patterns - invoke content with variable expression
    static const std::regex CONF_VARCHILDEXPR_ATTR;

    // Event handling patterns
    static const std::regex CONF_EVENT_ATTR;
    static const std::regex CONF_TYPE_ATTR;
    static const std::regex CONF_SRC_ATTR;
    // conf:sendIDExpr="1" -> sendidexpr="Var1" (numeric IDs for Test 210)
    static const std::regex CONF_SENDIDEXPR_NUMERIC_ATTR;
    // conf:sendIDExpr="varname" -> sendidexpr="varname" (general)
    static const std::regex CONF_SENDIDEXPR_ATTR;
    // conf:typeExpr="1" -> typeexpr="Var1" (numeric IDs for Test 215)
    static const std::regex CONF_TYPEEXPR_NUMERIC_ATTR;
    // conf:typeExpr="varname" -> typeexpr="varname" (general)
    static const std::regex CONF_TYPEEXPR_ATTR;
    // conf:srcExpr="1" -> srcexpr="Var1" (numeric IDs for Test 216)
    static const std::regex CONF_SRCEXPR_NUMERIC_ATTR;
    // conf:srcExpr="varname" -> srcexpr="varname" (general)
    static const std::regex CONF_SRCEXPR_ATTR;

    // Parameter and communication patterns
    static const std::regex CONF_NAME_ATTR;
    // conf:name="1" -> name="Var1" (numeric IDs for Test 226)
    static const std::regex CONF_NAME_NUMERIC_ATTR;
    static const std::regex CONF_NAMELIST_ATTR;
    // conf:namelist="1" -> namelist="Var1" (numeric IDs for Test 226)
    static const std::regex CONF_NAMELIST_NUMERIC_ATTR;
    static const std::regex CONF_BASIC_HTTP_TARGET_ATTR;
    static const std::regex CONF_EVENT_RAW_ATTR;

    // Timing and delay patterns
    // conf:delay="1" -> delay="1s" (numeric values with CSS2 spec suffix)
    static const std::regex CONF_DELAY_NUMERIC_ATTR;
    // conf:delay="varname" -> delay="varname" (general)
    static const std::regex CONF_DELAY_ATTR;

    // conf:delayFromVar="1" -> delayexpr="Var1" (numeric IDs for Test 175)
    static const std::regex CONF_DELAY_FROM_VAR_NUMERIC_ATTR;
    // conf:delayFromVar="varname" -> delayexpr="varname" (general)
    static const std::regex CONF_DELAY_FROM_VAR_ATTR;

    // Error handling and validation patterns
    static const std::regex CONF_INVALID_LOCATION_ATTR;
    static const std::regex CONF_INVALID_NAMELIST_ATTR;
    static const std::regex CONF_ILLEGAL_EXPR_ATTR;
    static const std::regex CONF_ILLEGAL_TARGET_ATTR;
    static const std::regex CONF_INVALID_SEND_TYPE_ATTR;
    // W3C SCXML C.1: Unreachable target pattern (test 496)
    static const std::regex CONF_UNREACHABLETARGET_ATTR;

    // Value and data processing patterns
    static const std::regex CONF_SOME_INLINE_VAL_ATTR;
    static const std::regex CONF_EVENTDATA_SOME_VAL_ATTR;
    static const std::regex CONF_EVENT_NAMED_PARAM_HAS_VALUE_ATTR;
    static const std::regex CONF_QUOTE_EXPR_ATTR;
    static const std::regex CONF_EVENT_EXPR_ATTR;
    // conf:targetExpr="1" -> targetexpr="Var1" (numeric IDs for Test 173)
    static const std::regex CONF_TARGETEXPR_NUMERIC_ATTR;
    // conf:targetExpr="varname" -> targetexpr="varname" (general)
    static const std::regex CONF_TARGETEXPR_ATTR;

    // W3C SCXML 5.10: Event field access pattern (test 342)
    static const std::regex CONF_EVENTFIELD_ATTR;

    // W3C SCXML 5.10: Event name access pattern (test 318)
    // <assign conf:location="X" conf:eventName=""/> converts to <assign location="varX" expr="_event.name"/>
    static const std::regex CONF_EVENTNAME_ATTR;

    // W3C SCXML 5.10: Event type access pattern (test 331)
    // <assign conf:location="X" conf:eventType=""/> converts to <assign location="varX" expr="_event.type"/>
    static const std::regex CONF_EVENTTYPE_ATTR;

    // Foreach element patterns
    static const std::regex CONF_ITEM_ATTR;
    static const std::regex CONF_INDEX_ATTR;
    static const std::regex CONF_ARRAYVAR_ATTR;

    // Array data patterns for W3C test data
    static const std::regex CONF_ARRAY123_PATTERN;
    static const std::regex CONF_ARRAY456_PATTERN;

    // Event data field access (Tests: 176, 186, 205, 233, 234)
    static const std::regex CONF_EVENTDATA_FIELD_VALUE_ATTR;

    // Event data value validation patterns (Tests: 179, 294, 527, 529)
    // conf:eventdataVal="123" converts to cond="_event.data == 123"
    // conf:eventdataVal="'foo'" converts to cond="_event.data == 'foo'"
    static const std::regex CONF_EVENTDATAVAL_ATTR;

    // Event data variable value validation pattern (Test: 294)
    // conf:eventvarVal="1=1" converts to cond="_event.data.Var1 == 1"
    static const std::regex CONF_EVENTVARVAL_ATTR;

    // Test 354 specific patterns - namelist and param data access
    static const std::regex CONF_EVENTDATA_NAMELIST_VALUE_ATTR;
    static const std::regex CONF_EVENTDATA_PARAM_VALUE_ATTR;
    static const std::regex CONF_IDVAL_COMPARISON_ATTR;

    // Test 332 specific patterns - event sendid field access
    static const std::regex CONF_EVENTSENDID_ATTR;

    // Test 198 specific patterns - event origintype validation
    static const std::regex CONF_ORIGINTYPEEQ_ATTR;

    // Test 240 specific patterns - namelist variable comparison
    static const std::regex CONF_NAMELISTIDVAL_COMPARISON_ATTR;

    // Test 183 specific patterns - send idlocation and variable binding
    // conf:idlocation="1" -> idlocation="Var1" (numeric IDs)
    static const std::regex CONF_IDLOCATION_NUMERIC_ATTR;
    // conf:idlocation="varname" -> idlocation="varname" (general)
    static const std::regex CONF_IDLOCATION_GENERAL_ATTR;

    // Variable binding check patterns (Tests: 183, 150, 151, etc.)
    // conf:isBound="1" -> cond="typeof Var1 !== 'undefined'" (numeric IDs)
    static const std::regex CONF_ISBOUND_NUMERIC_ATTR;
    // conf:isBound="varname" -> cond="typeof varname !== 'undefined'" (general)
    static const std::regex CONF_ISBOUND_GENERAL_ATTR;

    // Test 225 specific patterns - variable equality comparison
    static const std::regex CONF_VAREQVAR_ATTR;

    // Test 224 specific patterns - variable prefix check (string starts with)
    // conf:varPrefix="2 1" -> cond="Var1.indexOf(Var2) === 0" (checks if Var1 starts with Var2)
    static const std::regex CONF_VARPREFIX_ATTR;

    // W3C SCXML 5.8: Top-level script element pattern (test 302)
    static const std::regex CONF_SCRIPT_ELEMENT;

    // W3C SCXML 5.9: Non-boolean expression pattern (test 309)
    static const std::regex CONF_NONBOOLEAN_ATTR;

    // W3C SCXML 5.9: In() predicate pattern (test 310)
    // <transition conf:inState="s1" target="pass"/> converts to <transition cond="In('s1')" target="pass"/>
    static const std::regex CONF_INSTATE_ATTR;

    // W3C SCXML 5.10: System variable binding check pattern (test 319)
    static const std::regex CONF_SYSTEMVARISBOUND_ATTR;

    // W3C SCXML 5.10: System variable expression pattern (test 321)
    static const std::regex CONF_SYSTEMVAREXPR_ATTR;

    // W3C SCXML 5.10: System variable location pattern (test 329)
    static const std::regex CONF_SYSTEMVARLOCATION_ATTR;

    // W3C SCXML 5.10: Invalid session ID pattern (test 329)
    static const std::regex CONF_INVALIDSESSIONID_ATTR;

    // W3C SCXML 5.10: System variable value comparison pattern (test 329)
    static const std::regex CONF_IDSYSTEMVARVAL_ATTR;

    // W3C SCXML 5.10: ID quote value comparison pattern (test 318)
    static const std::regex CONF_IDQUOTEVAL_ATTR;

    // W3C SCXML 5.10: Send to sender pattern (test 336)
    static const std::regex CONF_SENDTOSENDER_ELEMENT;

    // W3C SCXML C.1: SCXML Event I/O Processor location pattern (test 500)
    static const std::regex CONF_SCXMLEVENTIOLOCATION_ATTR;

    // General patterns to remove all remaining conf: references
    static const std::regex CONF_ALL_ATTRIBUTES;
    static const std::regex CONF_ALL_ELEMENTS;

    /**
     * @brief Apply all transformation rules to TXML content
     * @param txml Original TXML content
     * @return Transformed SCXML content
     */
    std::string applyTransformations(const std::string &txml);

    /**
     * @brief Remove conf: namespace declaration
     */
    std::string removeConfNamespace(const std::string &content);

    /**
     * @brief Convert conf: attributes to standard SCXML
     */
    std::string convertConfAttributes(const std::string &content);

    /**
     * @brief Convert conf: elements to final states
     */
    std::string convertConfElements(const std::string &content);

    /**
     * @brief Validate that conversion produced valid SCXML
     */
    void validateSCXML(const std::string &scxml);

public:
    TXMLConverter() = default;
    ~TXMLConverter() override = default;

    /**
     * @brief Convert TXML content to valid SCXML
     * @param txml The TXML content with conf: namespace attributes
     * @return Valid SCXML content ready for RSM parsing
     * @throws std::invalid_argument if TXML is malformed
     * @throws std::runtime_error if conversion fails
     */
    std::string convertTXMLToSCXML(const std::string &txml) override;

    /**
     * @brief Convert TXML to SCXML without W3C validation
     * @param txml TXML content to convert
     * @return Converted SCXML content
     * @throws std::invalid_argument if TXML is malformed
     * @note Useful for converting sub-files that don't have pass/fail targets
     */
    std::string convertTXMLToSCXMLWithoutValidation(const std::string &txml);

private:
    /**
     * @brief Get script content for W3C test (W3C SCXML 5.8)
     * @return Script content string for the test
     * @note Maps test-specific script content for conf:script conversion
     */
    static std::string getDefaultScriptContent();
};

}  // namespace RSM::W3C