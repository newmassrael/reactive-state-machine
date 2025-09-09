#include "parsing/DocumentParser.h"
#include "common/GuardUtils.h"
#include "common/Logger.h"
#include "parsing/ActionParser.h"
#include "parsing/DoneDataParser.h"
#include "parsing/GuardParser.h"
#include "parsing/InvokeParser.h"
#include "parsing/ParsingCommon.h"
#include "parsing/TransitionParser.h"
#include <algorithm>
#include <filesystem>
#include <functional>

using namespace SCXML::Parsing;
using namespace std;

DocumentParser::DocumentParser(std::shared_ptr<SCXML::Model::INodeFactory> nodeFactory,
                               std::shared_ptr<SCXML::Model::IXIncludeProcessor> xincludeProcessor)
    : nodeFactory_(nodeFactory), xsdValidationEnabled_(false) {
    SCXML::Common::Logger::debug("DocumentParser::Constructor - Creating SCXML parser");

    // 전문화된 파서 초기화
    stateNodeParser_ = std::make_shared<SCXML::Parsing::StateNodeParser>(nodeFactory_);
    transitionParser_ = std::make_shared<TransitionParser>(nodeFactory_);
    actionParser_ = std::make_shared<ActionParser>(nodeFactory_);
    guardParser_ = std::make_shared<GuardParser>(nodeFactory_);
    dataModelParser_ = std::make_shared<DataModelParser>(nodeFactory_);
    invokeParser_ = std::make_shared<InvokeParser>(nodeFactory_);
    doneDataParser_ = std::make_shared<DoneDataParser>(nodeFactory_);

    // 관련 파서들을 연결
    stateNodeParser_->setRelatedParsers(transitionParser_, actionParser_, dataModelParser_, invokeParser_,
                                        doneDataParser_);

    // TransitionParser에 ActionParser 설정
    transitionParser_->setActionParser(actionParser_);

    // XInclude 프로세서 설정
    if (xincludeProcessor) {
        xincludeProcessor_ = xincludeProcessor;
    } else {
        xincludeProcessor_ = std::make_shared<SCXML::Parsing::XIncludeProcessor>();
    }

    // XSD 검증기 초기화 (기본적으로 비활성화됨)
    try {
        xsdValidator_ = std::make_unique<XSDValidator>();
        if (!xsdValidator_->isInitialized()) {
            SCXML::Common::Logger::warning("XSD validator initialization failed - validation disabled");
        }
    } catch (const std::exception &e) {
        SCXML::Common::Logger::warning("XSD validator creation failed: " + std::string(e.what()));
    }
}

DocumentParser::~DocumentParser() {
    SCXML::Common::Logger::debug("DocumentParser::Destructor - Destroying SCXML parser");
}

std::shared_ptr<::SCXML::Model::DocumentModel> DocumentParser::parseFile(const std::string &filename) {
    try {
        // 파싱 상태 초기화
        initParsing();

        // 파일이 존재하는지 확인
        if (!std::filesystem::exists(filename)) {
            addError("File not found: " + filename);
            return nullptr;
        }

        SCXML::Common::Logger::info("DocumentParser::parseFile() - Parsing SCXML file: " + filename);

        // 파일 파싱
        xmlpp::DomParser parser;
        parser.set_validate(false);
        parser.set_substitute_entities(true);  // 엔티티 대체 활성화
        parser.parse_file(filename);

        // XInclude 처리
        SCXML::Common::Logger::debug("DocumentParser::parseFile() - Processing XIncludes");
        xincludeProcessor_->process(parser.get_document());

        // 문서 파싱
        return parseDocument(parser.get_document());
    } catch (const std::exception &ex) {
        addError("Exception while parsing file: " + std::string(ex.what()));
        return nullptr;
    }
}

