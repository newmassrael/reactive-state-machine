#include "IntegrationTestCommon.h"
#include "common/Logger.h"
#include "parsing/XSDValidator.h"
#include <libxml++/parsers/domparser.h>

bool IntegrationTestBase::validateSCXMLDocument(const std::string &scxml) {
    try {
        // 1. XML 구조적 검증 (기본 파싱)
        xmlpp::DomParser domParser;
        domParser.parse_memory(scxml);

        auto document = domParser.get_document();
        if (!document) {
            SCXML::Common::Logger::error("validateSCXMLDocument: Failed to parse XML document");
            return false;
        }

        // 2. XSD 스키마 검증
        SCXML::Parsing::XSDValidator validator;
        if (!validator.isInitialized()) {
            SCXML::Common::Logger::warning(
                "validateSCXMLDocument: XSD validator not available, skipping schema validation");
        } else {
            if (!validator.validateDocument(*document)) {
                const auto &errors = validator.getErrors();
                for (const auto &error : errors) {
                    if (error.severity == SCXML::Parsing::ValidationError::Severity::ERROR ||
                        error.severity == SCXML::Parsing::ValidationError::Severity::FATAL_ERROR) {
                        SCXML::Common::Logger::error("XSD validation error: " + error.message);
                        return false;
                    } else {
                        SCXML::Common::Logger::warning("XSD validation warning: " + error.message);
                    }
                }
            }
        }

        // 3. 코드 레벨 검증 (DocumentParser를 통한 구조 검증)
        auto model = parser->parseContent(scxml);
        if (!model) {
            SCXML::Common::Logger::error("validateSCXMLDocument: DocumentParser validation failed");
            if (parser->hasErrors()) {
                for (const auto &error : parser->getErrorMessages()) {
                    SCXML::Common::Logger::error("Parser error: " + error);
                }
            }
            return false;
        }

        SCXML::Common::Logger::debug("validateSCXMLDocument: All validation checks passed");
        return true;

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("validateSCXMLDocument: Exception during validation - " + std::string(e.what()));
        return false;
    }
}

void IntegrationTestBase::executeStateMachine(const std::string &scxml, const std::vector<std::string> &events) {
    // Suppress unused parameter warning - events will be used in future implementation
    (void)events;
    // 상태 머신 실행 테스트 구현
    // 현재는 기본 파싱만 수행
    auto model = parser->parseContent(scxml);
    ASSERT_TRUE(model != nullptr) << "Failed to parse SCXML for execution test";

    // TODO: 실제 상태 머신 실행 로직은 RuntimeContext와 함께 구현 예정
    SCXML::Common::Logger::info(
        "executeStateMachine: Parsed SCXML model successfully. Event execution not yet implemented.");
}