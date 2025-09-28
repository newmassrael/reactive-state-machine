#pragma once

#include <memory>
#include <string>
#include <vector>

namespace RSM::W3C {

/**
 * @brief Test suite discovery information
 */
struct TestSuiteInfo {
    std::string name;
    std::string description;
    std::string resourcePath;
    size_t totalTests{0};
};

/**
 * @brief Interface for test suite discovery and management
 *
 * Single Responsibility: Only discovers and organizes test suites
 * - Scans directories for test files
 * - Provides test enumeration
 * - Supports different test suite types
 *
 * Open/Closed Principle: New test suite types can be added
 */
class ITestSuite {
public:
    virtual ~ITestSuite() = default;

    /**
     * @brief Get basic information about this test suite
     * @return Test suite metadata
     */
    virtual TestSuiteInfo getInfo() const = 0;

    /**
     * @brief Discover all available test directories
     * @return List of test directory paths (e.g., "resources/144", "resources/147")
     */
    virtual std::vector<std::string> discoverTests() = 0;

    /**
     * @brief Get path to TXML file for a specific test
     * @param testDirectory Test directory (e.g., "resources/144")
     * @return Full path to TXML file
     */
    virtual std::string getTXMLPath(const std::string &testDirectory) = 0;

    /**
     * @brief Get path to metadata file for a specific test
     * @param testDirectory Test directory (e.g., "resources/144")
     * @return Full path to metadata.txt file
     */
    virtual std::string getMetadataPath(const std::string &testDirectory) = 0;

    /**
     * @brief Filter tests based on criteria
     * @param conformanceLevel Filter by conformance level (mandatory, optional, etc.)
     * @param specSection Filter by spec section (e.g., "4.2")
     * @return Filtered list of test directories
     */
    virtual std::vector<std::string> filterTests(const std::string &conformanceLevel = "",
                                                 const std::string &specSection = "") = 0;
};

}  // namespace RSM::W3C