std::shared_ptr<::SCXML::Model::DocumentModel> DocumentParser::parseContent(const std::string &content) {
    try {
        // 파싱 상태 초기화
        initParsing();

        SCXML::Common::Logger::info("DocumentParser::parseContent() - Parsing SCXML content");

        // 문자열에서 파싱
        xmlpp::DomParser parser;
        parser.set_validate(false);
        parser.set_substitute_entities(true);

        // XML 네임스페이스 인식 활성화
        parser.set_throw_messages(true);

        parser.parse_memory(content);

        // XInclude 처리
        SCXML::Common::Logger::debug("DocumentParser::parseContent() - Processing XIncludes");
        xincludeProcessor_->process(parser.get_document());

        // 문서 파싱
        return parseDocument(parser.get_document());
    } catch (const std::exception &ex) {
        addError("Exception while parsing content: " + std::string(ex.what()));
        return nullptr;
    }
}

std::shared_ptr<::SCXML::Model::DocumentModel> DocumentParser::parseDocument(xmlpp::Document *doc) {
    if (!doc) {
        addError("Null document");
        return nullptr;
    }

    // XSD 스키마 검증 수행 (활성화된 경우)
    if (xsdValidationEnabled_ && xsdValidator_ && xsdValidator_->isInitialized()) {
        SCXML::Common::Logger::debug("DocumentParser::parseDocument - Performing XSD validation");

        if (!xsdValidator_->validateDocument(*doc)) {
            const auto &errors = xsdValidator_->getErrors();
            for (const auto &error : errors) {
                if (error.severity == ValidationError::Severity::ERROR ||
                    error.severity == ValidationError::Severity::FATAL_ERROR) {
                    addError("XSD validation error: " + error.message);
                } else {
                    addWarning("XSD validation warning: " + error.message);
                }
            }

            if (!errors.empty()) {
                auto errorCount = std::count_if(errors.begin(), errors.end(), [](const ValidationError &e) {
                    return e.severity == ValidationError::Severity::ERROR ||
                           e.severity == ValidationError::Severity::FATAL_ERROR;
                });

                if (errorCount > 0) {
                    SCXML::Common::Logger::error("Document failed XSD validation with " + std::to_string(errorCount) +
                                                 " errors");
                    return nullptr;
                }
            }
        } else {
            SCXML::Common::Logger::debug("XSD validation passed successfully");
        }
    }

    // 루트 요소 가져오기
    xmlpp::Element *rootElement = doc->get_root_node();
    if (!rootElement) {
        addError("No root element found");
        return nullptr;
    }

    // 루트 요소가 scxml인지 확인
    if (!SCXML::Parsing::ParsingCommon::matchNodeName(rootElement->get_name(), "scxml")) {
        addError("Root element is not 'scxml', found: " + rootElement->get_name());
        return nullptr;
    }

    SCXML::Common::Logger::info("DocumentParser::parseDocument() - Valid SCXML document found, parsing structure");

    // SCXML 모델 생성
    auto model = std::make_shared<::SCXML::Model::DocumentModel>();

    // SCXML 노드 파싱
    bool result = parseScxmlNode(rootElement, model);
    if (result) {
        SCXML::Common::Logger::info("DocumentParser::parseDocument() - SCXML document parsed successfully");

        // 모델 검증
        if (validateModel(model)) {
            return model;
        } else {
            SCXML::Common::Logger::error("DocumentParser::parseDocument() - SCXML model validation failed");
            return nullptr;
        }
    } else {
        SCXML::Common::Logger::error("DocumentParser::parseDocument() - Failed to parse SCXML document");
        return nullptr;
    }
}

