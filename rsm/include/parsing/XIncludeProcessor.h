#pragma once

#include "IXIncludeProcessor.h"
#include <libxml++/libxml++.h>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Class responsible for XInclude processing
 *
 * This class provides functionality to process XInclude directives in SCXML documents.
 * It handles loading and integrating external files, supporting both relative and absolute paths.
 */

namespace RSM {

class XIncludeProcessor : public IXIncludeProcessor {
public:
    /**
     * @brief Constructor
     */
    XIncludeProcessor();

    /**
     * @brief Destructor
     */
    ~XIncludeProcessor() override;

    /**
     * @brief Execute XInclude processing
     * @param doc libxml++ document object
     * @return Success status
     */
    bool process(xmlpp::Document *doc) override;

    /**
     * @brief Set base search path
     * @param basePath Base search path
     */
    void setBasePath(const std::string &basePath) override;

    /**
     * @brief Add search path
     * @param searchPath Search path to add
     */
    void addSearchPath(const std::string &searchPath);

    /**
     * @brief Return error messages that occurred during processing
     * @return List of error messages
     */
    const std::vector<std::string> &getErrorMessages() const override;

    /**
     * @brief Return warning messages that occurred during processing
     * @return List of warning messages
     */
    const std::vector<std::string> &getWarningMessages() const;

    /**
     * @brief Return list of already processed files
     * @return List of processed files (path -> node count)
     */
    const std::unordered_map<std::string, int> &getProcessedFiles() const;

private:
    /**
     * @brief Find and process XInclude elements
     * @param element Element to start search from
     * @param baseDir Base directory
     * @return Number of processed XInclude elements
     */
    int findAndProcessXIncludes(xmlpp::Element *element, const std::string &baseDir);

    /**
     * @brief Process single XInclude element
     * @param xincludeElement XInclude element
     * @param baseDir Base directory
     * @return Success status
     */
    bool processXIncludeElement(xmlpp::Element *xincludeElement, const std::string &baseDir);

    /**
     * @brief Load and merge external file
     * @param href File path
     * @param xincludeElement XInclude element
     * @param baseDir Base directory
     * @return Success status
     */
    bool loadAndMergeFile(const std::string &href, xmlpp::Element *xincludeElement, const std::string &baseDir);

    /**
     * @brief Resolve file path
     * @param href Original path
     * @param baseDir Base directory
     * @return Resolved absolute path
     */
    std::string resolveFilePath(const std::string &href, const std::string &baseDir);

    /**
     * @brief Add error message
     * @param message Error message
     */
    void addError(const std::string &message);

    /**
     * @brief Add warning message
     * @param message Warning message
     */
    void addWarning(const std::string &message);

    std::string basePath_;
    std::vector<std::string> searchPaths_;
    std::vector<std::string> errorMessages_;
    std::vector<std::string> warningMessages_;
    std::unordered_map<std::string, int> processedFiles_;
    bool isProcessing_;
    int maxRecursionDepth_;
    int currentRecursionDepth_;
};

}  // namespace RSM