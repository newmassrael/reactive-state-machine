#include "parsing/SCXMLParser.h"
#include "GuardUtils.h"
#include "common/Logger.h"
#include "parsing/ParsingCommon.h"
#include <algorithm>
#include <filesystem>

RSM::SCXMLParser::SCXMLParser(std::shared_ptr<RSM::NodeFactory> nodeFactory,
                              std::shared_ptr<RSM::IXIncludeProcessor> xincludeProcessor)
    : nodeFactory_(nodeFactory) {
    LOG_DEBUG("Creating SCXML parser");

    // 전문화된 파서 초기화
    stateNodeParser_ = std::make_shared<RSM::StateNodeParser>(nodeFactory_);
    transitionParser_ = std::make_shared<RSM::TransitionParser>(nodeFactory_);
    actionParser_ = std::make_shared<RSM::ActionParser>(nodeFactory_);
    guardParser_ = std::make_shared<RSM::GuardParser>(nodeFactory_);
    dataModelParser_ = std::make_shared<RSM::DataModelParser>(nodeFactory_);
    invokeParser_ = std::make_shared<RSM::InvokeParser>(nodeFactory_);
    doneDataParser_ = std::make_shared<RSM::DoneDataParser>(nodeFactory_);

    // 관련 파서들을 연결
    stateNodeParser_->setRelatedParsers(transitionParser_, actionParser_, dataModelParser_, invokeParser_,
                                        doneDataParser_);

    // TransitionParser에 ActionParser 설정
    transitionParser_->setActionParser(actionParser_);

    // XInclude 프로세서 설정
    if (xincludeProcessor) {
        xincludeProcessor_ = xincludeProcessor;
    } else {
        xincludeProcessor_ = std::make_shared<RSM::XIncludeProcessor>();
    }
}

RSM::SCXMLParser::~SCXMLParser() {
    LOG_DEBUG("Destroying SCXML parser");
}

std::shared_ptr<RSM::SCXMLModel> RSM::SCXMLParser::parseFile(const std::string &filename) {
    try {
        // 파싱 상태 초기화
        initParsing();

        // 파일이 존재하는지 확인
        if (!std::filesystem::exists(filename)) {
            addError("File not found: " + filename);
            return nullptr;
        }

        LOG_INFO("Parsing SCXML file: {}", filename);

        // 파일 파싱
        xmlpp::DomParser parser;
        parser.set_validate(false);
        parser.set_substitute_entities(true);  // 엔티티 대체 활성화
        parser.parse_file(filename);

        // XInclude 처리
        LOG_DEBUG("Processing XIncludes");
        xincludeProcessor_->process(parser.get_document());

        // 문서 파싱
        return parseDocument(parser.get_document());
    } catch (const std::exception &ex) {
        addError("Exception while parsing file: " + std::string(ex.what()));
        return nullptr;
    }
}

std::shared_ptr<RSM::SCXMLModel> RSM::SCXMLParser::parseContent(const std::string &content) {
    try {
        // 파싱 상태 초기화
        initParsing();

        LOG_INFO("Parsing SCXML content");

        // 문자열에서 파싱
        xmlpp::DomParser parser;
        parser.set_validate(false);
        parser.set_substitute_entities(true);

        // XML 네임스페이스 인식 활성화
        parser.set_throw_messages(true);

        parser.parse_memory(content);

        // XInclude 처리
        LOG_DEBUG("Processing XIncludes");
        xincludeProcessor_->process(parser.get_document());

        // 문서 파싱
        return parseDocument(parser.get_document());
    } catch (const std::exception &ex) {
        addError("Exception while parsing content: " + std::string(ex.what()));
        return nullptr;
    }
}

