#pragma once

#include "../interfaces/ITestMetadataParser.h"
#include <fstream>
#include <unordered_map>

namespace RSM::W3C {

/**
 * @brief Concrete implementation of W3C test metadata parser
 *
 * Parses metadata.txt files with format:
 * id: 144
 * specnum: 4.2
 * conformance: mandatory
 * manual: False
 * description: Test description...
 * variants: 1
 */
class TestMetadataParser : public ITestMetadataParser {
private:
    /**
     * @brief Parse a single line of metadata
     * @param line Line in format "key: value"
     * @param metadata Metadata structure to populate
     */
    void parseLine(const std::string &line, TestMetadata &metadata);

    /**
     * @brief Trim whitespace from string
     */
    std::string trim(const std::string &str);

    /**
     * @brief Parse boolean value from string
     */
    bool parseBool(const std::string &value);

    /**
     * @brief Parse integer value from string
     */
    int parseInt(const std::string &value);

    /**
     * @brief Parse variants list (currently single integer)
     */
    std::vector<std::string> parseVariants(const std::string &value);

    /**
     * @brief Validate parsed metadata
     */
    void validateMetadata(const TestMetadata &metadata, const std::string &filePath);

    /**
     * @brief Extract test ID from metadata file path
     */
    int extractTestIdFromPath(const std::string &metadataPath);

public:
    TestMetadataParser() = default;
    ~TestMetadataParser() override = default;

    /**
     * @brief Parse test metadata from metadata.txt file
     * @param metadataPath Path to metadata.txt file
     * @return Parsed and validated metadata structure
     * @throws std::runtime_error if file cannot be read or parsed
     * @throws std::invalid_argument if metadata format is invalid
     */
    TestMetadata parseMetadata(const std::string &metadataPath) override;
};

}  // namespace RSM::W3C