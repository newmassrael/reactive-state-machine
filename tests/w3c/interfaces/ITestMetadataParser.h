#pragma once

#include <string>
#include <vector>

namespace RSM::W3C {

/**
 * @brief Test metadata structure from W3C test definition
 */
struct TestMetadata {
    int id{0};
    std::string specnum;
    std::string conformance;  // mandatory, optional, etc.
    bool manual{false};
    std::string description;
    std::vector<std::string> variants;

    bool isValid() const {
        return id > 0 && !specnum.empty() && !conformance.empty();
    }
};

/**
 * @brief Interface for parsing W3C test metadata files
 *
 * Single Responsibility: Only parses metadata.txt files
 * - Extracts test properties (id, specnum, conformance, etc.)
 * - Validates metadata format
 */
class ITestMetadataParser {
public:
    virtual ~ITestMetadataParser() = default;

    /**
     * @brief Parse test metadata from metadata.txt file
     * @param metadataPath Path to metadata.txt file
     * @return Parsed metadata structure
     * @throws std::runtime_error if file cannot be read or parsed
     */
    virtual TestMetadata parseMetadata(const std::string &metadataPath) = 0;
};

}  // namespace RSM::W3C