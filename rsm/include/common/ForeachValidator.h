#pragma once
#include <stdexcept>
#include <string>

namespace RSM::Validation {

/**
 * @brief Validates foreach loop attributes according to W3C SCXML 4.6 specification
 *
 * W3C SCXML 4.6 Requirements:
 * - 'array' attribute is required and must not be empty
 * - 'item' attribute is required and must not be empty
 * - 'index' attribute is optional
 *
 * @param array The array expression to iterate over
 * @param item The variable name for each iteration item
 * @throws std::runtime_error if validation fails
 */
inline void validateForeachAttributes(const std::string &array, const std::string &item) {
    // W3C SCXML 4.6: array attribute is required
    if (array.empty()) {
        throw std::runtime_error("Foreach array attribute is missing or empty");
    }

    // W3C SCXML 4.6: item attribute is required
    if (item.empty()) {
        throw std::runtime_error("Foreach item attribute is missing or empty");
    }
}

}  // namespace RSM::Validation
