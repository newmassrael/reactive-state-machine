// TXML to SCXML Converter CLI Tool
// Converts W3C SCXML Test Suite TXML files to standard SCXML format

#include "tests/w3c/impl/TXMLConverter.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

void printUsage(const char *programName) {
    std::cout << "Usage: " << programName << " <input.txml> <output.scxml>\n";
    std::cout << "   or: " << programName << " <input.txml> (outputs to stdout)\n\n";
    std::cout << "Convert W3C SCXML Test Suite TXML files to standard SCXML format.\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  input.txml    Path to input TXML file\n";
    std::cout << "  output.scxml  Path to output SCXML file (optional)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " test144.txml test144.scxml\n";
    std::cout << "  " << programName << " test144.txml > test144.scxml\n";
}

std::string readFile(const fs::path &filePath) {
    if (!fs::exists(filePath)) {
        throw std::runtime_error("File does not exist: " + filePath.string());
    }

    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filePath.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void writeFile(const fs::path &filePath, const std::string &content) {
    std::ofstream file(filePath, std::ios::out | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to create output file: " + filePath.string());
    }

    file << content;
    if (!file) {
        throw std::runtime_error("Failed to write to output file: " + filePath.string());
    }
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        printUsage(argv[0]);
        return 1;
    }

    // Parse command line arguments
    fs::path inputPath = argv[1];
    bool writeToStdout = (argc == 2);
    fs::path outputPath;

    if (!writeToStdout) {
        outputPath = argv[2];
    }

    try {
        // Read TXML file
        std::string txmlContent = readFile(inputPath);

        // Convert TXML to SCXML
        RSM::W3C::TXMLConverter converter;
        std::string scxmlContent = converter.convertTXMLToSCXML(txmlContent);

        // Output result
        if (writeToStdout) {
            std::cout << scxmlContent;
        } else {
            writeFile(outputPath, scxmlContent);
            std::cerr << "Conversion successful: " << inputPath.string() << " -> " << outputPath.string() << "\n";
        }

        return 0;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
