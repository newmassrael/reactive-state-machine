#include "parsing/SCXMLParser.h"
#include "Logger.h"
#include "parsing/ParsingCommon.h"
#include <filesystem>
#include <algorithm>
#include "GuardUtils.h"

SCXMLParser::SCXMLParser(std::shared_ptr<INodeFactory> nodeFactory,
                         std::shared_ptr<IXIncludeProcessor> xincludeProcessor)
    : nodeFactory_(nodeFactory)
{
    Logger::debug("SCXMLParser::Constructor - Creating SCXML parser");

    // 전문화된 파서 초기화
    stateNodeParser_ = std::make_shared<StateNodeParser>(nodeFactory_);
    transitionParser_ = std::make_shared<TransitionParser>(nodeFactory_);
    actionParser_ = std::make_shared<ActionParser>(nodeFactory_);
    guardParser_ = std::make_shared<GuardParser>(nodeFactory_);
    dataModelParser_ = std::make_shared<DataModelParser>(nodeFactory_);
    invokeParser_ = std::make_shared<InvokeParser>(nodeFactory_);
    doneDataParser_ = std::make_shared<DoneDataParser>(nodeFactory_);

    // 관련 파서들을 연결
    stateNodeParser_->setRelatedParsers(
        transitionParser_,
        actionParser_,
        dataModelParser_,
        invokeParser_,
        doneDataParser_);

    // XInclude 프로세서 설정
    if (xincludeProcessor)
    {
        xincludeProcessor_ = xincludeProcessor;
    }
    else
    {
        xincludeProcessor_ = std::make_shared<XIncludeProcessor>();
    }
}

SCXMLParser::~SCXMLParser()
{
    Logger::debug("SCXMLParser::Destructor - Destroying SCXML parser");
}

std::shared_ptr<SCXMLModel> SCXMLParser::parseFile(const std::string &filename)
{
    try
    {
        // 파싱 상태 초기화
        initParsing();

        // 파일이 존재하는지 확인
        if (!std::filesystem::exists(filename))
        {
            addError("File not found: " + filename);
            return nullptr;
        }

        Logger::info("SCXMLParser::parseFile() - Parsing SCXML file: " + filename);

        // 파일 파싱
        xmlpp::DomParser parser;
        parser.set_validate(false);
        parser.set_substitute_entities(true); // 엔티티 대체 활성화
        parser.parse_file(filename);

        // XInclude 처리
        Logger::debug("SCXMLParser::parseFile() - Processing XIncludes");
        xincludeProcessor_->process(parser.get_document());

        // 문서 파싱
        return parseDocument(parser.get_document());
    }
    catch (const std::exception &ex)
    {
        addError("Exception while parsing file: " + std::string(ex.what()));
        return nullptr;
    }
}

std::shared_ptr<SCXMLModel> SCXMLParser::parseContent(const std::string &content)
{
    try
    {
        // 파싱 상태 초기화
        initParsing();

        Logger::info("SCXMLParser::parseContent() - Parsing SCXML content");

        // 문자열에서 파싱
        xmlpp::DomParser parser;
        parser.set_validate(false);
        parser.set_substitute_entities(true);

        // XML 네임스페이스 인식 활성화
        parser.set_throw_messages(true);

        parser.parse_memory(content);

        // XInclude 처리
        Logger::debug("SCXMLParser::parseContent() - Processing XIncludes");
        xincludeProcessor_->process(parser.get_document());

        // 문서 파싱
        return parseDocument(parser.get_document());
    }
    catch (const std::exception &ex)
    {
        addError("Exception while parsing content: " + std::string(ex.what()));
        return nullptr;
    }
}

std::shared_ptr<SCXMLModel> SCXMLParser::parseDocument(xmlpp::Document *doc)
{
    if (!doc)
    {
        addError("Null document");
        return nullptr;
    }

    // 루트 요소 가져오기
    xmlpp::Element *rootElement = doc->get_root_node();
    if (!rootElement)
    {
        addError("No root element found");
        return nullptr;
    }

    // 루트 요소가 scxml인지 확인
    if (!ParsingCommon::matchNodeName(rootElement->get_name(), "scxml"))
    {
        addError("Root element is not 'scxml', found: " + rootElement->get_name());
        return nullptr;
    }

    Logger::info("SCXMLParser::parseDocument() - Valid SCXML document found, parsing structure");

    // SCXML 모델 생성
    auto model = std::make_shared<SCXMLModel>();

    // SCXML 노드 파싱
    bool result = parseScxmlNode(rootElement, model);
    if (result)
    {
        Logger::info("SCXMLParser::parseDocument() - SCXML document parsed successfully");

        // 모델 검증
        if (validateModel(model))
        {
            return model;
        }
        else
        {
            Logger::error("SCXMLParser::parseDocument() - SCXML model validation failed");
            return nullptr;
        }
    }
    else
    {
        Logger::error("SCXMLParser::parseDocument() - Failed to parse SCXML document");
        return nullptr;
    }
}