bool DocumentParser::parseScxmlNode(const xmlpp::Element *scxmlNode,
                                    std::shared_ptr<::SCXML::Model::DocumentModel> model) {
    if (!scxmlNode || !model) {
        addError("Null scxml node or model");
        return false;
    }

    SCXML::Common::Logger::debug("DocumentParser::parseScxmlNode() - Parsing SCXML root node");

    // ParsingContext 생성 및 초기화
    SCXML::Model::ParsingContext context;

    // 기본 속성 파싱
    auto nameAttr = scxmlNode->get_attribute("name");
    if (nameAttr) {
        model->setName(nameAttr->get_value());
        SCXML::Common::Logger::debug("DocumentParser::parseScxmlNode() - Name: " + nameAttr->get_value());
    }

    auto initialAttr = scxmlNode->get_attribute("initial");
    if (initialAttr) {
        model->setInitialState(initialAttr->get_value());
        SCXML::Common::Logger::debug("DocumentParser::parseScxmlNode() - Initial state: " + initialAttr->get_value());
    }

    auto datamodelAttr = scxmlNode->get_attribute("datamodel");
    if (datamodelAttr) {
        std::string datamodelType = datamodelAttr->get_value();
        model->setDatamodel(datamodelType);
        context.setDatamodelType(datamodelType);  // ParsingContext에 설정
        SCXML::Common::Logger::debug("DocumentParser::parseScxmlNode() - Datamodel: " + datamodelType);
    }

    auto bindingAttr = scxmlNode->get_attribute("binding");
    if (bindingAttr) {
        std::string binding = bindingAttr->get_value();
        model->setBinding(binding);
        context.setBinding(binding);  // ParsingContext에 설정
        SCXML::Common::Logger::debug("DocumentParser::parseScxmlNode() - Binding mode: " + binding);
    }

    // 컨텍스트 속성 파싱
    parseContextProperties(scxmlNode, model);

    // 의존성 주입 지점 파싱
    parseInjectPoints(scxmlNode, model);

    // 가드 조건 파싱
    SCXML::Common::Logger::debug("DocumentParser::parseScxmlNode() - Parsing guards");
    auto guards = guardParser_->parseAllGuards(scxmlNode);
    for (const auto &guard : guards) {
        model->addGuard(guard);

        std::string logMessage = "DocumentParser::parseScxmlNode() - Added guard: " + guard->getId();
        if (!guard->getCondition().empty()) {
            logMessage += " with condition: " + guard->getCondition();
        }
        if (!guard->getTargetState().empty()) {
            logMessage += " targeting state: " + guard->getTargetState();
        }

        SCXML::Common::Logger::debug(logMessage);
    }

    // 최상위 데이터 모델 파싱 - 여기서 context 전달
    SCXML::Common::Logger::debug("DocumentParser::parseScxmlNode() - Parsing root datamodel");
    auto datamodelNode = SCXML::Parsing::ParsingCommon::findFirstChildElement(scxmlNode, "datamodel");
    if (datamodelNode) {
        auto dataItems = dataModelParser_->parseDataModelNode(datamodelNode, context);
        for (const auto &item : dataItems) {
            model->addDataModelItem(item);
            SCXML::Common::Logger::debug("DocumentParser::parseScxmlNode() - Added data model item: " + item->getId());
        }
    }

    addSystemVariables(model);

    // 상태 파싱 (모든 최상위 state, parallel, final 노드를 찾기)
    SCXML::Common::Logger::debug("DocumentParser::parseScxmlNode() - Looking for root state nodes");

    // 모든 타입의 상태 노드 수집
    std::vector<const xmlpp::Element *> rootStateElements;
    auto stateElements = SCXML::Parsing::ParsingCommon::findChildElements(scxmlNode, "state");
    rootStateElements.insert(rootStateElements.end(), stateElements.begin(), stateElements.end());

    auto parallelElements = SCXML::Parsing::ParsingCommon::findChildElements(scxmlNode, "parallel");
    rootStateElements.insert(rootStateElements.end(), parallelElements.begin(), parallelElements.end());

    auto finalElements = SCXML::Parsing::ParsingCommon::findChildElements(scxmlNode, "final");
    rootStateElements.insert(rootStateElements.end(), finalElements.begin(), finalElements.end());

    // 최소한 하나의 상태 노드가 있어야 함
    if (rootStateElements.empty()) {
        addError("No state nodes found in SCXML document");
        return false;
    }

    SCXML::Common::Logger::info("DocumentParser::parseScxmlNode() - Found " + std::to_string(rootStateElements.size()) +
                                " root state nodes");

    // 모든 루트 상태 노드 파싱
    for (auto *stateElement : rootStateElements) {
        SCXML::Common::Logger::info("DocumentParser::parseScxmlNode() - Parsing root state");
        auto state = stateNodeParser_->parseStateNode(stateElement, nullptr);
        if (state) {
            model->addState(state);

            // 첫 번째 상태를 루트 상태로 설정 (아직 설정되지 않은 경우)
            if (!model->getRootState()) {
                model->setRootState(state);
            }

            SCXML::Common::Logger::info("DocumentParser::parseScxmlNode() - Root state parsed: " + state->getId());
        } else {
            addError("Failed to parse a root state");
            return false;
        }
    }

    // Parse document-level transitions (transitions at root scxml level)
    SCXML::Common::Logger::debug("DocumentParser::parseScxmlNode() - Parsing document-level transitions");
    auto docTransitionElements = SCXML::Parsing::ParsingCommon::findChildElements(scxmlNode, "transition");
    SCXML::Common::Logger::debug("DocumentParser::parseScxmlNode() - Found " +
                                 std::to_string(docTransitionElements.size()) + " document-level transitions");

    for (auto *transElement : docTransitionElements) {
        // Parse document-level transition (no source state - represents document state)
        auto transition = transitionParser_->parseTransitionNode(transElement, nullptr);
        if (transition) {
            model->addDocumentTransition(transition);
            SCXML::Common::Logger::debug("DocumentParser::parseScxmlNode() - Added document-level transition: " +
                                         (!transition->getEvents().empty() ? transition->getEvents()[0] : "eventless"));
        } else {
            addWarning("Failed to parse document-level transition");
        }
    }

    return true;
}

