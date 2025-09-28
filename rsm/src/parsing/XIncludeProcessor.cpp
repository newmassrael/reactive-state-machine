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
        // 초기화
        errorMessages_.clear();
        warningMessages_.clear();
        isProcessing_ = true;
        currentRecursionDepth_ = 0;

        LOG_DEBUG("Starting XInclude processing");

        // 문서의 기본 디렉토리 경로 결정
        std::string baseDir = basePath_;
        if (baseDir.empty()) {
            baseDir = ".";  // 현재 디렉토리
        }

        // 루트 요소부터 XInclude 처리 시작
        auto rootElement = doc->get_root_node();
        if (rootElement) {
            int processedCount = findAndProcessXIncludes(rootElement, baseDir);
            LOG_DEBUG("Processed {} XInclude directives", processedCount);
        } else {
            addWarning("Document has no root element");
        }

        // 내장 XInclude 처리기 호출 (libxml의 XInclude 처리기)
        // libxml++의 XInclude 처리 함수가 있다면 사용, 없으면 libxml2 직접 호출
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

    // 재귀 깊이 제한 확인
    if (currentRecursionDepth_ >= maxRecursionDepth_) {
        addWarning("Maximum recursion depth reached, stopping XInclude processing");
        return 0;
    }

    int processedCount = 0;
    currentRecursionDepth_++;

    // 현재 요소가 XInclude 요소인지 확인
    std::string nodeName = element->get_name();
    bool isXInclude = (nodeName == "include" || nodeName == "xi:include");

    if (isXInclude) {
        if (processXIncludeElement(element, baseDir)) {
            processedCount++;
        }
    } else {
        // 자식 요소들에 대해 재귀적으로 처리
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

    // href 속성 확인
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

    // parse 속성 확인 (xml 또는 text)
    std::string parseMode = "xml";  // 기본값
    auto parseAttr = xincludeElement->get_attribute("parse");
    if (parseAttr) {
        parseMode = parseAttr->get_value();
    }

    // XML 모드만 지원
    if (parseMode != "xml") {
        addWarning("XInclude parse mode '" + parseMode + "' not supported, only 'xml' is supported");
        return false;
    }

    // 파일 로드 및 병합
    return loadAndMergeFile(href, xincludeElement, baseDir);
}

bool RSM::XIncludeProcessor::loadAndMergeFile(const std::string &href, xmlpp::Element *xincludeElement,
                                              const std::string &baseDir) {
    if (href.empty() || !xincludeElement) {
        return false;
    }

    // 파일 경로 해석
    std::string fullPath = resolveFilePath(href, baseDir);
    if (fullPath.empty()) {
        addError("Could not resolve file path: " + href);
        return false;
    }

    LOG_DEBUG("Loading: {}", fullPath);

    try {
        // 파일 존재 여부 확인
        if (!std::filesystem::exists(fullPath)) {
            addError("File not found: " + fullPath);
            return false;
        }

        // 순환 참조 확인
        if (processedFiles_.find(fullPath) != processedFiles_.end()) {
            addWarning("Circular reference detected: " + fullPath);
            return false;
        }

        // 파일 파싱
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

        // 포함된 파일의 XInclude도 처리 (재귀)
        std::string includedBaseDir = std::filesystem::path(fullPath).parent_path().string();
        findAndProcessXIncludes(includedRoot, includedBaseDir);

        // 부모 노드와 XInclude 요소 가져오기
        auto parent = xincludeElement->get_parent();
        auto *parentElement = dynamic_cast<xmlpp::Element *>(parent);
        if (!parentElement) {
            addError("XInclude element has no parent element");
            return false;
        }

        // 포함된 노드의 자식들을 복사하여 부모에 추가
        auto children = includedRoot->get_children();
        for (auto *child : children) {
            try {
                // parentElement->import_node()는 노드를 복사하고 자식으로 추가함
                parentElement->import_node(child);
            } catch (const std::exception &ex) {
                addError("Exception while importing node from " + fullPath + ": " + std::string(ex.what()));
            }
        }

        // XInclude 요소 제거
        try {
            // Node::remove_node는 정적 메서드이므로 'Node::'로 호출합니다
            xmlpp::Node::remove_node(xincludeElement);
        } catch (const std::exception &ex) {
            addWarning("Exception while removing XInclude element: " + std::string(ex.what()));
            // 계속 진행
        }

        // 처리된 파일 추적
        processedFiles_[fullPath]++;

        LOG_DEBUG("Successfully merged: {}", fullPath);
        return true;
    } catch (const std::exception &ex) {
        addError("Exception while processing included file " + fullPath + ": " + std::string(ex.what()));
        return false;
    }
}

std::string RSM::XIncludeProcessor::resolveFilePath(const std::string &href, const std::string &baseDir) {
    // 절대 경로인 경우 그대로 사용
    if (std::filesystem::path(href).is_absolute()) {
        return href;
    }

    // 기본 디렉토리에 대한 상대 경로 시도
    std::string fullPath = std::filesystem::path(baseDir) / href;
    if (std::filesystem::exists(fullPath)) {
        return std::filesystem::absolute(fullPath).string();
    }

    // 설정된 검색 경로에서 파일 찾기
    for (const auto &searchPath : searchPaths_) {
        fullPath = std::filesystem::path(searchPath) / href;
        if (std::filesystem::exists(fullPath)) {
            return std::filesystem::absolute(fullPath).string();
        }
    }

    // 파일을 찾을 수 없음
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