bool SCXMLParser::parseScxmlNode(const xmlpp::Element *scxmlNode, std::shared_ptr<SCXMLModel> model)
{
    if (!scxmlNode || !model)
    {
        addError("Null scxml node or model");
        return false;
    }

    Logger::debug("SCXMLParser::parseScxmlNode() - Parsing SCXML root node");

    // SCXMLContext 생성 및 초기화
    SCXMLContext context;

    // 기본 속성 파싱
    auto nameAttr = scxmlNode->get_attribute("name");
    if (nameAttr)
    {
        model->setName(nameAttr->get_value());
        Logger::debug("SCXMLParser::parseScxmlNode() - Name: " + nameAttr->get_value());
    }

    auto initialAttr = scxmlNode->get_attribute("initial");
    if (initialAttr)
    {
        model->setInitialState(initialAttr->get_value());
        Logger::debug("SCXMLParser::parseScxmlNode() - Initial state: " + initialAttr->get_value());
    }

    auto datamodelAttr = scxmlNode->get_attribute("datamodel");
    if (datamodelAttr)
    {
        std::string datamodelType = datamodelAttr->get_value();
        model->setDatamodel(datamodelType);
        context.setDatamodelType(datamodelType); // SCXMLContext에 설정
        Logger::debug("SCXMLParser::parseScxmlNode() - Datamodel: " + datamodelType);
    }

    auto bindingAttr = scxmlNode->get_attribute("binding");
    if (bindingAttr)
    {
        std::string binding = bindingAttr->get_value();
        model->setBinding(binding);
        context.setBinding(binding); // SCXMLContext에 설정
        Logger::debug("SCXMLParser::parseScxmlNode() - Binding mode: " + binding);
    }

    // 컨텍스트 속성 파싱
    parseContextProperties(scxmlNode, model);

    // 의존성 주입 지점 파싱
    parseInjectPoints(scxmlNode, model);

    // 가드 조건 파싱
    Logger::debug("SCXMLParser::parseScxmlNode() - Parsing guards");
    auto guards = guardParser_->parseAllGuards(scxmlNode);
    for (const auto &guard : guards)
    {
        model->addGuard(guard);

        std::string logMessage = "SCXMLParser::parseScxmlNode() - Added guard: " + guard->getId();
        if (!guard->getCondition().empty())
        {
            logMessage += " with condition: " + guard->getCondition();
        }
        if (!guard->getTargetState().empty())
        {
            logMessage += " targeting state: " + guard->getTargetState();
        }

        Logger::debug(logMessage);
    }

    // 최상위 데이터 모델 파싱 - 여기서 context 전달
    Logger::debug("SCXMLParser::parseScxmlNode() - Parsing root datamodel");
    auto datamodelNode = ParsingCommon::findFirstChildElement(scxmlNode, "datamodel");
    if (datamodelNode)
    {
        auto dataItems = dataModelParser_->parseDataModelNode(datamodelNode, context);
        for (const auto &item : dataItems)
        {
            model->addDataModelItem(item);
            Logger::debug("SCXMLParser::parseScxmlNode() - Added data model item: " + item->getId());
        }
    }

    addSystemVariables(model);

    // 상태 파싱 (모든 최상위 state, parallel, final 노드를 찾기)
    Logger::debug("SCXMLParser::parseScxmlNode() - Looking for root state nodes");

    // 모든 타입의 상태 노드 수집
    std::vector<const xmlpp::Element *> rootStateElements;
    auto stateElements = ParsingCommon::findChildElements(scxmlNode, "state");
    rootStateElements.insert(rootStateElements.end(), stateElements.begin(), stateElements.end());

    auto parallelElements = ParsingCommon::findChildElements(scxmlNode, "parallel");
    rootStateElements.insert(rootStateElements.end(), parallelElements.begin(), parallelElements.end());

    auto finalElements = ParsingCommon::findChildElements(scxmlNode, "final");
    rootStateElements.insert(rootStateElements.end(), finalElements.begin(), finalElements.end());

    // 최소한 하나의 상태 노드가 있어야 함
    if (rootStateElements.empty())
    {
        addError("No state nodes found in SCXML document");
        return false;
    }

    Logger::info("SCXMLParser::parseScxmlNode() - Found " + std::to_string(rootStateElements.size()) + " root state nodes");

    // 모든 루트 상태 노드 파싱
    for (auto *stateElement : rootStateElements)
    {
        Logger::info("SCXMLParser::parseScxmlNode() - Parsing root state");
        auto state = stateNodeParser_->parseStateNode(stateElement, nullptr);
        if (state)
        {
            model->addState(state);

            // 첫 번째 상태를 루트 상태로 설정 (아직 설정되지 않은 경우)
            if (!model->getRootState())
            {
                model->setRootState(state);
            }

            Logger::info("SCXMLParser::parseScxmlNode() - Root state parsed: " + state->getId());
        }
        else
        {
            addError("Failed to parse a root state");
            return false;
        }
    }

    return true;
}

