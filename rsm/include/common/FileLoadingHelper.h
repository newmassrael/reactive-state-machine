#pragma once

#include "common/Logger.h"
#include <fstream>
#include <sstream>
#include <string>

namespace RSM {

/**
 * @brief Helper functions for W3C SCXML external file loading
 *
 * Single Source of Truth for file loading logic shared between:
 * - Python code generator (scxml_parser.py - build-time)
 * - Interpreter engine (DataModelParser.cpp - runtime)
 * - StateMachine (StateMachine.cpp - runtime)
 *
 * W3C SCXML References:
 * - 5.2.2: Data Model - src attribute for external content
 * - 3.3: External SCXML file loading
 */
class FileLoadingHelper {
public:
    /**
     * @brief Normalize file path by removing URI prefix
     *
     * Single Source of Truth for path normalization.
     * W3C SCXML 5.2.2: src attribute may use "file:" URI scheme.
     *
     * @param srcPath Source path (may include "file:" prefix)
     * @return Normalized file path without URI prefix
     */
    static std::string normalizePath(const std::string &srcPath) {
        // W3C SCXML 5.2.2: Remove "file:" or "file://" prefix
        if (srcPath.find("file://") == 0) {
            return srcPath.substr(7);  // Remove "file://"
        } else if (srcPath.find("file:") == 0) {
            return srcPath.substr(5);  // Remove "file:"
        }
        return srcPath;
    }

    /**
     * @brief Load file content from disk
     *
     * Single Source of Truth for file loading logic.
     * Used by both Interpreter (runtime) and Python codegen (build-time).
     *
     * W3C SCXML 5.2.2: Content from external file via src attribute.
     *
     * @param filePath Path to file (already normalized)
     * @param content Output parameter for file content
     * @return true if file loaded successfully, false on error
     */
    static bool loadFileContent(const std::string &filePath, std::string &content) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG_ERROR("FileLoadingHelper: Failed to open file: {}", filePath);
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();

        // W3C SCXML 5.2.2: Trim whitespace for consistency
        // Remove leading/trailing whitespace
        size_t start = content.find_first_not_of(" \t\r\n");
        size_t end = content.find_last_not_of(" \t\r\n");

        if (start != std::string::npos && end != std::string::npos) {
            content = content.substr(start, end - start + 1);
        } else if (start == std::string::npos) {
            content = "";  // All whitespace
        }

        return true;
    }

    /**
     * @brief Load and normalize file content from src attribute
     *
     * Convenience function combining path normalization and file loading.
     *
     * @param srcAttribute Value of src attribute (may include "file:" prefix)
     * @param content Output parameter for file content
     * @return true if file loaded successfully, false on error
     */
    static bool loadFromSrc(const std::string &srcAttribute, std::string &content) {
        std::string normalizedPath = normalizePath(srcAttribute);
        return loadFileContent(normalizedPath, content);
    }
};

}  // namespace RSM
