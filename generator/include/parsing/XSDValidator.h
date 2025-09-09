#pragma once

#include <libxml++/libxml++.h>
#include <libxml/xmlschemas.h>
#include <memory>
#include <string>
#include <vector>

namespace SCXML {
namespace Parsing {

/**
 * @brief Validation error information
 */
struct ValidationError {
    std::string message;
    int line;
    int column;
    enum class Severity { WARNING, ERROR, FATAL_ERROR } severity;

    ValidationError(const std::string &msg, int l, int c, Severity sev)
        : message(msg), line(l), column(c), severity(sev) {}
};

/**
 * @brief XSD Schema validator for SCXML documents
 *
 * Provides W3C SCXML schema validation using libxml2's xmlSchema functions.
 * Validates SCXML documents against the official W3C SCXML 1.0 schema.
 */
class XSDValidator {
public:
    /**
     * @brief Constructor
     * @param schemaPath Path to the XSD schema file (defaults to built-in SCXML schema)
     */
    explicit XSDValidator(const std::string &schemaPath = "");

    /**
     * @brief Destructor - cleanup libxml2 resources
     */
    ~XSDValidator();

    // Non-copyable
    XSDValidator(const XSDValidator &) = delete;
    XSDValidator &operator=(const XSDValidator &) = delete;

    /**
     * @brief Validate an SCXML document against XSD schema
     * @param document The libxml++ document to validate
     * @return true if document is valid, false otherwise
     */
    bool validateDocument(const xmlpp::Document &document);

    /**
     * @brief Validate SCXML content string against XSD schema
     * @param scxmlContent XML content as string
     * @return true if content is valid, false otherwise
     */
    bool validateContent(const std::string &scxmlContent);

    /**
     * @brief Get validation errors from last validation
     * @return Vector of validation errors
     */
    const std::vector<ValidationError> &getErrors() const;

    /**
     * @brief Check if validator is properly initialized
     * @return true if validator is ready to use
     */
    bool isInitialized() const;

    /**
     * @brief Clear error list
     */
    void clearErrors();

private:
    /**
     * @brief Initialize the XSD validator with schema file
     * @param schemaPath Path to XSD schema file
     * @return true if initialization successful
     */
    bool initialize(const std::string &schemaPath);

    /**
     * @brief Cleanup libxml2 schema resources
     */
    void cleanup();

    /**
     * @brief Static error handler for libxml2 schema validation
     */
    static void errorHandler(void *ctx, const char *msg, ...);

    /**
     * @brief Static warning handler for libxml2 schema validation
     */
    static void warningHandler(void *ctx, const char *msg, ...);

    // libxml2 schema validation objects
    xmlSchemaParserCtxtPtr schemaParserCtx_;
    xmlSchemaPtr schema_;
    xmlSchemaValidCtxtPtr validationCtx_;

    // Error collection
    std::vector<ValidationError> errors_;
    bool initialized_;
    std::string schemaPath_;

    // Static instance for error handling callbacks
    static thread_local XSDValidator *currentValidator_;
};

}  // namespace Parsing
}  // namespace SCXML