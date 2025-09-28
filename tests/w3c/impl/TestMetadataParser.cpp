#include "TestMetadataParser.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace RSM::W3C {

TestMetadata TestMetadataParser::parseMetadata(const std::string &metadataPath) {
    // Create default metadata first
    TestMetadata metadata;
    int testId = extractTestIdFromPath(metadataPath);

    metadata.id = testId;
    metadata.specnum = "test" + std::to_string(testId);
    metadata.conformance = "mandatory";
    metadata.manual = false;
    metadata.description = "W3C SCXML Test " + std::to_string(testId);

    // Try to read metadata file if it exists
    std::ifstream file(metadataPath);
    if (file.is_open()) {
        std::string line;
        try {
            while (std::getline(file, line)) {
                if (!line.empty() && line[0] != '#') {  // Skip empty lines and comments
                    parseLine(line, metadata);
                }
            }
            validateMetadata(metadata, metadataPath);
        } catch (const std::exception &e) {
            // Log warning but use default metadata
            // Don't throw - allows tests without metadata to run
        }
    }

    return metadata;
}

void TestMetadataParser::parseLine(const std::string &line, TestMetadata &metadata) {
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos) {
        return;  // Skip lines without colon separator
    }

    std::string key = trim(line.substr(0, colonPos));
    std::string value = trim(line.substr(colonPos + 1));

    if (key == "id") {
        metadata.id = parseInt(value);
    } else if (key == "specnum") {
        metadata.specnum = value;
    } else if (key == "conformance") {
        metadata.conformance = value;
    } else if (key == "manual") {
        metadata.manual = parseBool(value);
    } else if (key == "description") {
        metadata.description = value;
    } else if (key == "variants") {
        metadata.variants = parseVariants(value);
    }
    // Ignore unknown keys for forward compatibility
}

std::string TestMetadataParser::trim(const std::string &str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool TestMetadataParser::parseBool(const std::string &value) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
        return true;
    } else if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
        return false;
    } else {
        throw std::invalid_argument("Invalid boolean value: " + value);
    }
}

int TestMetadataParser::parseInt(const std::string &value) {
    try {
        size_t pos;
        int result = std::stoi(value, &pos);
        if (pos != value.length()) {
            throw std::invalid_argument("Invalid integer format: " + value);
        }
        return result;
    } catch (const std::exception &e) {
        throw std::invalid_argument("Cannot parse integer: " + value);
    }
}

std::vector<std::string> TestMetadataParser::parseVariants(const std::string &value) {
    std::vector<std::string> variants;

    // Current W3C tests typically have single variant number
    // Future: could support comma-separated variant list
    if (!value.empty()) {
        variants.push_back(value);
    }

    return variants;
}

void TestMetadataParser::validateMetadata(const TestMetadata &metadata, const std::string &filePath) {
    if (!metadata.isValid()) {
        std::ostringstream oss;
        oss << "Invalid metadata in file " << filePath << ":";

        if (metadata.id <= 0) {
            oss << " missing or invalid id";
        }
        if (metadata.specnum.empty()) {
            oss << " missing specnum";
        }
        if (metadata.conformance.empty()) {
            oss << " missing conformance level";
        }

        throw std::invalid_argument(oss.str());
    }

    // Validate conformance level
    const std::vector<std::string> validConformance = {"mandatory", "optional", "prohibited"};

    if (std::find(validConformance.begin(), validConformance.end(), metadata.conformance) == validConformance.end()) {
        throw std::invalid_argument("Invalid conformance level: " + metadata.conformance + " in file " + filePath);
    }
}

int TestMetadataParser::extractTestIdFromPath(const std::string &metadataPath) {
    // Extract test ID from path like "/path/to/144/metadata.txt" -> 144
    std::filesystem::path path(metadataPath);
    std::string dirName = path.parent_path().filename().string();

    try {
        return std::stoi(dirName);
    } catch (const std::exception &) {
        return 0;  // Default ID if extraction fails
    }
}

}  // namespace RSM::W3C