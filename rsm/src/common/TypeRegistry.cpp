#include "common/TypeRegistry.h"
#include "common/Constants.h"
#include "common/Logger.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace RSM {

TypeRegistry &TypeRegistry::getInstance() {
    static TypeRegistry instance;
    return instance;
}

TypeRegistry::TypeRegistry() {
    initializeDefaultTypes();
}

bool TypeRegistry::registerType(Category category, const std::string &uri, const std::string &canonicalName) {
    if (uri.empty() || canonicalName.empty()) {
        LOG_ERROR("TypeRegistry: Cannot register type with empty URI or canonical name");
        return false;
    }

    std::string normalizedUri = normalizeUri(uri);

    // Use unique lock for write operations
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Check if already registered with different canonical name
    auto &categoryMap = typeMap_[category];
    auto existing = categoryMap.find(normalizedUri);
    if (existing != categoryMap.end() && existing->second != canonicalName) {
        LOG_WARN("TypeRegistry: URI '{}' already registered with canonical name '{}', not '{}'", uri, existing->second,
                 canonicalName);
        return false;
    }

    // Register in forward map
    categoryMap[normalizedUri] = canonicalName;

    // Register in reverse map
    reverseMap_[category][canonicalName].insert(normalizedUri);

    LOG_DEBUG("TypeRegistry: Registered type URI '{}' -> '{}' in category '{}'", uri, canonicalName,
              categoryToString(category));

    return true;
}

bool TypeRegistry::isRegisteredType(Category category, const std::string &uri) {
    if (uri.empty()) {
        return false;
    }

    std::string normalizedUri = normalizeUri(uri);

    // Use shared lock for read operations
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto categoryIt = typeMap_.find(category);
    if (categoryIt == typeMap_.end()) {
        return false;
    }

    return categoryIt->second.find(normalizedUri) != categoryIt->second.end();
}

std::string TypeRegistry::getCanonicalName(Category category, const std::string &uri) {
    if (uri.empty()) {
        return "";
    }

    std::string normalizedUri = normalizeUri(uri);

    // Use shared lock for read operations
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto categoryIt = typeMap_.find(category);
    if (categoryIt == typeMap_.end()) {
        return "";
    }

    auto uriIt = categoryIt->second.find(normalizedUri);
    if (uriIt == categoryIt->second.end()) {
        return "";
    }

    return uriIt->second;
}

std::vector<std::string> TypeRegistry::getUrisForCanonical(Category category, const std::string &canonicalName) {
    std::vector<std::string> uris;

    // Use shared lock for read operations
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto categoryIt = reverseMap_.find(category);
    if (categoryIt == reverseMap_.end()) {
        return uris;
    }

    auto canonicalIt = categoryIt->second.find(canonicalName);
    if (canonicalIt == categoryIt->second.end()) {
        return uris;
    }

    for (const auto &uri : canonicalIt->second) {
        uris.push_back(uri);
    }

    return uris;
}

bool TypeRegistry::isHttpType(const std::string &uri) {
    if (uri.empty()) {
        return false;
    }

    std::string normalizedUri = normalizeUri(uri);

    // Check for explicit HTTP types
    if (normalizedUri == "http" || normalizedUri == "https" || normalizedUri == "basichttp") {
        return true;
    }

    // Check for HTTP URL schemes
    if (normalizedUri.starts_with("http://") || normalizedUri.starts_with("https://")) {
        return true;
    }

    // Check if registered as HTTP type
    return isRegisteredType(Category::EVENT_PROCESSOR, uri) &&
           getCanonicalName(Category::EVENT_PROCESSOR, uri) == "http";
}

bool TypeRegistry::isBasicHttpEventProcessor(const std::string &uri) {
    if (uri.empty()) {
        return false;
    }

    std::string normalizedUri = normalizeUri(uri);

    // Direct W3C BasicHTTPEventProcessor URI
    const std::string basicHttpUri = normalizeUri(Constants::BASIC_HTTP_EVENT_PROCESSOR_URI);
    if (normalizedUri == basicHttpUri) {
        return true;
    }

    // Alternative representations
    if (normalizedUri == "basichttp" || normalizedUri == "basic-http") {
        return true;
    }

    // Check if registered with basic-http canonical name
    return isRegisteredType(Category::EVENT_PROCESSOR, uri) &&
           getCanonicalName(Category::EVENT_PROCESSOR, uri) == "basic-http";
}

bool TypeRegistry::isScxmlEventProcessor(const std::string &uri) {
    if (uri.empty()) {
        return false;
    }

    std::string normalizedUri = normalizeUri(uri);

    // Direct W3C SCXMLEventProcessor URI
    const std::string scxmlUri = normalizeUri(Constants::SCXML_EVENT_PROCESSOR_URI);
    if (normalizedUri == scxmlUri) {
        return true;
    }

    // Alternative representations
    if (normalizedUri == "scxml" || normalizedUri == "internal") {
        return true;
    }

    // Check if registered with scxml canonical name
    return isRegisteredType(Category::EVENT_PROCESSOR, uri) &&
           getCanonicalName(Category::EVENT_PROCESSOR, uri) == "scxml";
}

std::string TypeRegistry::normalizeUri(const std::string &uri) {
    if (uri.empty()) {
        return "";
    }

    std::string normalized = uri;

    // Convert to lowercase
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Remove trailing slash
    if (normalized.ends_with("/")) {
        normalized = normalized.substr(0, normalized.length() - 1);
    }

    // Remove whitespace
    normalized.erase(std::remove_if(normalized.begin(), normalized.end(), ::isspace), normalized.end());

    return normalized;
}

