#pragma once

#include <string>

namespace RSM::W3C {

/**
 * @brief Interface for converting TXML test files to SCXML format
 *
 * Single Responsibility: Only handles TXML to SCXML conversion
 * - Processes conf: namespace attributes
 * - Converts test-specific markup to standard SCXML
 */
class ITestConverter {
public:
    virtual ~ITestConverter() = default;

    /**
     * @brief Convert TXML content to valid SCXML
     * @param txml The TXML content with conf: namespace attributes
     * @return Valid SCXML content ready for parsing
     * @throws std::invalid_argument if TXML is malformed
     */
    virtual std::string convertTXMLToSCXML(const std::string &txml) = 0;
};

}  // namespace RSM::W3C