void SCXMLParser::parseContextProperties(const xmlpp::Element *scxmlNode, std::shared_ptr<SCXMLModel> model)
{
    if (!scxmlNode || !model)
    {
        return;
    }

    Logger::debug("SCXMLParser::parseContextProperties() - Parsing context properties");

    // 1. 직접 ctx:property 찾기
    auto ctxProps = ParsingCommon::findChildElements(scxmlNode, "property");

    for (auto *propElement : ctxProps)
    {
        auto nameAttr = propElement->get_attribute("name");
        auto typeAttr = propElement->get_attribute("type");

        if (nameAttr && typeAttr)
        {
            std::string name = nameAttr->get_value();
            std::string type = typeAttr->get_value();
            model->addContextProperty(name, type);
            Logger::debug("SCXMLParser::parseContextProperties() - Added property: " + name + " (" + type + ")");
        }
        else
        {
            addWarning("Property node missing required attributes");
        }
    }

    Logger::debug("SCXMLParser::parseContextProperties() - Found " +
                  std::to_string(model->getContextProperties().size()) + " context properties");
}

void SCXMLParser::parseInjectPoints(const xmlpp::Element *scxmlNode, std::shared_ptr<SCXMLModel> model)
{
    if (!scxmlNode || !model)
    {
        return;
    }

    Logger::debug("SCXMLParser::parseInjectPoints() - Parsing injection points");

    // 의존성 주입 지점 파싱 (여러 가능한 이름으로 시도)
    std::vector<std::string> injectNodeNames = {"inject-point", "inject_point", "injectpoint", "inject", "dependency"};

    bool foundInjectPoints = false;
    for (const auto &nodeName : injectNodeNames)
    {
        auto injectElements = ParsingCommon::findChildElements(scxmlNode, nodeName);

        for (auto *injectElement : injectElements)
        {
            auto nameAttr = injectElement->get_attribute("name");
            if (!nameAttr)
            {
                nameAttr = injectElement->get_attribute("id");
            }

            auto typeAttr = injectElement->get_attribute("type");
            if (!typeAttr)
            {
                typeAttr = injectElement->get_attribute("class");
            }

            if (nameAttr && typeAttr)
            {
                std::string name = nameAttr->get_value();
                std::string type = typeAttr->get_value();
                model->addInjectPoint(name, type);
                Logger::debug("SCXMLParser::parseInjectPoints() - Added inject point: " + name + " (" + type + ")");
                foundInjectPoints = true;
            }
            else
            {
                addWarning("Inject point node missing required attributes");
            }
        }

        if (foundInjectPoints)
        {
            break;
        }
    }

    // 상태 노드 내부의 주입 지점도 확인 (선택적으로 구현 가능)

    Logger::debug("SCXMLParser::parseInjectPoints() - Found " +
                  std::to_string(model->getInjectPoints().size()) + " injection points");
}

bool SCXMLParser::hasErrors() const
{
    return !errorMessages_.empty();
}

const std::vector<std::string> &SCXMLParser::getErrorMessages() const
{
    return errorMessages_;
}

const std::vector<std::string> &SCXMLParser::getWarningMessages() const
{
    return warningMessages_;
}

void SCXMLParser::initParsing()
{
    errorMessages_.clear();
    warningMessages_.clear();
}

void SCXMLParser::addError(const std::string &message)
{
    Logger::error("SCXMLParser - " + message);
    errorMessages_.push_back(message);
}

void SCXMLParser::addWarning(const std::string &message)
{
    Logger::warning("SCXMLParser - " + message);
    warningMessages_.push_back(message);
}