std::unordered_map<std::string, std::string> TypeRegistry::getRegisteredTypes(Category category) {
    // Use shared lock for read operations
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto categoryIt = typeMap_.find(category);
    if (categoryIt == typeMap_.end()) {
        return {};
    }

    return categoryIt->second;
}

void TypeRegistry::clear() {
    // Use unique lock for write operations
    std::unique_lock<std::shared_mutex> lock(mutex_);

    typeMap_.clear();
    reverseMap_.clear();
    LOG_DEBUG("TypeRegistry: All type registrations cleared");
}

std::string TypeRegistry::getDebugInfo() {
    // Use shared lock for read operations
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::ostringstream info;

    info << "TypeRegistry{";

    for (const auto &categoryPair : typeMap_) {
        Category category = categoryPair.first;
        const auto &uriMap = categoryPair.second;

        info << categoryToString(category) << "=" << uriMap.size() << "_types,";
    }

    info << "total_categories=" << typeMap_.size() << "}";

    return info.str();
}

void TypeRegistry::initializeDefaultTypes() {
    LOG_DEBUG("TypeRegistry: Initializing default SCXML types");

    // ========================================================================
    // Event Processors
    // ========================================================================

    // SCXML Event Processor (internal)
    registerType(Category::EVENT_PROCESSOR, "scxml", "scxml");
    registerType(Category::EVENT_PROCESSOR, "internal", "scxml");
    registerType(Category::EVENT_PROCESSOR, Constants::SCXML_EVENT_PROCESSOR_URI, "scxml");
    registerType(Category::EVENT_PROCESSOR, std::string(Constants::SCXML_EVENT_PROCESSOR_URI) + "/", "scxml");

    // Basic HTTP Event Processor
    registerType(Category::EVENT_PROCESSOR, Constants::BASIC_HTTP_EVENT_PROCESSOR_URI, "basic-http");
    registerType(Category::EVENT_PROCESSOR, std::string(Constants::BASIC_HTTP_EVENT_PROCESSOR_URI) + "/", "basic-http");
    registerType(Category::EVENT_PROCESSOR, "basichttp", "basic-http");
    registerType(Category::EVENT_PROCESSOR, "basic-http", "basic-http");
    registerType(Category::EVENT_PROCESSOR, "http", "basic-http");   // Default HTTP mapping
    registerType(Category::EVENT_PROCESSOR, "https", "basic-http");  // HTTPS uses same processor

    // ========================================================================
    // Invoke Processors
    // ========================================================================

    // SCXML Invoke
    registerType(Category::INVOKE_PROCESSOR, "scxml", "scxml");
    registerType(Category::INVOKE_PROCESSOR, "http://www.w3.org/TR/scxml", "scxml");
    registerType(Category::INVOKE_PROCESSOR, "http://www.w3.org/TR/scxml/", "scxml");
    registerType(Category::INVOKE_PROCESSOR, "application/scxml+xml", "scxml");
    registerType(Category::INVOKE_PROCESSOR, "text/scxml", "scxml");

    // HTTP Invoke (if supported)
    registerType(Category::INVOKE_PROCESSOR, "http", "http");
    registerType(Category::INVOKE_PROCESSOR, "https", "http");

    // ========================================================================
    // Data Models
    // ========================================================================

    // ECMAScript (JavaScript)
    registerType(Category::DATA_MODEL, "ecmascript", "ecmascript");
    registerType(Category::DATA_MODEL, "javascript", "ecmascript");
    registerType(Category::DATA_MODEL, "js", "ecmascript");
    registerType(Category::DATA_MODEL, "application/ecmascript", "ecmascript");
    registerType(Category::DATA_MODEL, "application/javascript", "ecmascript");
    registerType(Category::DATA_MODEL, "text/javascript", "ecmascript");

    // Null data model
    registerType(Category::DATA_MODEL, "null", "null");
    registerType(Category::DATA_MODEL, "", "null");  // Empty means null data model

    // ========================================================================
    // Content Types
    // ========================================================================

    // JSON
    registerType(Category::CONTENT_TYPE, "application/json", "json");
    registerType(Category::CONTENT_TYPE, "text/json", "json");

    // XML
    registerType(Category::CONTENT_TYPE, "application/xml", "xml");
    registerType(Category::CONTENT_TYPE, "text/xml", "xml");
    registerType(Category::CONTENT_TYPE, "application/scxml+xml", "scxml-xml");

    // Plain text
    registerType(Category::CONTENT_TYPE, "text/plain", "text");

    // Form data
    registerType(Category::CONTENT_TYPE, "application/x-www-form-urlencoded", "form");
    registerType(Category::CONTENT_TYPE, "multipart/form-data", "multipart-form");

    LOG_INFO("TypeRegistry: Initialized {} event processors, {} invoke processors, {} data models, {} content types",
             getRegisteredTypes(Category::EVENT_PROCESSOR).size(),
             getRegisteredTypes(Category::INVOKE_PROCESSOR).size(), getRegisteredTypes(Category::DATA_MODEL).size(),
             getRegisteredTypes(Category::CONTENT_TYPE).size());
}

std::string TypeRegistry::categoryToString(Category category) {
    switch (category) {
    case Category::EVENT_PROCESSOR:
        return "event_processor";
    case Category::INVOKE_PROCESSOR:
        return "invoke_processor";
    case Category::DATA_MODEL:
        return "data_model";
    case Category::CONTENT_TYPE:
        return "content_type";
    default:
        return "unknown";
    }
}

}  // namespace RSM