void DocumentParser::parseContextProperties(const xmlpp::Element *scxmlNode,
                                            std::shared_ptr<::SCXML::Model::DocumentModel> model) {
    if (!scxmlNode || !model) {
        return;
    }

    SCXML::Common::Logger::debug("DocumentParser::parseContextProperties() - Parsing context properties");

    // 1. 직접 ctx:property 찾기
    auto ctxProps = SCXML::Parsing::ParsingCommon::findChildElements(scxmlNode, "property");

    for (auto *propElement : ctxProps) {
        auto nameAttr = propElement->get_attribute("name");
        auto typeAttr = propElement->get_attribute("type");

        if (nameAttr && typeAttr) {
            std::string name = nameAttr->get_value();
            std::string type = typeAttr->get_value();
            model->addContextProperty(name, type);
            SCXML::Common::Logger::debug("DocumentParser::parseContextProperties() - Added property: " + name + " (" +
                                         type + ")");
        } else {
            addWarning("Property node missing required attributes");
        }
    }

    SCXML::Common::Logger::debug("DocumentParser::parseContextProperties() - Found " +
                                 std::to_string(model->getContextProperties().size()) + " context properties");
}

void DocumentParser::parseInjectPoints(const xmlpp::Element *scxmlNode,
                                       std::shared_ptr<::SCXML::Model::DocumentModel> model) {
    if (!scxmlNode || !model) {
        return;
    }

    SCXML::Common::Logger::debug("DocumentParser::parseInjectPoints() - Parsing injection points");

    // 의존성 주입 지점 파싱 (여러 가능한 이름으로 시도)
    std::vector<std::string> injectNodeNames = {"inject-point", "inject_point", "injectpoint", "inject", "dependency"};

    bool foundInjectPoints = false;
    for (const auto &nodeName : injectNodeNames) {
        auto injectElements = SCXML::Parsing::ParsingCommon::findChildElements(scxmlNode, nodeName);

        for (auto *injectElement : injectElements) {
            auto nameAttr = injectElement->get_attribute("name");
            if (!nameAttr) {
                nameAttr = injectElement->get_attribute("id");
            }

            auto typeAttr = injectElement->get_attribute("type");
            if (!typeAttr) {
                typeAttr = injectElement->get_attribute("class");
            }

            if (nameAttr && typeAttr) {
                std::string name = nameAttr->get_value();
                std::string type = typeAttr->get_value();
                model->addInjectPoint(name, type);
                SCXML::Common::Logger::debug("DocumentParser::parseInjectPoints() - Added inject point: " + name +
                                             " (" + type + ")");
                foundInjectPoints = true;
            } else {
                addWarning("Inject point node missing required attributes");
            }
        }

        if (foundInjectPoints) {
            break;
        }
    }

    // 상태 노드 내부의 주입 지점도 확인 (선택적으로 구현 가능)

    SCXML::Common::Logger::debug("DocumentParser::parseInjectPoints() - Found " +
                                 std::to_string(model->getInjectPoints().size()) + " injection points");
}