bool SCXMLParser::validateModel(std::shared_ptr<SCXMLModel> model)
{
    if (!model)
    {
        addError("Null model in validation");
        return false;
    }

    Logger::info("SCXMLParser::validateModel() - Validating SCXML model");

    bool isValid = true; // 전체 유효성 검사 결과

    // 1. 루트 상태 확인
    if (!model->getRootState())
    {
        addError("Model has no root state");
        return false;
    }

    // 2. 초기 상태 검증
    if (!model->getInitialState().empty())
    {
        if (!model->findStateById(model->getInitialState()))
        {
            addError("Initial state '" + model->getInitialState() + "' not found");
            isValid = false; // 오류를 기록하고 계속 진행
        }
    }

    // 3. 상태 관계 검증
    for (const auto &state : model->getAllStates())
    {
        // 부모 상태 검증
        auto parent = state->getParent();
        if (parent)
        {
            bool isChild = false;
            for (const auto &child : parent->getChildren())
            {
                if (child.get() == state.get())
                {
                    isChild = true;
                    break;
                }
            }

            if (!isChild)
            {
                addError("State '" + state->getId() + "' has parent '" + parent->getId() +
                         "' but is not in parent's children list");
                isValid = false; // 오류를 기록하고 계속 진행
            }
        }

        // 전환 대상 상태 검증
        for (const auto &transition : state->getTransitions())
        {
            const auto &targets = transition->getTargets();
            for (const auto &target : targets)
            {
                if (!target.empty() && !model->findStateById(target))
                {
                    addError("Transition in state '" + state->getId() +
                             "' references non-existent target state '" + target + "'");
                    isValid = false; // 오류를 기록하고 계속 진행
                }
            }
        }

        // 초기 상태 검증
        if (!state->getInitialState().empty() && state->getChildren().size() > 0)
        {
            bool initialStateFound = false;
            for (const auto &child : state->getChildren())
            {
                if (child->getId() == state->getInitialState())
                {
                    initialStateFound = true;
                    break;
                }
            }

            if (!initialStateFound)
            {
                addError("State '" + state->getId() + "' references non-existent initial state '" +
                         state->getInitialState() + "'");
                isValid = false; // 오류를 기록하고 계속 진행
            }
        }
    }

    // 4. 가드 검증
    for (const auto &guard : model->getGuards())
    {
        // getTarget() -> getTargetState() 로 변경
        if (!GuardUtils::isConditionExpression(guard->getTargetState()) &&
            !model->findStateById(guard->getTargetState()))
        {
            addWarning("Guard '" + guard->getId() + "' references non-existent target state '" +
                       guard->getTargetState() + "'");
            // 경고만 생성하고 계속 진행
        }
    }

    if (isValid)
    {
        Logger::info("SCXMLParser::validateModel() - Model validation successful");
    }
    else
    {
        Logger::info("SCXMLParser::validateModel() - Model validation completed with errors");
    }

    return isValid;
}

void SCXMLParser::addSystemVariables(std::shared_ptr<SCXMLModel> model)
{
    if (!model)
    {
        Logger::warning("SCXMLParser::addSystemVariables() - Null model");
        return;
    }

    Logger::debug("SCXMLParser::addSystemVariables() - Adding system variables to data model");

    std::string datamodelType = model->getDatamodel();
    // 시스템 변수가 정의된 데이터 모델에만 적용
    if (datamodelType.empty() || datamodelType == "null")
    {
        Logger::debug("SCXMLParser::addSystemVariables() - Skipping system variables for null datamodel");
        return;
    }

    // _name 시스템 변수 추가
    auto nameItem = nodeFactory_->createDataModelItem("_name", datamodelType);
    nameItem->setType(datamodelType);
    if (datamodelType == "ecmascript")
    {
        nameItem->setExpr("''");
    }
    else if (datamodelType == "xpath")
    {
        nameItem->setContent("''");
    }
    model->addSystemVariable(nameItem);
    Logger::debug("SCXMLParser::addSystemVariables() - Added system variable: _name");

    // _sessionid 시스템 변수 추가
    auto sessionIdItem = nodeFactory_->createDataModelItem("_sessionid", datamodelType);
    sessionIdItem->setType(datamodelType);
    if (datamodelType == "ecmascript")
    {
        sessionIdItem->setExpr("''");
    }
    else if (datamodelType == "xpath")
    {
        sessionIdItem->setContent("''");
    }
    model->addSystemVariable(sessionIdItem);
    Logger::debug("SCXMLParser::addSystemVariables() - Added system variable: _sessionid");

    // _ioprocessors 시스템 변수 추가
    auto ioProcessorsItem = nodeFactory_->createDataModelItem("_ioprocessors", datamodelType);
    ioProcessorsItem->setType(datamodelType);
    if (datamodelType == "ecmascript")
    {
        ioProcessorsItem->setExpr("{}");
    }
    else if (datamodelType == "xpath")
    {
        ioProcessorsItem->setContent("<ioprocessors/>");
    }
    model->addSystemVariable(ioProcessorsItem);
    Logger::debug("SCXMLParser::addSystemVariables() - Added system variable: _ioprocessors");

    // _event 시스템 변수 추가
    auto eventItem = nodeFactory_->createDataModelItem("_event", datamodelType);
    eventItem->setType(datamodelType);
    if (datamodelType == "ecmascript")
    {
        eventItem->setExpr("{ name: '' }");
    }
    else if (datamodelType == "xpath")
    {
        eventItem->setContent("<event name=\"\"/>");
    }
    model->addSystemVariable(eventItem);
    Logger::debug("SCXMLParser::addSystemVariables() - Added system variable: _event");
}
