#include "parsing/XIncludeProcessor.h"
#include "common/Logger.h"
#include <algorithm>
#include <filesystem>

RSM::XIncludeProcessor::XIncludeProcessor() : isProcessing_(false), maxRecursionDepth_(10), currentRecursionDepth_(0) {
    LOG_DEBUG("Creating XInclude processor");
}

RSM::XIncludeProcessor::~XIncludeProcessor() {
    LOG_DEBUG("Destroying XInclude processor");
}

bool RSM::XIncludeProcessor::process(xmlpp::Document *doc) {
    if (!doc) {
        addError("Null document");
        return false;
    }

    if (isProcessing_) {
        addError("XInclude processing already in progress");
        return false;
    }

    try {
        // Initialize
        errorMessages_.clear();
        warningMessages_.clear();
        isProcessing_ = true;
        currentRecursionDepth_ = 0;

        LOG_DEBUG("Starting XInclude processing");

        // Determine document's base directory path
        std::string baseDir = basePath_;
        if (baseDir.empty()) {
            baseDir = ".";  // Current directory
        }

        // Start XInclude processing from root element
        auto rootElement = doc->get_root_node();
        if (rootElement) {
            int processedCount = findAndProcessXIncludes(rootElement, baseDir);
            LOG_DEBUG("Processed {} XInclude directives", processedCount);
        } else {
            addWarning("Document has no root element");
        }

        // Call built-in XInclude processor (libxml's XInclude processor)
        // Use libxml++'s XInclude function if available, otherwise call libxml2 directly
        try {
            doc->process_xinclude();
            LOG_DEBUG("Native XInclude processing successful");
        } catch (const std::exception &ex) {
            addWarning("Native XInclude processing failed: " + std::string(ex.what()));
        }

        isProcessing_ = false;
        return errorMessages_.empty();
    } catch (const std::exception &ex) {
        isProcessing_ = false;
        addError("Exception during XInclude processing: " + std::string(ex.what()));
        return false;
    }
}

void RSM::XIncludeProcessor::setBasePath(const std::string &basePath) {
    basePath_ = basePath;
    LOG_DEBUG("Base path set to: {}", basePath);
}

void RSM::XIncludeProcessor::addSearchPath(const std::string &searchPath) {
    searchPaths_.push_back(searchPath);
    LOG_DEBUG("Added search path: {}", searchPath);
}

const std::vector<std::string> &RSM::XIncludeProcessor::getErrorMessages() const {
    return errorMessages_;
}

const std::vector<std::string> &RSM::XIncludeProcessor::getWarningMessages() const {
    return warningMessages_;
}

const std::unordered_map<std::string, int> &RSM::XIncludeProcessor::getProcessedFiles() const {
    return processedFiles_;
}

int RSM::XIncludeProcessor::findAndProcessXIncludes(xmlpp::Element *element, const std::string &baseDir) {
    if (!element) {
        return 0;
    }

    // Check recursion depth limit
    if (currentRecursionDepth_ >= maxRecursionDepth_) {
        addWarning("Maximum recursion depth reached, stopping XInclude processing");
        return 0;
    }

    int processedCount = 0;
    currentRecursionDepth_++;

    // Check if current element is an XInclude element
    std::string nodeName = element->get_name();
    bool isXInclude = (nodeName == "include" || nodeName == "xi:include");

    if (isXInclude) {
        if (processXIncludeElement(element, baseDir)) {
            processedCount++;
        }
    } else {
        // Process child elements recursively
        auto children = element->get_children();
        for (auto *child : children) {
            auto *childElement = dynamic_cast<xmlpp::Element *>(child);
            if (childElement) {
                processedCount += findAndProcessXIncludes(childElement, baseDir);
            }
        }
    }

    currentRecursionDepth_--;
    return processedCount;
}

bool RSM::XIncludeProcessor::processXIncludeElement(xmlpp::Element *xincludeElement, const std::string &baseDir) {
    if (!xincludeElement) {
        return false;
    }

    LOG_DEBUG("Processing XInclude element");

    // Check href attribute
    auto hrefAttr = xincludeElement->get_attribute("href");
    if (!hrefAttr) {
        addWarning("XInclude element missing href attribute");
        return false;
    }

    std::string href = hrefAttr->get_value();
    if (href.empty()) {
        addWarning("XInclude href is empty");
        return false;
    }

    // Check parse attribute (xml or text)
    std::string parseMode = "xml";  // Default value
    auto parseAttr = xincludeElement->get_attribute("parse");
    if (parseAttr) {
        parseMode = parseAttr->get_value();
    }

    // Only XML mode is supported
    if (parseMode != "xml") {
        addWarning("XInclude parse mode '" + parseMode + "' not supported, only 'xml' is supported");
        return false;
    }

    // Load and merge file
    return loadAndMergeFile(href, xincludeElement, baseDir);
}