std::shared_ptr<RSM::SCXMLModel> RSM::SCXMLParser::parseDocument(xmlpp::Document *doc) {
    if (!doc) {
        addError("Null document");
        return nullptr;
    }

    // 루트 요소 가져오기
    xmlpp::Element *rootElement = doc->get_root_node();
    if (!rootElement) {
        addError("No root element found");
        return nullptr;
    }

    // 루트 요소가 scxml인지 확인
    if (!ParsingCommon::matchNodeName(rootElement->get_name(), "scxml")) {
        addError("Root element is not 'scxml', found: " + rootElement->get_name());
        return nullptr;
    }

    LOG_INFO("Valid SCXML document "
             "found, parsing structure");

    // SCXML 모델 생성
    auto model = std::make_shared<SCXMLModel>();

    // SCXML 노드 파싱
    bool result = parseScxmlNode(rootElement, model);
    if (result) {
        LOG_INFO("SCXML document parsed "
                 "successfully");

        // 모델 검증
        if (validateModel(model)) {
            return model;
        } else {
            LOG_ERROR("SCXML model validation failed");
            return nullptr;
        }
    } else {
        LOG_ERROR("Failed to parse SCXML document");
        return nullptr;
    }
}

bool RSM::SCXMLParser::parseScxmlNode(const xmlpp::Element *scxmlNode, std::shared_ptr<SCXMLModel> model) {
    if (!scxmlNode || !model) {
        addError("Null scxml node or model");
        return false;
    }

    LOG_DEBUG("Parsing SCXML root node");

    // SCXMLContext 생성 및 초기화
    SCXMLContext context;

    // 기본 속성 파싱
    auto nameAttr = scxmlNode->get_attribute("name");
    if (nameAttr) {
        model->setName(nameAttr->get_value());
        LOG_DEBUG("Name: {}", nameAttr->get_value());
    }

    auto initialAttr = scxmlNode->get_attribute("initial");
    if (initialAttr) {
        model->setInitialState(initialAttr->get_value());
        LOG_DEBUG("Initial state: {}", initialAttr->get_value());
    }

    auto datamodelAttr = scxmlNode->get_attribute("datamodel");
    if (datamodelAttr) {
        std::string datamodelType = datamodelAttr->get_value();
        model->setDatamodel(datamodelType);
        context.setDatamodelType(datamodelType);  // SCXMLContext에 설정
        LOG_DEBUG("Datamodel: {}", datamodelType);
    }

    auto bindingAttr = scxmlNode->get_attribute("binding");
    if (bindingAttr) {
        std::string binding = bindingAttr->get_value();
        model->setBinding(binding);
        context.setBinding(binding);  // SCXMLContext에 설정
        LOG_DEBUG("Binding mode: {}", binding);
    }

    // 컨텍스트 속성 파싱
    parseContextProperties(scxmlNode, model);

    // 의존성 주입 지점 파싱
    parseInjectPoints(scxmlNode, model);

    // 가드 조건 파싱
    LOG_DEBUG("Parsing guards");
    auto guards = guardParser_->parseAllGuards(scxmlNode);
    for (const auto &guard : guards) {
        model->addGuard(guard);

        // Build log message with conditional parts
        if (!guard->getCondition().empty() && !guard->getTargetState().empty()) {
            LOG_DEBUG("Added guard: {} with condition: {} targeting state: {}", guard->getId(), guard->getCondition(),
                      guard->getTargetState());
        } else if (!guard->getCondition().empty()) {
            LOG_DEBUG("Added guard: {} with condition: {}", guard->getId(), guard->getCondition());
        } else if (!guard->getTargetState().empty()) {
            LOG_DEBUG("Added guard: {} targeting state: {}", guard->getId(), guard->getTargetState());
        } else {
            LOG_DEBUG("Added guard: {}", guard->getId());
        }
    }

    // 최상위 데이터 모델 파싱 - 여기서 context 전달
    LOG_DEBUG("Parsing root datamodel");
    auto datamodelNode = ParsingCommon::findFirstChildElement(scxmlNode, "datamodel");
    if (datamodelNode) {
        auto dataItems = dataModelParser_->parseDataModelNode(datamodelNode, context);
        for (const auto &item : dataItems) {
            model->addDataModelItem(item);
            LOG_DEBUG("Added data model item: {}", item->getId());
        }
    }

    addSystemVariables(model);

    // W3C SCXML 5.8: Parse top-level <script> elements (children of <scxml>)
    auto scriptElements = ParsingCommon::findChildElements(scxmlNode, "script");
    if (!scriptElements.empty()) {
        LOG_DEBUG("Parsing {} root script element(s) (W3C SCXML 5.8)", scriptElements.size());
        size_t parsedCount = 0;

        for (size_t i = 0; i < scriptElements.size(); ++i) {
            auto scriptAction = actionParser_->parseActionNode(scriptElements[i]);
            if (scriptAction) {
                model->addTopLevelScript(scriptAction);
                parsedCount++;
                LOG_DEBUG("Added top-level script #{} for document load time execution (W3C SCXML 5.8)", i + 1);
            } else {
                LOG_WARN("Failed to parse top-level script element #{} - skipping (W3C SCXML 5.8)", i + 1);
            }
        }

        LOG_DEBUG("Successfully parsed {}/{} top-level script(s) (W3C SCXML 5.8)", parsedCount, scriptElements.size());
    }

    // 상태 파싱 (모든 최상위 state, parallel, final 노드를 찾기)
    LOG_DEBUG("Looking for root state nodes");

    // 모든 타입의 상태 노드 수집
    std::vector<const xmlpp::Element *> rootStateElements;
    auto stateElements = ParsingCommon::findChildElements(scxmlNode, "state");
    rootStateElements.insert(rootStateElements.end(), stateElements.begin(), stateElements.end());

    auto parallelElements = ParsingCommon::findChildElements(scxmlNode, "parallel");
    rootStateElements.insert(rootStateElements.end(), parallelElements.begin(), parallelElements.end());

    auto finalElements = ParsingCommon::findChildElements(scxmlNode, "final");
    rootStateElements.insert(rootStateElements.end(), finalElements.begin(), finalElements.end());

    // 최소한 하나의 상태 노드가 있어야 함
    if (rootStateElements.empty()) {
        addError("No state nodes found in SCXML document");
        return false;
    }

    LOG_INFO("Found {} root state nodes", rootStateElements.size());

    // 모든 루트 상태 노드 파싱
    for (auto *stateElement : rootStateElements) {
        LOG_INFO("Parsing root state");
        auto state = stateNodeParser_->parseStateNode(stateElement, nullptr);
        if (state) {
            model->addState(state);

            // 첫 번째 상태를 루트 상태로 설정 (아직 설정되지 않은 경우)
            if (!model->getRootState()) {
                model->setRootState(state);
            }

            LOG_INFO("Root state parsed: {}", state->getId());
        } else {
            addError("Failed to parse a root state");
            return false;
        }
    }

    return true;
}

