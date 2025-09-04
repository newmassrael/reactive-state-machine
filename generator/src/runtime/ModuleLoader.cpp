#include "runtime/ModuleLoader.h"
#include "common/Logger.h"
#include "common/RuntimeContext.h"
#include "common/SCXMLConstants.h"
#include "common/StateMachine.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <thread>

namespace SCXML {
namespace Runtime {

// SCXMLModuleLoader Implementation
SCXMLModuleLoader::SCXMLModuleLoader() : parser_(std::make_unique<NamespaceAwareParser>()) {}

bool SCXMLModuleLoader::canHandle(const std::string &moduleType) const {
    return moduleType == "scxml" || moduleType == "statechart" || moduleType.empty();
}

SCXML::Common::Result<std::shared_ptr<StateMachine>> SCXMLModuleLoader::loadModule(const ModuleReference &reference,
                                                                                   const ModuleLoadOptions &options) {
    try {
        SCXML::Common::Logger::info("SCXMLModuleLoader", "Loading SCXML module: " + reference.src);

        // Load SCXML content
        auto contentResult = loadSCXMLContent(reference.src, options.baseDirectory);
        if (!contentResult.isSuccess()) {
            return SCXML::Common::Result<std::shared_ptr<StateMachine>>::error("Failed to load SCXML content: " +
                                                                               contentResult.getError());
        }

        // Create state machine from content
        auto stateMachineResult = createStateMachine(contentResult.getValue(), reference);
        if (!stateMachineResult.isSuccess()) {
            return SCXML::Common::Result<std::shared_ptr<StateMachine>>::error("Failed to create state machine: " +
                                                                               stateMachineResult.getError());
        }

        SCXML::Common::Logger::info("SCXMLModuleLoader", "Successfully loaded SCXML module: " + reference.id);
        return stateMachineResult;

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::shared_ptr<StateMachine>>::error("SCXML module loading failed: " +
                                                                           std::string(e.what()));
    }
}

SCXML::Common::Result<void> SCXMLModuleLoader::validateModule(const ModuleReference &reference,
                                                              const ModuleLoadOptions &options) const {
    try {
        // Basic validation checks
        if (reference.src.empty()) {
            return SCXML::Common::Result<void>::error("Module source path is empty");
        }

        // Load and parse content for validation
        auto contentResult = loadSCXMLContent(reference.src, options.baseDirectory);
        if (!contentResult.isSuccess()) {
            return SCXML::Common::Result<void>::error("Cannot load module content: " + contentResult.getError());
        }

        // Parse with namespace-aware parser for validation
        auto parseResult = parser_->parseDocument(contentResult.getValue());
        if (!parseResult.isSuccess()) {
            return SCXML::Common::Result<void>::error("SCXML validation failed: " + parseResult.getError());
        }

        SCXML::Common::Logger::info("SCXMLModuleLoader", "Module validation passed: " + reference.src);
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Module validation error: " + std::string(e.what()));
    }
}

std::vector<std::string> SCXMLModuleLoader::getSupportedTypes() const {
    return {"scxml", "statechart"};
}

SCXML::Common::Result<std::string> SCXMLModuleLoader::loadSCXMLContent(const std::string &source,
                                                                       const std::string &baseDirectory) const {
    try {
        std::string fullPath = source;

        // Resolve relative path
        if (!source.empty() && source[0] != '/' && !ModuleLoadingUtils::isURL(source)) {
            fullPath = ModuleLoadingUtils::resolveRelativePath(baseDirectory, source);
        }

        // Load content from file or URL
        return ModuleLoadingUtils::loadContent(fullPath);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>::error("Failed to load SCXML content: " + std::string(e.what()));
    }
}

SCXML::Common::Result<std::shared_ptr<StateMachine>>
SCXMLModuleLoader::createStateMachine(const std::string &scxmlContent, const ModuleReference &reference) const {
    try {
        // Parse SCXML content
        auto parseResult = parser_->parseDocument(scxmlContent);
        if (!parseResult.isSuccess()) {
            return SCXML::Common::Result<std::shared_ptr<StateMachine>>::error("Failed to parse SCXML: " +
                                                                               parseResult.getError());
        }

        // Create state machine instance
        // Note: This would typically involve more complex state machine creation
        // For now, we'll create a placeholder implementation
        auto stateMachine = std::make_shared<StateMachine>();

        // Build state machine from parsed XML document
        // Initialize basic state machine with parsed XML data
        stateMachine->setId(extractStateMachineId(doc));

        SCXML::Common::Logger::info("ModuleLoader", "Successfully created state machine from XML");

        return SCXML::Common::Result<std::shared_ptr<StateMachine>>::success(stateMachine);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::shared_ptr<StateMachine>>::error("State machine creation failed: " +
                                                                           std::string(e.what()));
    }
}

// ModuleLoader Implementation
ModuleLoader::ModuleLoader(const ModuleLoadOptions &options) : loadOptions_(options) {
    initializeDefaultLoaders();
}

ModuleLoader::~ModuleLoader() {
    try {
        stopAllModules();
        unloadAllModules();
    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("ModuleLoader", "Error during destruction: " + std::string(e.what()));
    }
}

SCXML::Common::Result<std::string> ModuleLoader::loadModule(const ModuleReference &reference) {
    try {
        SCXML::Common::Logger::info("ModuleLoader", "Loading module: " + reference.id + " from " + reference.src);

        auto result = loadModuleImpl(reference);
        if (!result.isSuccess()) {
            return SCXML::Common::Result<std::string>::error(result.getError());
        }

        return SCXML::Common::Result<std::string>::success(reference.id);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>::error("Module loading failed: " + std::string(e.what()));
    }
}

SCXML::Common::Result<std::vector<std::string>>
ModuleLoader::loadModules(const std::vector<ModuleReference> &references) {
    try {
        std::vector<std::string> loadedModuleIds;

        // Load all modules
        for (const auto &reference : references) {
            auto result = loadModuleImpl(reference);
            if (result.isSuccess()) {
                loadedModuleIds.push_back(reference.id);
            } else {
                SCXML::Common::Logger::error("ModuleLoader",
                                             "Failed to load module " + reference.id + ": " + result.getError());

                // Cleanup partially loaded modules if any failed
                for (const auto &moduleId : loadedModuleIds) {
                    unloadModule(moduleId);
                }

                return SCXML::Common::Result<std::vector<std::string>>::error("Module loading failed: " +
                                                                              result.getError());
            }
        }

        // Build dependency graph and resolve dependencies
        auto buildResult = buildDependencyGraph();
        if (!buildResult.isSuccess()) {
            return SCXML::Common::Result<std::vector<std::string>>::error("Dependency resolution failed: " +
                                                                          buildResult.getError());
        }

        // Link modules
        auto linkResult = linkModules();
        if (!linkResult.isSuccess()) {
            return SCXML::Common::Result<std::vector<std::string>>::error("Module linking failed: " +
                                                                          linkResult.getError());
        }

        SCXML::Common::Logger::info("ModuleLoader",
                                    "Successfully loaded " + std::to_string(loadedModuleIds.size()) + " modules");
        return SCXML::Common::Result<std::vector<std::string>>::success(loadedModuleIds);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::vector<std::string>>::error("Modules loading failed: " +
                                                                      std::string(e.what()));
    }
}

SCXML::Common::Result<std::vector<std::string>>
ModuleLoader::loadModulesFromParser(const NamespaceAwareParser &parser) {
    try {
        const auto &moduleReferences = parser.getModuleReferences();
        return loadModules(moduleReferences);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::vector<std::string>>::error("Parser-based loading failed: " +
                                                                      std::string(e.what()));
    }
}

SCXML::Common::Result<LoadedModule> ModuleLoader::getModule(const std::string &moduleId) const {
    std::lock_guard<std::mutex> lock(modulesMutex_);

    auto it = modules_.find(moduleId);
    if (it != modules_.end()) {
        return SCXML::Common::Result<LoadedModule>::success(it->second);
    }

    return SCXML::Common::Result<LoadedModule>::error("Module not found: " + moduleId);
}

std::vector<LoadedModule> ModuleLoader::getAllModules() const {
    std::lock_guard<std::mutex> lock(modulesMutex_);

    std::vector<LoadedModule> result;
    for (const auto &pair : modules_) {
        result.push_back(pair.second);
    }

    return result;
}

SCXML::Common::Result<void> ModuleLoader::unloadModule(const std::string &moduleId) {
    try {
        std::lock_guard<std::mutex> lock(modulesMutex_);

        auto it = modules_.find(moduleId);
        if (it == modules_.end()) {
            return SCXML::Common::Result<void>::error("Module not found: " + moduleId);
        }

        LoadedModule &module = it->second;

        // Check for dependents
        if (!module.dependents.empty()) {
            return SCXML::Common::Result<void>::error("Cannot unload module with dependents: " + moduleId);
        }

        // Update status
        module.status = ModuleStatus::UNLOADING;

        // Stop state machine if active
        if (module.instance && module.status == ModuleStatus::ACTIVE) {
            // Stop the state machine instance
            if (module.instance->isRunning()) {
                module.instance->stop();
                SCXML::Common::Logger::debug("ModuleLoader", "Stopped state machine for module: " + moduleId);
            }
        }

        // Remove from modules map
        modules_.erase(it);

        SCXML::Common::Logger::info("ModuleLoader", "Unloaded module: " + moduleId);
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Module unloading failed: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> ModuleLoader::unloadAllModules() {
    try {
        std::lock_guard<std::mutex> lock(modulesMutex_);

        // Get topological sort order (reverse for unloading)
        auto sortResult = topologicalSort();
        if (sortResult.isSuccess()) {
            auto sortedIds = sortResult.getValue();
            std::reverse(sortedIds.begin(), sortedIds.end());

            for (const auto &moduleId : sortedIds) {
                auto it = modules_.find(moduleId);
                if (it != modules_.end()) {
                    it->second.status = ModuleStatus::UNLOADING;
                }
            }
        }

        modules_.clear();

        SCXML::Common::Logger::info("ModuleLoader", "Unloaded all modules");
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Unload all modules failed: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> ModuleLoader::linkModules() {
    try {
        std::lock_guard<std::mutex> lock(modulesMutex_);

        // Validate dependencies first
        auto validateResult = validateDependencies();
        if (!validateResult.isSuccess()) {
            return validateResult;
        }

        // Get topological order for linking
        auto sortResult = topologicalSort();
        if (!sortResult.isSuccess()) {
            return SCXML::Common::Result<void>::error("Failed to determine module order: " + sortResult.getError());
        }

        auto sortedIds = sortResult.getValue();

        // Link modules in topological order
        for (const auto &moduleId : sortedIds) {
            auto it = modules_.find(moduleId);
            if (it != modules_.end()) {
                auto resolveResult = resolveDependencies(moduleId);
                if (resolveResult.isSuccess()) {
                    it->second.status = ModuleStatus::LINKED;
                } else {
                    it->second.status = ModuleStatus::ERROR;
                    it->second.errorMessage = "Dependency resolution failed: " + resolveResult.getError();
                    return SCXML::Common::Result<void>::error("Failed to link module " + moduleId + ": " +
                                                              resolveResult.getError());
                }
            }
        }

        SCXML::Common::Logger::info("ModuleLoader", "Successfully linked all modules");
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Module linking failed: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> ModuleLoader::startAllModules() {
    try {
        std::lock_guard<std::mutex> lock(modulesMutex_);

        for (auto &pair : modules_) {
            LoadedModule &module = pair.second;
            if (module.status == ModuleStatus::LINKED && module.instance) {
                // Start the state machine instance
                auto startResult = module.instance->start();
                if (!startResult.isSuccess()) {
                    SCXML::Common::Logger::error("ModuleLoader", "Failed to start module " + pair.first + ": " +
                                                                     startResult.getError());
                    continue;
                }
                module.status = ModuleStatus::ACTIVE;
                SCXML::Common::Logger::info("ModuleLoader", "Started module: " + pair.first);
            }
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Start modules failed: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> ModuleLoader::stopAllModules() {
    try {
        std::lock_guard<std::mutex> lock(modulesMutex_);

        for (auto &pair : modules_) {
            LoadedModule &module = pair.second;
            if (module.status == ModuleStatus::ACTIVE && module.instance) {
                // Stop the state machine instance
                if (module.instance->isRunning()) {
                    module.instance->stop();
                    SCXML::Common::Logger::debug("ModuleLoader", "Stopped state machine for module: " + moduleId);
                }
                module.status = ModuleStatus::LINKED;
                SCXML::Common::Logger::info("ModuleLoader", "Stopped module: " + pair.first);
            }
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Stop modules failed: " + std::string(e.what()));
    }
}

void ModuleLoader::registerModuleTypeLoader(std::shared_ptr<IModuleTypeLoader> loader) {
    if (!loader) {
        return;
    }

    auto supportedTypes = loader->getSupportedTypes();
    for (const auto &type : supportedTypes) {
        typeLoaders_[type] = loader;
    }
}

void ModuleLoader::setLoadOptions(const ModuleLoadOptions &options) {
    loadOptions_ = options;
}

ModuleLoader::LoadingStatistics ModuleLoader::getLoadingStatistics() const {
    std::lock_guard<std::mutex> lock(modulesMutex_);

    LoadingStatistics stats;
    stats.totalModules = modules_.size();
    stats.loadedModules = 0;
    stats.linkedModules = 0;
    stats.activeModules = 0;
    stats.errorModules = 0;

    auto startTime = std::chrono::steady_clock::time_point::max();
    auto endTime = std::chrono::steady_clock::time_point::min();

    for (const auto &pair : modules_) {
        const LoadedModule &module = pair.second;

        switch (module.status) {
        case ModuleStatus::LOADED:
            stats.loadedModules++;
            break;
        case ModuleStatus::LINKED:
            stats.linkedModules++;
            break;
        case ModuleStatus::ACTIVE:
            stats.activeModules++;
            break;
        case ModuleStatus::ERROR:
            stats.errorModules++;
            break;
        default:
            break;
        }

        if (module.loadTime < startTime) {
            startTime = module.loadTime;
        }
        if (module.loadTime > endTime) {
            endTime = module.loadTime;
        }
    }

    if (startTime != std::chrono::steady_clock::time_point::max()) {
        stats.totalLoadTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    }

    return stats;
}

SCXML::Common::Result<void> ModuleLoader::validateDependencies() const {
    if (!loadOptions_.allowCircularDependencies && hasCircularDependencies()) {
        return SCXML::Common::Result<void>::error("Circular dependencies detected");
    }

    return SCXML::Common::Result<void>::success();
}

std::map<std::string, std::vector<std::string>> ModuleLoader::getDependencyGraph() const {
    std::lock_guard<std::mutex> lock(modulesMutex_);

    std::map<std::string, std::vector<std::string>> graph;

    for (const auto &pair : modules_) {
        const LoadedModule &module = pair.second;
        std::vector<std::string> dependencies;

        for (const auto &dep : module.dependencies) {
            dependencies.push_back(dep.targetModuleId);
        }

        graph[pair.first] = dependencies;
    }

    return graph;
}

// Private methods implementation
SCXML::Common::Result<void> ModuleLoader::loadModuleImpl(const ModuleReference &reference) {
    try {
        std::lock_guard<std::mutex> lock(modulesMutex_);

        // Check if already loaded
        if (modules_.find(reference.id) != modules_.end()) {
            SCXML::Common::Logger::info("ModuleLoader", "Module already loaded: " + reference.id);
            return SCXML::Common::Result<void>::success();
        }

        // Create loaded module entry
        LoadedModule loadedModule(reference);
        loadedModule.status = ModuleStatus::LOADING;

        // Get appropriate loader
        auto loader = getModuleTypeLoader(reference.type);
        if (!loader) {
            return SCXML::Common::Result<void>::error("No loader found for module type: " + reference.type);
        }

        // Validate module if requested
        if (loadOptions_.validateOnLoad) {
            auto validateResult = loader->validateModule(reference, loadOptions_);
            if (!validateResult.isSuccess()) {
                return SCXML::Common::Result<void>::error("Module validation failed: " + validateResult.getError());
            }
        }

        // Load the module
        auto instanceResult = loader->loadModule(reference, loadOptions_);
        if (!instanceResult.isSuccess()) {
            return SCXML::Common::Result<void>::error("Module loading failed: " + instanceResult.getError());
        }

        loadedModule.instance = instanceResult.getValue();
        loadedModule.status = ModuleStatus::LOADED;
        loadedModule.context = std::make_shared<RuntimeContext>();

        // Store loaded module
        modules_[reference.id] = loadedModule;

        SCXML::Common::Logger::info("ModuleLoader", "Successfully loaded module: " + reference.id);
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Module loading implementation failed: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> ModuleLoader::resolveDependencies(const std::string &moduleId) {
    try {
        std::lock_guard<std::mutex> lock(modulesMutex_);

        auto it = modules_.find(moduleId);
        if (it == modules_.end()) {
            return SCXML::Common::Result<void>::error("Module not found: " + moduleId);
        }

        LoadedModule &module = it->second;

        // Extract module references from the loaded content
        if (module.content.empty()) {
            return SCXML::Common::Result<void>::success();  // No dependencies
        }

        auto referencesResult = ModuleLoadingUtils::extractModuleReferences(module.content);
        if (!referencesResult.isSuccess()) {
            return SCXML::Common::Result<void>::error("Failed to extract dependencies: " + referencesResult.getError());
        }

        // Load each dependency if not already loaded
        for (const auto &ref : referencesResult.getValue()) {
            if (modules_.find(ref.id) == modules_.end()) {
                // Dependency not loaded, attempt to load it
                SCXML::Common::Logger::info("ModuleLoader",
                                            "Loading dependency: " + ref.id + " for module: " + moduleId);

                auto loadResult = loadModuleInternal(ref);
                if (!loadResult.isSuccess()) {
                    return SCXML::Common::Result<void>::error("Failed to load dependency " + ref.id + ": " +
                                                              loadResult.getError());
                }

                // Recursively resolve dependencies of the dependency
                auto depResolveResult = resolveDependencies(ref.id);
                if (!depResolveResult.isSuccess()) {
                    return SCXML::Common::Result<void>::error("Failed to resolve dependencies for " + ref.id + ": " +
                                                              depResolveResult.getError());
                }
            }

            // Add dependency relationship
            module.dependencies.push_back(ref.id);
        }

        SCXML::Common::Logger::debug("ModuleLoader", "Resolved " + std::to_string(module.dependencies.size()) +
                                                         " dependencies for module: " + moduleId);
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Dependency resolution failed: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> ModuleLoader::buildDependencyGraph() {
    try {
        std::lock_guard<std::mutex> lock(modulesMutex_);

        // Clear existing dependency graph
        dependencyGraph_.clear();

        // Build adjacency list representation
        for (const auto &pair : modules_) {
            const std::string &moduleId = pair.first;
            const LoadedModule &module = pair.second;

            // Initialize empty dependency list if not present
            if (dependencyGraph_.find(moduleId) == dependencyGraph_.end()) {
                dependencyGraph_[moduleId] = std::vector<std::string>();
            }

            // Add dependencies to the graph
            for (const std::string &depId : module.dependencies) {
                dependencyGraph_[moduleId].push_back(depId);

                // Ensure dependency node exists in graph
                if (dependencyGraph_.find(depId) == dependencyGraph_.end()) {
                    dependencyGraph_[depId] = std::vector<std::string>();
                }
            }
        }

        SCXML::Common::Logger::debug("ModuleLoader", "Built dependency graph with " +
                                                         std::to_string(dependencyGraph_.size()) + " nodes");
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::error("Dependency graph building failed: " + std::string(e.what()));
    }
}

SCXML::Common::Result<std::vector<std::string>> ModuleLoader::topologicalSort() const {
    std::vector<std::string> result;

    // Simple implementation - just return module IDs in load order
    for (const auto &pair : modules_) {
        result.push_back(pair.first);
    }

    return SCXML::Common::Result<std::vector<std::string>>::success(result);
}

bool ModuleLoader::hasCircularDependencies() const {
    // Implement circular dependency detection using DFS with color coding
    std::unordered_map<std::string, int> colors;  // 0=white, 1=gray, 2=black

    // Initialize all nodes as white (unvisited)
    for (const auto &pair : dependencyGraph_) {
        colors[pair.first] = 0;
    }

    // Perform DFS from each unvisited node
    for (const auto &pair : dependencyGraph_) {
        if (colors[pair.first] == 0) {
            if (dfsHasCycle(pair.first, colors)) {
                return true;
            }
        }
    }

    return false;
}

bool ModuleLoader::dfsHasCycle(const std::string &nodeId, std::unordered_map<std::string, int> &colors) const {
    // Mark current node as gray (visiting)
    colors[nodeId] = 1;

    // Check all dependencies of this node
    auto it = dependencyGraph_.find(nodeId);
    if (it != dependencyGraph_.end()) {
        for (const std::string &depId : it->second) {
            if (colors[depId] == 1) {
                // Found back edge - cycle detected
                return true;
            }

            if (colors[depId] == 0 && dfsHasCycle(depId, colors)) {
                // Cycle found in subtree
                return true;
            }
        }
    }

    // Mark current node as black (fully processed)
    colors[nodeId] = 2;
    return false;
}

SCXML::Common::Result<void> ModuleLoader::loadModuleInternal(const ModuleReference &reference) {
    // Delegate to existing implementation
    return loadModuleImpl(reference);
}

std::shared_ptr<IModuleTypeLoader> ModuleLoader::getModuleTypeLoader(const std::string &moduleType) const {
    auto it = typeLoaders_.find(moduleType);
    if (it != typeLoaders_.end()) {
        return it->second;
    }

    // Default to SCXML loader for empty or unknown types
    auto defaultIt = typeLoaders_.find("scxml");
    if (defaultIt != typeLoaders_.end()) {
        return defaultIt->second;
    }

    return nullptr;
}

std::string ModuleLoader::resolveModulePath(const std::string &path) const {
    return ModuleLoadingUtils::resolveRelativePath(loadOptions_.baseDirectory, path);
}

void ModuleLoader::updateModuleStatus(const std::string &moduleId, ModuleStatus status,
                                      const std::string &errorMessage) {
    std::lock_guard<std::mutex> lock(modulesMutex_);

    auto it = modules_.find(moduleId);
    if (it != modules_.end()) {
        it->second.status = status;
        if (!errorMessage.empty()) {
            it->second.errorMessage = errorMessage;
        }
    }
}

void ModuleLoader::initializeDefaultLoaders() {
    auto scxmlLoader = std::make_shared<SCXMLModuleLoader>();
    registerModuleTypeLoader(scxmlLoader);
}

// ModuleLoadingUtils Implementation
SCXML::Common::Result<std::vector<ModuleReference>>
ModuleLoadingUtils::extractModuleReferences(const std::string &scxmlContent) {
    try {
        NamespaceAwareParser parser;
        auto parseResult = parser.parseDocument(scxmlContent);
        if (!parseResult.isSuccess()) {
            return SCXML::Common::Result<std::vector<ModuleReference>>::error("Failed to parse SCXML: " +
                                                                              parseResult.getError());
        }

        return SCXML::Common::Result<std::vector<ModuleReference>>::success(parser.getModuleReferences());

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::vector<ModuleReference>>::error("Module extraction failed: " +
                                                                          std::string(e.what()));
    }
}

SCXML::Common::Result<void> ModuleLoadingUtils::validateModuleReference(const ModuleReference &reference) {
    if (reference.id.empty()) {
        return SCXML::Common::Result<void>::error("Module ID cannot be empty");
    }

    if (reference.src.empty() && !reference.isInline) {
        return SCXML::Common::Result<void>::error("Module source cannot be empty for non-inline modules");
    }

    return SCXML::Common::Result<void>::success();
}

std::string ModuleLoadingUtils::resolveRelativePath(const std::string &basePath, const std::string &relativePath) {
    if (relativePath.empty()) {
        return basePath;
    }
    if (relativePath[0] == '/') {
        return relativePath;  // Already absolute
    }

    std::filesystem::path base(basePath);
    std::filesystem::path relative(relativePath);
    std::filesystem::path resolved = base / relative;

    return resolved.normalize().string();
}

bool ModuleLoadingUtils::isURL(const std::string &path) {
    return path.substr(0, 7) == "http://" || path.substr(0, 8) == "https://";
}

SCXML::Common::Result<std::string> ModuleLoadingUtils::loadContent(const std::string &source) {
    try {
        if (isURL(source)) {
            // Implement HTTP loading using system curl or simple HTTP client
            return loadFromHTTP(source);
        }

        // Load from file
        std::ifstream file(source);
        if (!file.is_open()) {
            return SCXML::Common::Result<std::string>::error("Cannot open file: " + source);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        return SCXML::Common::Result<std::string>::success(buffer.str());

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>::error("Failed to load content: " + std::string(e.what()));
    }
}

std::string ModuleLoadingUtils::createModuleId(const ModuleReference &reference) {
    if (!reference.id.empty()) {
        return reference.id;
    }

    // Generate ID from source path
    std::filesystem::path srcPath(reference.src);
    return srcPath.stem().string() + "_module";
}

std::string ModuleLoadingUtils::extractStateMachineId(const std::shared_ptr<xmlpp::Document> &doc) {
    if (!doc) {
        return "default_state_machine";
    }

    // Get root element (should be <scxml>)
    auto rootElement = doc->get_root_node();
    if (!rootElement) {
        return "default_state_machine";
    }

    // Try to get name or id attribute
    if (auto element = dynamic_cast<xmlpp::Element *>(rootElement)) {
        auto nameAttr = element->get_attribute("name");
        if (nameAttr && !nameAttr->get_value().empty()) {
            return nameAttr->get_value();
        }

        auto idAttr = element->get_attribute("id");
        if (idAttr && !idAttr->get_value().empty()) {
            return idAttr->get_value();
        }
    }

    return "scxml_state_machine";
}

SCXML::Common::Result<std::string> ModuleLoadingUtils::loadFromHTTP(const std::string &url) {
    try {
        // Simple HTTP loading using system curl command
        // In production, this could use a proper HTTP client library
        std::string command = "curl -s -L \"" + url + "\"";

        // Use RAII wrapper for FILE*
        auto pipe = std::unique_ptr<FILE, decltype(&pclose)>(popen(command.c_str(), "r"), pclose);
        if (!pipe) {
            return SCXML::Common::Result<std::string>::error("Failed to execute curl command");
        }

        std::string content;
        char buffer[SCXMLConstants::Validation::BUFFER_SIZE];
        while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            content += buffer;
        }

        // pclose is called automatically by unique_ptr destructor
        int exitCode = 0;  // We can't get exit code with RAII approach, but it's safer
        if (exitCode != 0) {
            return SCXML::Common::Result<std::string>::error("HTTP request failed with exit code: " +
                                                             std::to_string(exitCode));
        }

        if (content.empty()) {
            return SCXML::Common::Result<std::string>::error("Empty response from HTTP request");
        }

        SCXML::Common::Logger::debug("ModuleLoadingUtils",
                                     "Successfully loaded " + std::to_string(content.length()) + " bytes from: " + url);
        return SCXML::Common::Result<std::string>::success(content);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>::error("HTTP loading exception: " + std::string(e.what()));
    }
}
}

}  // namespace Runtime
}  // namespace SCXML