bool RSM::XIncludeProcessor::loadAndMergeFile(const std::string &href, xmlpp::Element *xincludeElement,
                                              const std::string &baseDir) {
    if (href.empty() || !xincludeElement) {
        return false;
    }

    // Resolve file path
    std::string fullPath = resolveFilePath(href, baseDir);
    if (fullPath.empty()) {
        addError("Could not resolve file path: " + href);
        return false;
    }

    LOG_DEBUG("Loading: {}", fullPath);

    try {
        // Check if file exists
        if (!std::filesystem::exists(fullPath)) {
            addError("File not found: " + fullPath);
            return false;
        }

        // Check for circular references
        if (processedFiles_.find(fullPath) != processedFiles_.end()) {
            addWarning("Circular reference detected: " + fullPath);
            return false;
        }

        // Parse file
        xmlpp::DomParser parser;
        parser.set_validate(false);
        parser.set_substitute_entities(true);
        parser.parse_file(fullPath);

        auto includedDoc = parser.get_document();
        if (!includedDoc) {
            addError("Failed to parse included file: " + fullPath);
            return false;
        }

        auto includedRoot = includedDoc->get_root_node();
        if (!includedRoot) {
            addError("Included file has no root element: " + fullPath);
            return false;
        }

        // Process XIncludes in the included file as well (recursive)
        std::string includedBaseDir = std::filesystem::path(fullPath).parent_path().string();
        findAndProcessXIncludes(includedRoot, includedBaseDir);

        // Get parent node and XInclude element
        auto parent = xincludeElement->get_parent();
        auto *parentElement = dynamic_cast<xmlpp::Element *>(parent);
        if (!parentElement) {
            addError("XInclude element has no parent element");
            return false;
        }

        // Copy and add children of included node to parent
        auto children = includedRoot->get_children();
        for (auto *child : children) {
            try {
                // parentElement->import_node() copies the node and adds it as a child
                parentElement->import_node(child);
            } catch (const std::exception &ex) {
                addError("Exception while importing node from " + fullPath + ": " + std::string(ex.what()));
            }
        }

        // Remove XInclude element
        try {
            // Node::remove_node is a static method, call with 'Node::'
            xmlpp::Node::remove_node(xincludeElement);
        } catch (const std::exception &ex) {
            addWarning("Exception while removing XInclude element: " + std::string(ex.what()));
            // Continue processing
        }

        // Track processed files
        processedFiles_[fullPath]++;

        LOG_DEBUG("Successfully merged: {}", fullPath);
        return true;
    } catch (const std::exception &ex) {
        addError("Exception while processing included file " + fullPath + ": " + std::string(ex.what()));
        return false;
    }
}

std::string RSM::XIncludeProcessor::resolveFilePath(const std::string &href, const std::string &baseDir) {
    // Use as-is if absolute path
    if (std::filesystem::path(href).is_absolute()) {
        return href;
    }

    // Try relative path to base directory
    std::string fullPath = std::filesystem::path(baseDir) / href;
    if (std::filesystem::exists(fullPath)) {
        return std::filesystem::absolute(fullPath).string();
    }

    // Search for file in configured search paths
    for (const auto &searchPath : searchPaths_) {
        fullPath = std::filesystem::path(searchPath) / href;
        if (std::filesystem::exists(fullPath)) {
            return std::filesystem::absolute(fullPath).string();
        }
    }

    // File not found
    addWarning("File not found in any search path: " + href);
    return "";
}

void RSM::XIncludeProcessor::addError(const std::string &message) {
    LOG_ERROR("XIncludeProcessor - {}", message);
    errorMessages_.push_back(message);
}

void RSM::XIncludeProcessor::addWarning(const std::string &message) {
    LOG_WARN("XIncludeProcessor - {}", message);
    warningMessages_.push_back(message);
}
