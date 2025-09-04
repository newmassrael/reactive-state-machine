#pragma once

#include "common/Result.h"
#include "parsing/NamespaceAwareParser.h"
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SCXML {
// Forward declarations
namespace Runtime {
class RuntimeContext;
}
class StateMachine;

/**
 * Status of a loaded module
 */
enum class ModuleStatus {
    UNLOADED,  // Module not loaded
    LOADING,   // Module currently being loaded
    LOADED,    // Module loaded successfully
    LINKED,    // Module linked with dependencies
    ACTIVE,    // Module active and running
    ERROR,     // Module in error state
    UNLOADING  // Module being unloaded
};

/**
 * Dependency information between modules
 */
struct ModuleDependency {
    std::string sourceModuleId;  // Module that depends on another
    std::string targetModuleId;  // Module that is depended upon
    std::string dependencyType;  // Type of dependency (invoke, data, state)
    bool isRequired;             // True if dependency is required

    ModuleDependency() : isRequired(true) {}

    ModuleDependency(const std::string &source, const std::string &target, const std::string &type = "invoke",
                     bool required = true)
        : sourceModuleId(source), targetModuleId(target), dependencyType(type), isRequired(required) {}
};

/**
 * Loaded module information with state and dependencies
 */
struct LoadedModule {
    ModuleReference reference;                       // Original module reference
    ModuleStatus status;                             // Current status
    std::shared_ptr<StateMachine> instance;          // State machine instance
    std::shared_ptr<RuntimeContext> context;         // Runtime context
    std::string content;                             // Module content (SCXML)
    std::vector<std::string> dependencies;           // Module dependency IDs
    std::vector<std::string> dependents;             // Modules depending on this one
    std::chrono::steady_clock::time_point loadTime;  // When module was loaded
    std::string errorMessage;                        // Error message if status is ERROR

    LoadedModule() : status(ModuleStatus::UNLOADED), loadTime(std::chrono::steady_clock::now()) {}

    LoadedModule(const ModuleReference &ref)
        : reference(ref), status(ModuleStatus::UNLOADED), loadTime(std::chrono::steady_clock::now()) {}
};

/**
 * Module loading options and configuration
 */
struct ModuleLoadOptions {
    bool loadDependenciesAutomatically;  // Auto-load dependencies
    bool enableCaching;                  // Cache loaded modules
    bool allowCircularDependencies;      // Allow circular dependencies
    int maxLoadTimeoutSeconds;           // Maximum load timeout
    bool validateOnLoad;                 // Validate module on load
    std::string baseDirectory;           // Base directory for relative paths

    ModuleLoadOptions()
        : loadDependenciesAutomatically(true), enableCaching(true), allowCircularDependencies(false),
          maxLoadTimeoutSeconds(30), validateOnLoad(true), baseDirectory(".") {}
};

/**
 * Interface for module loaders that handle specific module types
 */
class IModuleTypeLoader {
public:
    virtual ~IModuleTypeLoader() = default;

    /**
     * Check if this loader can handle the given module type
     */
    virtual bool canHandle(const std::string &moduleType) const = 0;

    /**
     * Load module from source
     */
    virtual SCXML::Common::Result<std::shared_ptr<StateMachine>> loadModule(const ModuleReference &reference,
                                                                            const ModuleLoadOptions &options) = 0;

    /**
     * Validate module before loading
     */
    virtual SCXML::Common::Result<void> validateModule(const ModuleReference &reference,
                                                       const ModuleLoadOptions &options) const = 0;

    /**
     * Get supported module types
     */
    virtual std::vector<std::string> getSupportedTypes() const = 0;
};

/**
 * SCXML module loader for loading external SCXML state machines
 */
class SCXMLModuleLoader : public IModuleTypeLoader {
public:
    SCXMLModuleLoader();
    ~SCXMLModuleLoader() override = default;

    bool canHandle(const std::string &moduleType) const override;

    SCXML::Common::Result<std::shared_ptr<StateMachine>> loadModule(const ModuleReference &reference,
                                                                    const ModuleLoadOptions &options) override;

    SCXML::Common::Result<void> validateModule(const ModuleReference &reference,
                                               const ModuleLoadOptions &options) const override;

    std::vector<std::string> getSupportedTypes() const override;

private:
    std::unique_ptr<NamespaceAwareParser> parser_;

    /**
     * Load SCXML content from file or URL
     */
    SCXML::Common::Result<std::string> loadSCXMLContent(const std::string &source,
                                                        const std::string &baseDirectory) const;

    /**
     * Parse and create state machine from SCXML content
     */
    SCXML::Common::Result<std::shared_ptr<StateMachine>> createStateMachine(const std::string &scxmlContent,
                                                                            const ModuleReference &reference) const;
};

/**
 * Advanced module loader with dependency resolution and caching
 */
class ModuleLoader {
public:
    explicit ModuleLoader(const ModuleLoadOptions &options = ModuleLoadOptions());
    ~ModuleLoader();

    /**
     * Load a single module
     */
    SCXML::Common::Result<std::string> loadModule(const ModuleReference &reference);

    /**
     * Load multiple modules with dependency resolution
     */
    SCXML::Common::Result<std::vector<std::string>> loadModules(const std::vector<ModuleReference> &references);

    /**
     * Load modules from namespace-aware parser results
     */
    SCXML::Common::Result<std::vector<std::string>> loadModulesFromParser(const NamespaceAwareParser &parser);

    /**
     * Get loaded module by ID
     */
    SCXML::Common::Result<LoadedModule> getModule(const std::string &moduleId) const;

    /**
     * Get all loaded modules
     */
    std::vector<LoadedModule> getAllModules() const;

    /**
     * Unload a module
     */
    SCXML::Common::Result<void> unloadModule(const std::string &moduleId);

    /**
     * Unload all modules
     */
    SCXML::Common::Result<void> unloadAllModules();

    /**
     * Link modules (resolve dependencies)
     */
    SCXML::Common::Result<void> linkModules();

    /**
     * Start all loaded and linked modules
     */
    SCXML::Common::Result<void> startAllModules();

    /**
     * Stop all active modules
     */
    SCXML::Common::Result<void> stopAllModules();

    /**
     * Register a custom module type loader
     */
    void registerModuleTypeLoader(std::shared_ptr<SCXML::Model::IModuleTypeLoader> loader);

    /**
     * Set module loading options
     */
    void setLoadOptions(const ModuleLoadOptions &options);

    /**
     * Get module loading statistics
     */
    struct LoadingStatistics {
        size_t totalModules;
        size_t loadedModules;
        size_t linkedModules;
        size_t activeModules;
        size_t errorModules;
        std::chrono::milliseconds totalLoadTime;
    };

    LoadingStatistics getLoadingStatistics() const;

    /**
     * Validate module dependencies for circular references
     */
    SCXML::Common::Result<void> validateDependencies() const;

    /**
     * Get dependency graph as adjacency list
     */
    std::map<std::string, std::vector<std::string>> getDependencyGraph() const;

private:
    mutable std::mutex modulesMutex_;
    std::map<std::string, LoadedModule> modules_;
    std::map<std::string, std::shared_ptr<SCXML::Model::IModuleTypeLoader>> typeLoaders_;
    ModuleLoadOptions loadOptions_;
    std::map<std::string, std::vector<std::string>> dependencyGraph_;

    /**
     * Load module implementation with locking
     */
    SCXML::Common::Result<void> loadModuleImpl(const ModuleReference &reference);

    /**
     * Resolve module dependencies
     */
    SCXML::Common::Result<void> resolveDependencies(const std::string &moduleId);

    /**
     * Build dependency graph from loaded modules
     */
    SCXML::Common::Result<void> buildDependencyGraph();

    /**
     * Topological sort for dependency ordering
     */
    SCXML::Common::Result<std::vector<std::string>> topologicalSort() const;

    /**
     * Check for circular dependencies
     */
    bool hasCircularDependencies() const;

    /**
     * Get module type loader for module type
     */
    std::shared_ptr<SCXML::Model::IModuleTypeLoader> getModuleTypeLoader(const std::string &moduleType) const;

    /**
     * Resolve relative path with base directory
     */
    std::string resolveModulePath(const std::string &path) const;

    /**
     * Update module status safely
     */
    void updateModuleStatus(const std::string &moduleId, ModuleStatus status, const std::string &errorMessage = "");

    /**
     * Initialize default module type loaders
     */
    void initializeDefaultLoaders();

    /**
     * Internal module loading implementation
     */
    SCXML::Common::Result<void> loadModuleInternal(const ModuleReference &reference);

    /**
     * DFS cycle detection helper
     */
    bool dfsHasCycle(const std::string &nodeId, std::unordered_map<std::string, int> &colors) const;
};

/**
 * Module loading utilities
 */
class ModuleLoadingUtils {
public:
    /**
     * Extract module references from SCXML content
     */
    static SCXML::Common::Result<std::vector<ModuleReference>> extractModuleReferences(const std::string &scxmlContent);

    /**
     * Validate module reference format
     */
    static SCXML::Common::Result<void> validateModuleReference(const ModuleReference &reference);

    /**
     * Convert relative paths to absolute paths
     */
    static std::string resolveRelativePath(const std::string &basePath, const std::string &relativePath);

    /**
     * Check if path is URL (http/https)
     */
    static bool isURL(const std::string &path);

    /**
     * Load content from file or URL
     */
    static SCXML::Common::Result<std::string> loadContent(const std::string &source);

    /**
     * Create unique module ID from reference
     */
    static std::string createModuleId(const ModuleReference &reference);

    /**
     * Extract state machine ID from XML document
     */
    static std::string extractStateMachineId(const std::shared_ptr<xmlpp::Document> &doc);

private:
    /**
     * Load content from HTTP URL using curl
     */
    static SCXML::Common::Result<std::string> loadFromHTTP(const std::string &url);
};

}  // namespace Runtime
}  // namespace SCXML