bool DocumentParser::hasErrors() const {
    return !errorMessages_.empty();
}

const std::vector<std::string> &DocumentParser::getErrorMessages() const {
    return errorMessages_;
}

const std::vector<std::string> &DocumentParser::getWarningMessages() const {
    return warningMessages_;
}

void DocumentParser::setXSDValidationEnabled(bool enabled) {
    xsdValidationEnabled_ = enabled;
    if (enabled && (!xsdValidator_ || !xsdValidator_->isInitialized())) {
        SCXML::Common::Logger::warning("XSD validation requested but validator not available");
        xsdValidationEnabled_ = false;
    }
}

bool DocumentParser::isXSDValidationEnabled() const {
    return xsdValidationEnabled_ && xsdValidator_ && xsdValidator_->isInitialized();
}

void DocumentParser::initParsing() {
    errorMessages_.clear();
    warningMessages_.clear();
}

void DocumentParser::addError(const std::string &message) {
    SCXML::Common::Logger::error("DocumentParser - " + message);
    errorMessages_.push_back(message);
}

void DocumentParser::addWarning(const std::string &message) {
    SCXML::Common::Logger::warning("DocumentParser - " + message);
    warningMessages_.push_back(message);
}

bool DocumentParser::validateModel(std::shared_ptr<::SCXML::Model::DocumentModel> model) {
    if (!model) {
        addError("Null model in validation");
        return false;
    }

    SCXML::Common::Logger::info("DocumentParser::validateModel() - Validating SCXML model");

    bool isValid = true;  // 전체 유효성 검사 결과

    // 1. 루트 상태 확인
    if (!model->getRootState()) {
        addError("Model has no root state");
        return false;
    }

    // 2. 초기 상태 검증
    if (!model->getInitialState().empty()) {
        if (!model->findStateById(model->getInitialState())) {
            addError("Initial state '" + model->getInitialState() + "' not found");
            isValid = false;  // 오류를 기록하고 계속 진행
        }
    }

    // Helper function to recursively collect all states
    std::function<void(std::shared_ptr<SCXML::Model::IStateNode>,
                       std::vector<std::shared_ptr<SCXML::Model::IStateNode>> &)>
        collectAllStates;
    collectAllStates = [&](std::shared_ptr<SCXML::Model::IStateNode> state,
                           std::vector<std::shared_ptr<SCXML::Model::IStateNode>> &allStates) {
        allStates.push_back(state);
        for (const auto &child : state->getChildren()) {
            collectAllStates(child, allStates);
        }
    };

    // Collect all states recursively (including children)
    std::vector<std::shared_ptr<SCXML::Model::IStateNode>> allStatesRecursive;
    for (const auto &state : model->getAllStates()) {
        collectAllStates(state, allStatesRecursive);
    }

    // 3. 상태 관계 검증
    for (const auto &state : allStatesRecursive) {
        // 부모 상태 검증
        auto parent = state->getParent();
        if (parent) {
            bool isChild = false;
            for (const auto &child : parent->getChildren()) {
                if (child.get() == state.get()) {
                    isChild = true;
                    break;
                }
            }

            if (!isChild) {
                addError("State '" + state->getId() + "' has parent '" + parent->getId() +
                         "' but is not in parent's children list");
                isValid = false;  // 오류를 기록하고 계속 진행
            }
        }

        // 전환 대상 상태 검증 - use collected states for validation
        for (const auto &transition : state->getTransitions()) {
            const auto &targets = transition->getTargets();
            for (const auto &target : targets) {
                if (!target.empty()) {
                    // Check if target exists - use findStateById which supports dotted notation
                    bool targetFound = false;

                    // Handle relative path targets like "../finalizing"
                    if (target.substr(0, 3) == "../") {
                        std::string relativeTarget = target.substr(3);  // Remove "../"
                        // Find parent of current state
                        auto currentParent = state->getParent();
                        if (currentParent) {
                            // Look for sibling of parent (i.e., sibling of current state's parent)
                            auto parentOfParent = currentParent->getParent();
                            if (parentOfParent) {
                                // Search in parent's parent children
                                for (const auto &sibling : parentOfParent->getChildren()) {
                                    if (sibling->getId() == relativeTarget) {
                                        targetFound = true;
                                        break;
                                    }
                                }
                            } else {
                                // Parent's parent is root, search in all root states
                                for (const auto &candidateState : allStatesRecursive) {
                                    if (candidateState->getId() == relativeTarget && !candidateState->getParent()) {
                                        targetFound = true;
                                        break;
                                    }
                                }
                            }
                        }
                    } else {
                        // First check direct ID match in collected states
                        for (const auto &candidateState : allStatesRecursive) {
                            if (candidateState->getId() == target) {
                                targetFound = true;
                                break;
                            }
                        }
                    }

                    // If not found and target contains dots, try dotted notation resolution
                    if (!targetFound && target.find('.') != std::string::npos) {
                        // For dotted notation like "player.stopped", we need to check hierarchically
                        size_t dotPos = target.find('.');
                        std::string parentId = target.substr(0, dotPos);
                        std::string childPath = target.substr(dotPos + 1);

                        // Find parent state
                        for (const auto &parentCandidate : allStatesRecursive) {
                            if (parentCandidate->getId() == parentId) {
                                // Check if child exists within parent
                                std::function<bool(const std::shared_ptr<SCXML::Model::IStateNode> &,
                                                   const std::string &)>
                                    findInHierarchy;
                                findInHierarchy = [&](const std::shared_ptr<SCXML::Model::IStateNode> &parent,
                                                      const std::string &childPath) -> bool {
                                    size_t nextDot = childPath.find('.');
                                    if (nextDot == std::string::npos) {
                                        // Direct child
                                        for (const auto &child : parent->getChildren()) {
                                            if (child->getId() == childPath) {
                                                return true;
                                            }
                                        }
                                        return false;
                                    } else {
                                        // Nested child
                                        std::string immediateChild = childPath.substr(0, nextDot);
                                        std::string remaining = childPath.substr(nextDot + 1);
                                        for (const auto &child : parent->getChildren()) {
                                            if (child->getId() == immediateChild) {
                                                return findInHierarchy(child, remaining);
                                            }
                                        }
                                        return false;
                                    }
                                };

                                if (findInHierarchy(parentCandidate, childPath)) {
                                    targetFound = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (!targetFound) {
                        addError("Transition in state '" + state->getId() + "' references non-existent target state '" +
                                 target + "'");
                        isValid = false;  // 오류를 기록하고 계속 진행
                    }
                }
            }
        }

        // 초기 상태 검증
        if (!state->getInitialState().empty() && state->getChildren().size() > 0) {
            bool initialStateFound = false;
            for (const auto &child : state->getChildren()) {
                if (child->getId() == state->getInitialState()) {
                    initialStateFound = true;
                    break;
                }
            }

            if (!initialStateFound) {
                addError("State '" + state->getId() + "' references non-existent initial state '" +
                         state->getInitialState() + "'");
                isValid = false;  // 오류를 기록하고 계속 진행
            }
        }

        // W3C SCXML Compliance: History state validation
        if (state->getType() == Type::HISTORY) {
            // History states MUST have exactly one default transition (W3C SCXML Section 3.10)
            if (state->getTransitions().empty()) {
                addError("History state '" + state->getId() +
                         "' must have exactly one default transition (W3C SCXML Section 3.10)");
                isValid = false;
            } else if (state->getTransitions().size() > 1) {
                addError("History state '" + state->getId() + "' must have exactly one default transition, found " +
                         std::to_string(state->getTransitions().size()) + " transitions");
                isValid = false;
            } else {
                // Validate that the single transition is a proper default transition
                const auto &transition = state->getTransitions()[0];
                if (!transition->getEvent().empty()) {
                    addError("History state '" + state->getId() +
                             "' default transition must not have an event attribute");
                    isValid = false;
                }
                if (!transition->getGuard().empty()) {
                    addError("History state '" + state->getId() +
                             "' default transition must not have a condition attribute");
                    isValid = false;
                }
                if (transition->getTargets().empty()) {
                    addError("History state '" + state->getId() +
                             "' default transition must specify a non-null target");
                    isValid = false;
                }
            }
        }
    }

    // 4. 가드 검증
    for (const auto &guard : model->getGuards()) {
        // getTarget() -> getTargetState() 로 변경
        if (!SCXML::Common::GuardUtils::isConditionExpression(guard->getTargetState()) &&
            !model->findStateById(guard->getTargetState())) {
            addWarning("Guard '" + guard->getId() + "' references non-existent target state '" +
                       guard->getTargetState() + "'");
            // 경고만 생성하고 계속 진행
        }
    }

    if (isValid) {
        SCXML::Common::Logger::info("DocumentParser::validateModel() - Model validation successful");
    } else {
        SCXML::Common::Logger::info("DocumentParser::validateModel() - Model validation completed with errors");
    }

    return isValid;
}

void DocumentParser::addSystemVariables(std::shared_ptr<::SCXML::Model::DocumentModel> model) {
    if (!model) {
        SCXML::Common::Logger::warning("DocumentParser::addSystemVariables() - Null model");
        return;
    }

    SCXML::Common::Logger::debug("DocumentParser::addSystemVariables() - Adding system variables to data model");

    std::string datamodelType = model->getDatamodel();
    // 시스템 변수가 정의된 데이터 모델에만 적용
    if (datamodelType.empty() || datamodelType == "null") {
        SCXML::Common::Logger::debug(
            "DocumentParser::addSystemVariables() - Skipping system variables for null datamodel");
        return;
    }

    // _name 시스템 변수 추가
    auto nameItem = nodeFactory_->createDataModelItem("_name", datamodelType);
    nameItem->setType(datamodelType);
    if (datamodelType == "ecmascript") {
        nameItem->setExpr("''");
    } else if (datamodelType == "xpath") {
        nameItem->setContent("''");
    }
    model->addSystemVariable(nameItem);
    SCXML::Common::Logger::debug("DocumentParser::addSystemVariables() - Added system variable: _name");

    // _sessionid 시스템 변수 추가
    auto sessionIdItem = nodeFactory_->createDataModelItem("_sessionid", datamodelType);
    sessionIdItem->setType(datamodelType);
    if (datamodelType == "ecmascript") {
        sessionIdItem->setExpr("''");
    } else if (datamodelType == "xpath") {
        sessionIdItem->setContent("''");
    }
    model->addSystemVariable(sessionIdItem);
    SCXML::Common::Logger::debug("DocumentParser::addSystemVariables() - Added system variable: _sessionid");

    // _ioprocessors 시스템 변수 추가
    auto ioProcessorsItem = nodeFactory_->createDataModelItem("_ioprocessors", datamodelType);
    ioProcessorsItem->setType(datamodelType);
    if (datamodelType == "ecmascript") {
        ioProcessorsItem->setExpr("{}");
    } else if (datamodelType == "xpath") {
        ioProcessorsItem->setContent("<ioprocessors/>");
    }
    model->addSystemVariable(ioProcessorsItem);
    SCXML::Common::Logger::debug("DocumentParser::addSystemVariables() - Added system variable: _ioprocessors");

    // _event 시스템 변수 추가
    auto eventItem = nodeFactory_->createDataModelItem("_event", datamodelType);
    eventItem->setType(datamodelType);
    if (datamodelType == "ecmascript") {
        eventItem->setExpr("{ name: '' }");
    } else if (datamodelType == "xpath") {
        eventItem->setContent("<event name=\"\"/>");
    }
    model->addSystemVariable(eventItem);
    SCXML::Common::Logger::debug("DocumentParser::addSystemVariables() - Added system variable: _event");
}
