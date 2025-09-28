#pragma once

#include "StateMachine.h"
#include "scripting/IScriptEngine.h"
#include <memory>
#include <string>
#include <variant>

namespace RSM {

/**
 * @brief Factory for creating StateMachine instances with proper dependency injection
 *
 * This factory follows the SOLID principles:
 * - SRP: Only responsible for creating StateMachine instances
 * - OCP: Open for extension with new creation methods
 * - DIP: Depends on abstractions (IScriptEngine) not concretions
 */
class StateMachineFactory {
public:
    /**
     * @brief Result type for factory operations
     */
    struct CreationResult {
        std::unique_ptr<StateMachine> value;
        std::string error;
        bool success;

        CreationResult(std::unique_ptr<StateMachine> sm) : value(std::move(sm)), success(true) {}

        CreationResult(const std::string &err) : error(err), success(false) {}

        bool has_value() const {
            return success;
        }

        explicit operator bool() const {
            return success;
        }
    };

    /**
     * @brief Create StateMachine for production use
     * @return StateMachine instance or error message
     */
    static CreationResult createProduction();

    /**
     * @brief Create StateMachine with SCXML content
     * @param scxmlContent SCXML document content
     * @return Fully configured StateMachine or error message
     */
    static CreationResult createWithSCXML(const std::string &scxmlContent);

    /**
     * @brief Builder pattern for StateMachine configuration
     */
    class Builder {
    private:
        std::string scxmlContent_;
        bool autoInitialize_ = true;

    public:
        Builder &withSCXML(const std::string &content) {
            scxmlContent_ = content;
            return *this;
        }

        Builder &withAutoInitialize(bool autoInit) {
            autoInitialize_ = autoInit;
            return *this;
        }

        /**
         * @brief Build the StateMachine with specified configuration
         * @return StateMachine instance or error message
         */
        CreationResult build();
    };

    /**
     * @brief Get a builder instance
     * @return Builder for fluent configuration
     */
    static Builder builder() {
        return Builder{};
    }

private:
    /**
     * @brief Internal StateMachine creation
     * @param scxmlContent Optional SCXML content
     * @param autoInitialize Whether to initialize automatically
     * @return StateMachine instance or error message
     */
    static CreationResult createInternal(const std::string &scxmlContent = "", bool autoInitialize = true);
};

}  // namespace RSM