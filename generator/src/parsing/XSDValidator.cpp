#include "parsing/XSDValidator.h"
#include "common/Logger.h"
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <libxml/parser.h>
#include <libxml/tree.h>

namespace SCXML {
namespace Parsing {

// Thread-local storage for error handling callbacks
thread_local XSDValidator *XSDValidator::currentValidator_ = nullptr;

XSDValidator::XSDValidator(const std::string &schemaPath)
    : schemaParserCtx_(nullptr), schema_(nullptr), validationCtx_(nullptr), initialized_(false) {
#ifdef SCXML_SCHEMA_PATH
    std::string actualSchemaPath = schemaPath.empty() ? SCXML_SCHEMA_PATH : schemaPath;
#else
    if (schemaPath.empty()) {
        Common::Logger::error("XSDValidator: SCXML_SCHEMA_PATH not defined and no schema path provided");
        return;
    }
    std::string actualSchemaPath = schemaPath;
#endif
    initialized_ = initialize(actualSchemaPath);
}

XSDValidator::~XSDValidator() {
    cleanup();
}

bool XSDValidator::initialize(const std::string &schemaPath) {
    schemaPath_ = schemaPath;

    // Check if schema file exists
    if (!std::filesystem::exists(schemaPath)) {
        Common::Logger::error("XSD schema file not found: " + schemaPath);
        return false;
    }

    // Initialize libxml2 if not already done
    xmlInitParser();

    try {
        // Create schema parser context
        schemaParserCtx_ = xmlSchemaNewParserCtxt(schemaPath.c_str());
        if (!schemaParserCtx_) {
            Common::Logger::error("Failed to create XSD schema parser context");
            return false;
        }

        // Set error handlers for schema parsing
        xmlSchemaSetParserErrors(schemaParserCtx_, (xmlSchemaValidityErrorFunc)errorHandler,
                                 (xmlSchemaValidityWarningFunc)warningHandler, this);

        // Parse the schema
        Common::Logger::debug("XSDValidator: Parsing schema file: " + schemaPath);
        schema_ = xmlSchemaParse(schemaParserCtx_);
        if (!schema_) {
            Common::Logger::error("Failed to parse XSD schema: " + schemaPath);

            // Check if file exists and is readable
            if (!std::filesystem::exists(schemaPath)) {
                Common::Logger::error("XSD schema file does not exist: " + schemaPath);
            } else if (!std::filesystem::is_regular_file(schemaPath)) {
                Common::Logger::error("XSD schema path is not a regular file: " + schemaPath);
            } else {
                Common::Logger::error("XSD schema file exists but failed to parse - may contain errors");
            }
            return false;
        }

        // Create validation context
        validationCtx_ = xmlSchemaNewValidCtxt(schema_);
        if (!validationCtx_) {
            Common::Logger::error("Failed to create XSD validation context");
            return false;
        }

        // Set error handlers for validation
        xmlSchemaSetValidErrors(validationCtx_, (xmlSchemaValidityErrorFunc)errorHandler,
                                (xmlSchemaValidityWarningFunc)warningHandler, this);

        Common::Logger::info("XSD validator initialized with schema: " + schemaPath);
        return true;

    } catch (const std::exception &e) {
        Common::Logger::error("Exception during XSD validator initialization: " + std::string(e.what()));
        cleanup();
        return false;
    }
}

void XSDValidator::cleanup() {
    if (validationCtx_) {
        xmlSchemaFreeValidCtxt(validationCtx_);
        validationCtx_ = nullptr;
    }

    if (schema_) {
        xmlSchemaFree(schema_);
        schema_ = nullptr;
    }

    if (schemaParserCtx_) {
        xmlSchemaFreeParserCtxt(schemaParserCtx_);
        schemaParserCtx_ = nullptr;
    }

    initialized_ = false;
}

bool XSDValidator::validateDocument(const xmlpp::Document &document) {
    if (!initialized_) {
        Common::Logger::error("XSD validator not initialized");
        return false;
    }

    clearErrors();
    currentValidator_ = this;

    try {
        // Get the underlying libxml2 document
        xmlDocPtr xmlDoc = const_cast<xmlDocPtr>(document.cobj());
        if (!xmlDoc) {
            Common::Logger::error("Failed to get libxml2 document object");
            return false;
        }

        // Validate the document
        int result = xmlSchemaValidateDoc(validationCtx_, xmlDoc);
        currentValidator_ = nullptr;

        if (result == 0) {
            Common::Logger::debug("XSD validation successful");
            return true;
        } else {
            Common::Logger::warning("XSD validation failed with " + std::to_string(errors_.size()) + " errors");
            return false;
        }

    } catch (const std::exception &e) {
        currentValidator_ = nullptr;
        Common::Logger::error("Exception during XSD validation: " + std::string(e.what()));
        return false;
    }
}

bool XSDValidator::validateContent(const std::string &scxmlContent) {
    if (!initialized_) {
        Common::Logger::error("XSD validator not initialized");
        return false;
    }

    try {
        // Parse the content into a libxml2 document
        xmlDocPtr xmlDoc = xmlReadMemory(scxmlContent.c_str(), static_cast<int>(scxmlContent.length()), "scxml.xml",
                                         nullptr, XML_PARSE_NONET | XML_PARSE_NOWARNING);

        if (!xmlDoc) {
            Common::Logger::error("Failed to parse XML content for validation");
            return false;
        }

        clearErrors();
        currentValidator_ = this;

        // Validate the document
        int result = xmlSchemaValidateDoc(validationCtx_, xmlDoc);
        currentValidator_ = nullptr;

        // Clean up the temporary document
        xmlFreeDoc(xmlDoc);

        if (result == 0) {
            Common::Logger::debug("XSD validation successful");
            return true;
        } else {
            Common::Logger::warning("XSD validation failed with " + std::to_string(errors_.size()) + " errors");
            return false;
        }

    } catch (const std::exception &e) {
        currentValidator_ = nullptr;
        Common::Logger::error("Exception during XSD content validation: " + std::string(e.what()));
        return false;
    }
}

const std::vector<ValidationError> &XSDValidator::getErrors() const {
    return errors_;
}

bool XSDValidator::isInitialized() const {
    return initialized_;
}

void XSDValidator::clearErrors() {
    errors_.clear();
}

void XSDValidator::errorHandler(void * /* ctx */, const char *msg, ...) {
    XSDValidator *validator = currentValidator_;
    if (!validator) {
        return;
    }

    va_list args;
    va_start(args, msg);

    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    // Remove trailing newline if present
    std::string errorMsg(buffer);
    if (!errorMsg.empty() && errorMsg.back() == '\n') {
        errorMsg.pop_back();
    }

    validator->errors_.emplace_back(errorMsg, 0, 0, ValidationError::Severity::ERROR);
    Common::Logger::warning("XSD Validation Error: " + errorMsg);
}

void XSDValidator::warningHandler(void * /* ctx */, const char *msg, ...) {
    XSDValidator *validator = currentValidator_;
    if (!validator) {
        return;
    }

    va_list args;
    va_start(args, msg);

    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    // Remove trailing newline if present
    std::string warningMsg(buffer);
    if (!warningMsg.empty() && warningMsg.back() == '\n') {
        warningMsg.pop_back();
    }

    validator->errors_.emplace_back(warningMsg, 0, 0, ValidationError::Severity::WARNING);
    Common::Logger::debug("XSD Validation Warning: " + warningMsg);
}

}  // namespace Parsing
}  // namespace SCXML