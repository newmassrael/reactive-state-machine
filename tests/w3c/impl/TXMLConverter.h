#pragma once

#include "../interfaces/ITestConverter.h"
#include <regex>
#include <unordered_map>

namespace RSM::W3C {

/**
 * @brief Comprehensive TXML to SCXML converter
 *
 * Converts W3C TXML test files to standard SCXML by:
 * - Removing all conf: namespace declarations and references
 * - Converting specific conf: attributes to SCXML equivalents
 * - Removing all other conf: attributes and elements
 */
class TXMLConverter : public ITestConverter {
private:
    // Namespace and structural patterns
    static const std::regex CONF_NAMESPACE_DECL;
    static const std::regex CONF_DATAMODEL_ATTR;

    // Target and state conversion patterns
    static const std::regex CONF_TARGETPASS_ATTR;
    static const std::regex CONF_TARGETFAIL_ATTR;
    static const std::regex CONF_PASS_ELEMENT;
    static const std::regex CONF_FAIL_ELEMENT;

    // Variable and expression patterns
    static const std::regex CONF_ISBOUND_ATTR;
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
    static const std::regex CONF_VAR_EXPR_ATTR;
    static const std::regex CONF_ID_VAL_ATTR;

    // Test 155 specific patterns
    static const std::regex CONF_SUMVARS_ELEMENT;

    // Event handling patterns
    static const std::regex CONF_EVENT_ATTR;
    static const std::regex CONF_TYPE_ATTR;
    static const std::regex CONF_SRC_ATTR;
    static const std::regex CONF_SENDIDEXPR_ATTR;
    static const std::regex CONF_TYPEEXPR_ATTR;
    // W3C SCXML invoke srcexpr attribute support for dynamic source evaluation
    static const std::regex CONF_SRCEXPR_ATTR;

    // Parameter and communication patterns
    static const std::regex CONF_NAME_ATTR;
    static const std::regex CONF_NAMELIST_ATTR;
    static const std::regex CONF_BASIC_HTTP_TARGET_ATTR;
    static const std::regex CONF_EVENT_RAW_ATTR;

    // Timing and delay patterns
    static const std::regex CONF_DELAY_ATTR;
    static const std::regex CONF_DELAY_FROM_VAR_ATTR;

    // Error handling and validation patterns
    static const std::regex CONF_INVALID_LOCATION_ATTR;
    static const std::regex CONF_INVALID_NAMELIST_ATTR;
    static const std::regex CONF_ILLEGAL_EXPR_ATTR;
    static const std::regex CONF_ILLEGAL_TARGET_ATTR;
    static const std::regex CONF_INVALID_SEND_TYPE_ATTR;

    // Value and data processing patterns
    static const std::regex CONF_SOME_INLINE_VAL_ATTR;
    static const std::regex CONF_EVENTDATA_SOME_VAL_ATTR;
    static const std::regex CONF_EVENT_NAMED_PARAM_HAS_VALUE_ATTR;
    static const std::regex CONF_QUOTE_EXPR_ATTR;
    static const std::regex CONF_EVENT_EXPR_ATTR;

    // Foreach element patterns
    static const std::regex CONF_ITEM_ATTR;
    static const std::regex CONF_INDEX_ATTR;
    static const std::regex CONF_ARRAYVAR_ATTR;

    // Array data patterns for W3C test data
    static const std::regex CONF_ARRAY123_PATTERN;
    static const std::regex CONF_ARRAY456_PATTERN;

    // Test 176 specific patterns - event data field access
    static const std::regex CONF_EVENTDATA_FIELD_VALUE_ATTR;
    static const std::regex CONF_IDVAL_COMPARISON_ATTR;

    // Test 240 specific patterns - namelist variable comparison
    static const std::regex CONF_NAMELISTIDVAL_COMPARISON_ATTR;

    // Test 183 specific patterns - send idlocation and variable binding
    static const std::regex CONF_IDLOCATION_ATTR;

    // Test 225 specific patterns - variable equality comparison
    static const std::regex CONF_VAREQVAR_ATTR;

    // W3C SCXML 5.8: Top-level script element pattern (test 302)
    static const std::regex CONF_SCRIPT_ELEMENT;

    // W3C SCXML 5.9: Non-boolean expression pattern (test 309)
    static const std::regex CONF_NONBOOLEAN_ATTR;

    // W3C SCXML 5.10: System variable binding check pattern (test 319)
    static const std::regex CONF_SYSTEMVARISBOUND_ATTR;

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