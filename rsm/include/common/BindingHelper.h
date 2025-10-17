#pragma once

#include <string>

namespace RSM {

/**
 * @brief Helper functions for W3C SCXML 5.3 binding mode processing
 *
 * Single Source of Truth for data binding semantics shared between:
 * - Interpreter engine (StateMachine)
 * - JIT engine (StaticCodeGenerator)
 *
 * W3C SCXML References:
 * - 5.3: Data Model and Data Manipulation
 * - B.2.2: Late Binding specification
 */
class BindingHelper {
public:
    /**
     * @brief Check if binding mode is early binding
     *
     * W3C SCXML 5.3: Early binding is the default when binding attribute
     * is absent or explicitly set to "early".
     *
     * @param bindingMode Binding mode string from <scxml binding="...">
     * @return true if early binding (default), false otherwise
     */
    static bool isEarlyBinding(const std::string &bindingMode) {
        return bindingMode.empty() || bindingMode == "early";
    }

    /**
     * @brief Check if binding mode is late binding
     *
     * W3C SCXML 5.3: Late binding defers value assignment to state entry.
     *
     * @param bindingMode Binding mode string from <scxml binding="...">
     * @return true if late binding, false otherwise
     */
    static bool isLateBinding(const std::string &bindingMode) {
        return bindingMode == "late";
    }

    /**
     * @brief Determine if variable should be initialized at document load
     *
     * Single Source of Truth for initialization timing logic.
     *
     * W3C SCXML 5.3 / B.2.2 Rules:
     * - Early binding: ALL variables initialized with values at document load
     * - Late binding: ALL variables created with undefined at document load
     *
     * @param bindingMode Binding mode ("early", "late", or empty for default)
     * @return true if variable should be assigned value at document load
     */
    static bool shouldAssignValueAtDocumentLoad(const std::string &bindingMode) {
        // Early binding: always initialize (may have expr, src, or content)
        // Note: initializeDataItem handles all cases (expr/src/content/undefined)
        if (isEarlyBinding(bindingMode)) {
            return true;  // Always call initializeDataItem for early binding
        }

        // Late binding: never assign values at document load
        // W3C SCXML B.2.2: "MUST create data model elements at initialization time"
        // but "MUST assign the specified initial values to data elements only when
        // the state containing them is first entered"
        return false;
    }

    /**
     * @brief Determine if variable should be initialized on state entry
     *
     * Single Source of Truth for state entry initialization logic.
     *
     * W3C SCXML 5.3 / B.2.2 Rules:
     * - Early binding: NO initialization on state entry (already done at load)
     * - Late binding: Initialize ALL variables on first state entry
     *
     * @param bindingMode Binding mode ("early", "late", or empty for default)
     * @param isFirstEntry true if this is the first time entering the state
     * @param hasExpr true if <data> element has expr attribute
     * @return true if variable should be assigned value on state entry
     */
    static bool shouldAssignValueOnStateEntry(const std::string &bindingMode, bool isFirstEntry, bool hasExpr) {
        // Early binding: never assign on state entry (already done at load)
        if (isEarlyBinding(bindingMode)) {
            return false;
        }

        // Late binding: assign values on first entry only
        // W3C SCXML B.2.2: "only when the state containing them is first entered"
        return isLateBinding(bindingMode) && isFirstEntry && hasExpr;
    }

    /**
     * @brief Get default binding mode
     *
     * W3C SCXML 5.3: Default binding mode is "early" when not specified.
     *
     * @return Default binding mode string ("early")
     */
    static std::string getDefaultBinding() {
        return "early";
    }

    /**
     * @brief Normalize binding mode string
     *
     * Ensures binding mode is valid and normalized.
     *
     * @param bindingMode Input binding mode (may be empty)
     * @return Normalized binding mode ("early" or "late")
     */
    static std::string normalizeBinding(const std::string &bindingMode) {
        if (bindingMode == "late") {
            return "late";
        }
        // Default to early for empty or invalid values
        return "early";
    }
};

}  // namespace RSM
