#pragma once

#include <functional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RSM {

/**
 * @brief Registry for SCXML type URIs and their normalization
 *
 * Manages mapping between different URI representations for SCXML types,
 * following SOLID principles for extensible type management.
 */
class TypeRegistry {
public:
    /**
     * @brief Type category identifiers
     */
    enum class Category {
        EVENT_PROCESSOR,   // Event I/O processors (send/receive)
        INVOKE_PROCESSOR,  // Invoke processors
        DATA_MODEL,        // Data model types
        CONTENT_TYPE       // Content types
    };

    /**
     * @brief Get singleton instance
     * @return TypeRegistry instance
     */
    static TypeRegistry &getInstance();

    /**
     * @brief Register a type URI with a category and canonical name
     * @param category Type category
     * @param uri Type URI to register
     * @param canonicalName Canonical name for this type
     * @return true if registration succeeded
     */
    bool registerType(Category category, const std::string &uri, const std::string &canonicalName);

    /**
     * @brief Check if a type URI is registered in a category
     * @param category Type category to check
     * @param uri Type URI to check
     * @return true if registered
     */
    bool isRegisteredType(Category category, const std::string &uri);

    /**
     * @brief Get canonical name for a type URI
     * @param category Type category
     * @param uri Type URI
     * @return Canonical name, or empty string if not found
     */
    std::string getCanonicalName(Category category, const std::string &uri);

    /**
     * @brief Get all registered URIs for a canonical name
     * @param category Type category
     * @param canonicalName Canonical name
     * @return Vector of URIs mapped to this canonical name
     */
    std::vector<std::string> getUrisForCanonical(Category category, const std::string &canonicalName);

    /**
     * @brief Check if URI represents an HTTP-based type
     * @param uri Type URI to check
     * @return true if HTTP-based
     */
    bool isHttpType(const std::string &uri);

    /**
     * @brief Check if URI represents BasicHTTPEventProcessor
     * @param uri Type URI to check
     * @return true if BasicHTTPEventProcessor
     */
    bool isBasicHttpEventProcessor(const std::string &uri);

    /**
     * @brief Check if URI represents SCXMLEventProcessor
     * @param uri Type URI to check
     * @return true if SCXMLEventProcessor
     */
    bool isScxmlEventProcessor(const std::string &uri);

    /**
     * @brief Normalize type URI for comparison
     * @param uri Type URI to normalize
     * @return Normalized URI
     */
    static std::string normalizeUri(const std::string &uri);

    /**
     * @brief Get all registered types in a category
     * @param category Type category
     * @return Map of URI -> canonical name
     */
    std::unordered_map<std::string, std::string> getRegisteredTypes(Category category);

    /**
     * @brief Clear all registrations (mainly for testing)
     */
    void clear();

    /**
     * @brief Get debug information about registered types
     * @return Debug information string
     */
    std::string getDebugInfo();

private:
    TypeRegistry();
    ~TypeRegistry() = default;

    // Non-copyable, non-movable
    TypeRegistry(const TypeRegistry &) = delete;
    TypeRegistry &operator=(const TypeRegistry &) = delete;
    TypeRegistry(TypeRegistry &&) = delete;
    TypeRegistry &operator=(TypeRegistry &&) = delete;

    /**
     * @brief Initialize default SCXML types
     */
    void initializeDefaultTypes();

    /**
     * @brief Convert category enum to string
     * @param category Category enum
     * @return Category string
     */
    static std::string categoryToString(Category category);

    // Thread safety for all operations
    std::shared_mutex mutex_;

    // Storage: category -> (normalized_uri -> canonical_name)
    std::unordered_map<Category, std::unordered_map<std::string, std::string>> typeMap_;

    // Reverse mapping: category -> (canonical_name -> set of URIs)
    std::unordered_map<Category, std::unordered_map<std::string, std::unordered_set<std::string>>> reverseMap_;
};

}  // namespace RSM