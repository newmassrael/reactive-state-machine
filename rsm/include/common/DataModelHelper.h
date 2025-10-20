#pragma once

#include "model/IDataModelItem.h"
#include <string>

namespace RSM {

/**
 * @brief Helper functions for W3C SCXML datamodel extraction
 *
 * Single Source of Truth for datamodel variable extraction logic shared between:
 * - StaticCodeGenerator (AOT engine code generation)
 * - Future Interpreter engine datamodel handling
 *
 * W3C SCXML References:
 * - 5.3: Data Model initialization
 * - 5.10: Global scope for datamodel variables
 */
class DataModelHelper {
public:
    /**
     * @brief Structure representing a datamodel variable
     *
     * Matches the DataModelVariable struct used in StaticCodeGenerator
     * for consistency across the codebase.
     */
    struct Variable {
        std::string name;
        std::string initialValue;
    };

    /**
     * @brief Trim whitespace from string
     *
     * Helper function to remove leading and trailing whitespace from datamodel content.
     * Important for inline array/object literals from content text.
     *
     * @param value String to trim
     * @return Trimmed string
     */
    static inline std::string trimWhitespace(const std::string &value) {
        auto start = value.find_first_not_of(" \t\n\r");
        auto end = value.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            return value.substr(start, end - start + 1);
        }
        return value;
    }

    /**
     * @brief Extract datamodel variable from IDataModelItem
     *
     * Single Source of Truth for datamodel extraction logic.
     * Handles both expr attribute and content (text between tags) with proper whitespace trimming.
     *
     * W3C SCXML 5.3: Data elements can specify initial values via:
     * - expr attribute: JavaScript expression
     * - content: Inline value (array literals, object literals, etc.)
     *
     * @param dataItem Pointer to IDataModelItem from model
     * @return Variable struct with name and initial value
     */
    static inline Variable extractVariable(const IDataModelItem *dataItem) {
        Variable var;
        var.name = dataItem->getId();

        // W3C SCXML 5.3: Try expr attribute first, fallback to content (text between tags)
        var.initialValue = dataItem->getExpr();
        if (var.initialValue.empty()) {
            var.initialValue = dataItem->getContent();
            // Trim whitespace from content (important for inline array/object literals)
            var.initialValue = trimWhitespace(var.initialValue);
        }

        return var;
    }
};

}  // namespace RSM