void RSM::SCXMLParser::parseContextProperties(const xmlpp::Element *scxmlNode, std::shared_ptr<SCXMLModel> model) {
    if (!scxmlNode || !model) {
        return;
    }

    LOG_DEBUG("Parsing context "
              "properties");

    // 1. 직접 ctx:property 찾기
    auto ctxProps = ParsingCommon::findChildElements(scxmlNode, "property");

    for (auto *propElement : ctxProps) {
        auto nameAttr = propElement->get_attribute("name");
        auto typeAttr = propElement->get_attribute("type");

        if (nameAttr && typeAttr) {
            std::string name = nameAttr->get_value();
            std::string type = typeAttr->get_value();
            model->addContextProperty(name, type);
            LOG_DEBUG("Added property: {} ({})", name, type);
        } else {
            addWarning("Property node missing required attributes");
        }
    }

    LOG_DEBUG("Found {} context properties", model->getContextProperties().size());
}

void RSM::SCXMLParser::parseInjectPoints(const xmlpp::Element *scxmlNode, std::shared_ptr<SCXMLModel> model) {
    if (!scxmlNode || !model) {
        return;
    }

    LOG_DEBUG("Parsing injection points");

    // 의존성 주입 지점 파싱 (여러 가능한 이름으로 시도)
    std::vector<std::string> injectNodeNames = {"inject-point", "inject_point", "injectpoint", "inject", "dependency"};

    bool foundInjectPoints = false;
    for (const auto &nodeName : injectNodeNames) {
        auto injectElements = ParsingCommon::findChildElements(scxmlNode, nodeName);

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
                LOG_DEBUG("Added inject point: {} ({})", name, type);
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

    LOG_DEBUG("Found {} injection points", model->getInjectPoints().size());
}

bool RSM::SCXMLParser::hasErrors() const {
    return !errorMessages_.empty();
}

const std::vector<std::string> &RSM::SCXMLParser::getErrorMessages() const {
    return errorMessages_;
}

const std::vector<std::string> &RSM::SCXMLParser::getWarningMessages() const {
    return warningMessages_;
}

void RSM::SCXMLParser::initParsing() {
    errorMessages_.clear();
    warningMessages_.clear();
}

void RSM::SCXMLParser::addError(const std::string &message) {
    LOG_ERROR("SCXMLParser - {}", message);
    errorMessages_.push_back(message);
}

void RSM::SCXMLParser::addWarning(const std::string &message) {
    LOG_WARN("SCXMLParser - {}", message);
    warningMessages_.push_back(message);
}

bool RSM::SCXMLParser::validateModel(std::shared_ptr<SCXMLModel> model) {
    if (!model) {
        addError("Null model in validation");
        return false;
    }

    LOG_INFO("Validating SCXML model");

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

    // 3. 상태 관계 검증
    for (const auto &state : model->getAllStates()) {
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

        // 전환 대상 상태 검증
        for (const auto &transition : state->getTransitions()) {
            const auto &targets = transition->getTargets();
            for (const auto &target : targets) {
                if (!target.empty() && !model->findStateById(target)) {
                    addError("Transition in state '" + state->getId() + "' references non-existent target state '" +
                             target + "'");
                    isValid = false;  // 오류를 기록하고 계속 진행
                }
            }
        }

        // W3C SCXML 3.3: Validate initial state(s) - supports space-separated list for parallel states
        if (!state->getInitialState().empty() && state->getChildren().size() > 0) {
            // Parse space-separated initial state list
            std::istringstream iss(state->getInitialState());
            std::string initialStateId;
            bool allInitialStatesFound = true;

            while (iss >> initialStateId) {
                // Search in entire model (not just direct children) to support deep initial states
                if (!model->findStateById(initialStateId)) {
                    addError("State '" + state->getId() + "' references non-existent initial state '" + initialStateId +
                             "'");
                    allInitialStatesFound = false;
                }
            }

            if (!allInitialStatesFound) {
                isValid = false;  // Continue validation but mark as invalid
            }
        }
    }

    // 4. 가드 검증
    for (const auto &guard : model->getGuards()) {
        // getTarget() -> getTargetState() 로 변경
        if (!GuardUtils::isConditionExpression(guard->getTargetState()) &&
            !model->findStateById(guard->getTargetState())) {
            addWarning("Guard '" + guard->getId() + "' references non-existent target state '" +
                       guard->getTargetState() + "'");
            // 경고만 생성하고 계속 진행
        }
    }

    if (isValid) {
        LOG_INFO("Model validation successful");
    } else {
        LOG_INFO("Model validation completed with errors");
    }

    return isValid;
}

void RSM::SCXMLParser::addSystemVariables(std::shared_ptr<SCXMLModel> model) {
    if (!model) {
        LOG_WARN("Null model");
        return;
    }

    LOG_DEBUG("Adding system variables to data model");

    std::string datamodelType = model->getDatamodel();
    // 시스템 변수가 정의된 데이터 모델에만 적용
    if (datamodelType.empty() || datamodelType == "null") {
        LOG_DEBUG("Skipping system variables for null datamodel");
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
    LOG_DEBUG("Added system variable: _name");

    // _sessionid 시스템 변수 추가
    auto sessionIdItem = nodeFactory_->createDataModelItem("_sessionid", datamodelType);
    sessionIdItem->setType(datamodelType);
    if (datamodelType == "ecmascript") {
        sessionIdItem->setExpr("''");
    } else if (datamodelType == "xpath") {
        sessionIdItem->setContent("''");
    }
    model->addSystemVariable(sessionIdItem);
    LOG_DEBUG("Added system variable: _sessionid");

    // _ioprocessors 시스템 변수 추가
    auto ioProcessorsItem = nodeFactory_->createDataModelItem("_ioprocessors", datamodelType);
    ioProcessorsItem->setType(datamodelType);
    if (datamodelType == "ecmascript") {
        ioProcessorsItem->setExpr("{}");
    } else if (datamodelType == "xpath") {
        ioProcessorsItem->setContent("<ioprocessors/>");
    }
    model->addSystemVariable(ioProcessorsItem);
    LOG_DEBUG("Added system variable: _ioprocessors");

    // W3C SCXML 5.10: _event is bound lazily on first event, not at initialization
    LOG_DEBUG("Skipping _event initialization per W3C SCXML 5.10 (bound only after first event)");
}
