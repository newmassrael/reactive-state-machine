#include "runtime/ECMAScriptDataModelEngine.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "model/IDataModelItem.h"
#include "runtime/ECMAScriptDataModelEngine.h"
#include "runtime/ECMAScriptEngineFactory.h"
#include "runtime/RuntimeContext.h"
#include <chrono>
#include <sstream>

namespace SCXML {

ECMAScriptDataModelEngine::ECMAScriptDataModelEngine(ECMAScriptEngineFactory::EngineType engineType)
    : DataModelEngine(DataModelType::ECMASCRIPT), currentEngineType_(engineType) {
    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::Constructor - Creating ECMAScript data model engine");

    // ECMAScript 엔진 초기화
    switchEngine(engineType);

    // SCXML 내장 함수 등록
    registerSCXMLBuiltins();

    // ECMAScript 객체 등록
    registerECMAScriptObjects();

    // 시스템 변수 초기화
    initializeSystemVariables();
}

ECMAScriptDataModelEngine::~ECMAScriptDataModelEngine() {
    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::Destructor - Cleaning up ECMAScript engine");
}

DataModelEngine::DataModelResult ECMAScriptDataModelEngine::initializeFromDataItems(
    const std::vector<std::shared_ptr<SCXML::Model::IDataModelItem>> &dataItems, const std::string &sessionId) {
    (void)sessionId;  // 현재 사용하지 않음

    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::initializeFromDataItems - Initializing " +
                  std::to_string(dataItems.size()) + " data items");

    try {
        for (const auto &item : dataItems) {
            if (!item) {
                continue;
            }

            std::string id = item->getId();
            std::string expr = item->getExpr();
            DataValue value;

            if (!expr.empty()) {
                // 함수 표현식인지 확인 (function 키워드를 포함하고 있는 것)
                if (expr.find("function") != std::string::npos) {
                    // 함수 표현식을 JavaScript 환경에 직접 할당
                    std::string functionScript = "var " + id + " = " + expr + ";";
                    SCXML::Common::Logger::debug(
                        "ECMAScriptDataModelEngine::initializeFromDataItems - Executing function expression: " +
                        functionScript);

                    SCXML::Runtime::RuntimeContext context;
                    auto scriptResult = ecmaEngine_->executeScript(functionScript, context);

                    if (scriptResult.success) {
                        SCXML::Common::Logger::debug(
                            "ECMAScriptDataModelEngine::initializeFromDataItems - Successfully registered function: " +
                            id);
                        value = expr;  // 표현식을 값으로도 저장
                    } else {
                        SCXML::Common::Logger::error(
                            "ECMAScriptDataModelEngine::initializeFromDataItems - Failed to register function '" + id +
                            "': " + scriptResult.errorMessage);
                        value = expr;  // 실패해도 표현식은 저장
                    }
                } else {
                    // 일반 표현식 평가 (오브젝트 리터럴은 괄호로 감쌈)
                    std::string evalExpr = expr;
                    if (expr.front() == '{' && expr.back() == '}') {
                        evalExpr = "(" + expr + ")";  // 오브젝트 리터럴을 괄호로 감쌈
                        SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::initializeFromDataItems - Wrapping object literal in "
                                      "parentheses: " +
                                      evalExpr);
                    }
                    // ECMAScript 엔진으로 직접 평가 (무한 루프 방지)
                    SCXML::Runtime::RuntimeContext tempContext;
                    auto result = ecmaEngine_->evaluateExpression(evalExpr, tempContext);
                    DataModelResult dataResult;
                    if (result.success) {
                        dataResult = DataModelResult::createSuccess(ecmaValueToDataValue(result.value));
                    } else {
                        dataResult = DataModelResult::createError(result.errorMessage);
                    }
                    if (dataResult.success) {
                        value = dataResult.value;
                    } else {
                        SCXML::Common::Logger::warning("ECMAScriptDataModelEngine::initializeFromDataItems - " +
                                        std::string("Failed to evaluate expression for '") + id + "': " + expr);
                        // 기본값으로 표현식 문자열 사용
                        value = expr;
                    }
                }
            } else {
                // 기본값 사용
                value = std::string("");
            }

            // 변수를 DataModelEngine에 설정
            auto setResult = DataModelEngine::setValue(id, value, Scope::GLOBAL);
            if (!setResult.success) {
                SCXML::Common::Logger::error("ECMAScriptDataModelEngine::initializeFromDataItems - " +
                              std::string("Failed to set variable '") + id + "'");
                return DataModelResult::createError("Failed to set variable '" + id + "'");
            }

            // ECMAScript 엔진에도 변수 등록 (함수가 아닌 경우에만)
            if (expr.find("function") == std::string::npos) {
                ECMAValue ecmaValue = dataValueToECMAValue(value);
                bool jsSetResult = ecmaEngine_->setVariable(id, ecmaValue);
                if (jsSetResult) {
                    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::initializeFromDataItems - Successfully set variable '" +
                                  id + "' in JavaScript engine");
                } else {
                    SCXML::Common::Logger::warning("ECMAScriptDataModelEngine::initializeFromDataItems - Failed to set variable '" +
                                    id + "' in JavaScript engine");
                }
            }
        }

        SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::initializeFromDataItems - Successfully initialized all data items");
        return DataModelResult::createSuccess();

    } catch (const std::exception &e) {
        std::string error = "ECMAScriptDataModelEngine::initializeFromDataItems - Exception: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        return DataModelResult::createError(error);
    }
}

DataModelEngine::DataModelResult ECMAScriptDataModelEngine::setValue(const std::string &location,
                                                                     const DataValue &value, Scope scope) {
    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::setValue - Setting '" + location + "'");

    if (!ecmaEngine_) {
        return DataModelResult::createError("JavaScript engine not initialized");
    }

    try {
        // ECMAScript 엔진에서 변수 설정
        bool success = ecmaEngine_->setVariable(location, dataValueToECMAValue(value));

        if (success) {
            // DataModelEngine의 내부 저장소에도 저장
            DataModelEngine::setValue(location, value, scope);
            SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::setValue - Successfully set '" + location + "'");
            return DataModelResult::createSuccess();
        }

        return DataModelResult::createError("Failed to set variable: " + location);

    } catch (const std::exception &e) {
        std::string error = "ECMAScriptDataModelEngine::setValue - Exception: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        return DataModelResult::createError(error);
    }
}

DataModelEngine::DataModelResult ECMAScriptDataModelEngine::getValue(const std::string &location,
                                                                     std::optional<Scope> scope) const {
    (void)scope;  // 현재 사용하지 않음

    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::getValue - Getting '" + location + "'");

    if (!ecmaEngine_) {
        return DataModelResult::createError("JavaScript engine not initialized");
    }

    try {
        // ECMAScript 엔진에서 변수 값 조회
        auto result = ecmaEngine_->getVariable(location);

        if (result.success) {
            // ECMAValue를 DataValue로 변환
            DataValue value = ecmaValueToDataValue(result.value);
            SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::getValue - Successfully got '" + location + "'");
            return DataModelResult::createSuccess(value);
        }

        return DataModelResult::createError(result.errorMessage);

    } catch (const std::exception &e) {
        std::string error = "ECMAScriptDataModelEngine::getValue - Exception: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        return DataModelResult::createError(error);
    }
}

DataModelEngine::DataModelResult ECMAScriptDataModelEngine::evaluateExpression(const std::string &expression,
                                                                               Scope scope) const {
    (void)scope;  // 현재 사용하지 않음

    auto startTime = std::chrono::high_resolution_clock::now();
    expressionCount_++;

    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::evaluateExpression - Evaluating: " + expression);

    if (!ecmaEngine_) {
        return DataModelResult::createError("JavaScript engine not initialized");
    }

    try {
        // ECMAScript 표현식 실행
        SCXML::Runtime::RuntimeContext context;  // 임시 컨텍스트 생성
        auto result = ecmaEngine_->evaluateExpression(expression, context);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        totalExecutionTimeMs_ += static_cast<size_t>(duration.count());

        if (result.success) {
            // ECMAValue를 DataValue로 변환
            DataValue value = ecmaValueToDataValue(result.value);
            SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::evaluateExpression - Successfully evaluated: " + expression);
            return DataModelResult::createSuccess(value);
        }

        return DataModelResult::createError(result.errorMessage);

    } catch (const std::exception &e) {
        std::string error = "ECMAScriptDataModelEngine::evaluateExpression - Exception: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        return DataModelResult::createError(error);
    }
}

DataModelEngine::DataModelResult ECMAScriptDataModelEngine::executeScript(const std::string &script, Scope scope) {
    (void)scope;  // Suppress unused parameter warning

    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::executeScript - Executing script");

    if (!ecmaEngine_) {
        return DataModelResult::createError("JavaScript engine not initialized");
    }

    try {
        // ECMAScript 스크립트 실행
        SCXML::Runtime::RuntimeContext context;
        auto result = ecmaEngine_->executeScript(script, context);

        if (result.success) {
            return DataModelResult::createSuccess();
        }

        return DataModelResult::createError(result.errorMessage);

    } catch (const std::exception &e) {
        std::string error = "ECMAScriptDataModelEngine::executeScript - Exception: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        return DataModelResult::createError(error);
    }
}

bool ECMAScriptDataModelEngine::switchEngine(ECMAScriptEngineFactory::EngineType engineType) {
    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::switchEngine - Switching to engine type: " +
                  std::to_string(static_cast<int>(engineType)));

    try {
        // 새 엔진 생성
        ecmaEngine_ = ECMAScriptEngineFactory::create(engineType);

        if (ecmaEngine_ && ecmaEngine_->initialize()) {
            currentEngineType_ = engineType;
            SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::switchEngine - Successfully switched engine");
            return true;
        } else {
            SCXML::Common::Logger::error("ECMAScriptDataModelEngine::switchEngine - Failed to initialize engine");
            ecmaEngine_.reset();
            return false;
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("ECMAScriptDataModelEngine::switchEngine - Exception: " + std::string(e.what()));
        return false;
    }
}

std::string ECMAScriptDataModelEngine::getCurrentEngineName() const {
    return ecmaEngine_ ? ecmaEngine_->getEngineName() : "None";
}

std::string ECMAScriptDataModelEngine::getCurrentEngineVersion() const {
    return ecmaEngine_ ? ecmaEngine_->getEngineVersion() : "Unknown";
}

DataModelEngine::DataModelResult ECMAScriptDataModelEngine::defineFunction(const std::string &name,
                                                                           const std::vector<std::string> &params,
                                                                           const std::string &body) {
    (void)name;    // Suppress unused parameter warning
    (void)params;  // Suppress unused parameter warning
    (void)body;    // Suppress unused parameter warning

    SCXML::Common::Logger::debug(
        "ECMAScriptDataModelEngine::defineFunction - Function definition not supported in this implementation");

    // 현재 IECMAScriptEngine 인터페이스에 defineFunction이 없으므로
    // 임시로 지원하지 않음으로 처리
    return DataModelResult::createError(
        "Function definition not supported in current ECMAScript engine implementation");
}

DataModelEngine::DataModelResult ECMAScriptDataModelEngine::callFunction(const std::string &name,
                                                                         const std::vector<DataValue> &args) {
    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::callFunction - Calling function: " + name);

    if (!ecmaEngine_) {
        return DataModelResult::createError("JavaScript engine not initialized");
    }

    try {
        // DataValue 인수들을 ECMAValue로 변환
        std::vector<ECMAValue> ecmaArgs;
        for (const auto &arg : args) {
            ecmaArgs.push_back(dataValueToECMAValue(arg));
        }

        // callFunction 메서드가 IECMAScriptEngine 인터페이스에 없으므로 임시로 지원하지 않음
        (void)name;      // Suppress unused parameter warning
        (void)ecmaArgs;  // Suppress unused parameter warning
        return DataModelResult::createError("Function call not supported in current ECMAScript engine implementation");

    } catch (const std::exception &e) {
        std::string error = "ECMAScriptDataModelEngine::callFunction - Exception: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        return DataModelResult::createError(error);
    }
}

void ECMAScriptDataModelEngine::setupSCXMLSystemVariables(const std::string &sessionId, const std::string &sessionName,
                                                          const std::vector<std::string> &ioProcessors) {
    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::setupSCXMLSystemVariables - Setting up system variables");

    if (!ecmaEngine_) {
        SCXML::Common::Logger::error("ECMAScriptDataModelEngine::setupSCXMLSystemVariables - No JavaScript engine");
        return;
    }

    try {
        // _sessionid 설정
        ecmaEngine_->setVariable("_sessionid", ECMAValue(sessionId));

        // _name 설정
        ecmaEngine_->setVariable("_name", ECMAValue(sessionName));

        // _ioprocessors 객체 생성
        std::ostringstream ioProcessorScript;
        ioProcessorScript << "var _ioprocessors = {";
        for (size_t i = 0; i < ioProcessors.size(); ++i) {
            if (i > 0) {
                ioProcessorScript << ", ";
            }
            ioProcessorScript << "\"" << ioProcessors[i] << "\": {}";
        }
        ioProcessorScript << "};";

        SCXML::Runtime::RuntimeContext context;
        ecmaEngine_->executeScript(ioProcessorScript.str(), context);

        SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::setupSCXMLSystemVariables - System variables set up");

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("ECMAScriptDataModelEngine::setupSCXMLSystemVariables - Exception: " + std::string(e.what()));
    }
}

void ECMAScriptDataModelEngine::setCurrentEvent(const std::shared_ptr<::SCXML::Events::Event> &event) {
    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::setCurrentEvent - Setting current event");

    if (!ecmaEngine_) {
        SCXML::Common::Logger::error("ECMAScriptDataModelEngine::setCurrentEvent - No JavaScript engine");
        return;
    }

    try {
        if (event) {
            // _event 객체 생성
            std::ostringstream eventScript;
            eventScript << "var _event = {";
            eventScript << "  name: \"" << event->getName() << "\",";
            eventScript << "  type: \"" << (event->isInternal() ? "internal" : "external") << "\",";

            // 이벤트 데이터 처리
            auto eventData = event->getData();
            eventScript << "  data: ";

            std::visit(
                [&eventScript](const auto &v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::string>) {
                        // JSON 문자열인 경우 파싱 시도
                        if (!v.empty() && v.front() == '{' && v.back() == '}') {
                            eventScript << v;  // JSON 객체로 직접 사용
                        } else {
                            eventScript << "\"" << v << "\"";  // 문자열로 감싸기
                        }
                    } else if constexpr (std::is_same_v<T, int>) {
                        eventScript << v;
                    } else if constexpr (std::is_same_v<T, double>) {
                        eventScript << v;
                    } else if constexpr (std::is_same_v<T, bool>) {
                        eventScript << (v ? "true" : "false");
                    } else {
                        eventScript << "null";
                    }
                },
                eventData);

            eventScript << "};";

            SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::setCurrentEvent - Event script: " + eventScript.str());

            // _event 객체를 ECMAScript 환경에 설정 (executeScript 대신 setVariable 사용)
            SCXML::Common::Logger::warning("ECMAScriptDataModelEngine::setCurrentEvent - Skipping _event script execution to prevent "
                            "stack overflow");
            // TODO: _event 객체를 안전하게 설정하는 방법 구현 필요
        } else {
            // _event를 null로 설정 (executeScript 대신 setVariable 사용)
            ecmaEngine_->setVariable("_event", ECMAValue(std::monostate{}));
            SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::setCurrentEvent - Set _event to null using setVariable");
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("ECMAScriptDataModelEngine::setCurrentEvent - Exception: " + std::string(e.what()));
    }
}

void ECMAScriptDataModelEngine::setStateCheckFunction(const StateCheckFunction &func) {
    stateCheckFunction_ = func;

    if (ecmaEngine_ && func) {
        // In() 함수를 ECMAScript에 등록
        ecmaEngine_->registerNativeFunction("In", [this](const std::vector<ECMAValue> &args) -> ECMAValue {
            if (args.empty() || !stateCheckFunction_) {
                return ECMAValue(false);
            }

            // 첫 번째 인수를 상태 이름으로 변환
            std::string stateName;
            std::visit(
                [&stateName](const auto &v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::string>) {
                        stateName = v;
                    }
                },
                args[0]);

            return ECMAValue(stateCheckFunction_(stateName));
        });
    }
}

ECMAScriptDataModelEngine::EngineStats ECMAScriptDataModelEngine::getEngineStats() const {
    EngineStats stats;
    stats.engineName = getCurrentEngineName();
    stats.engineVersion = getCurrentEngineVersion();
    stats.expressionCount = expressionCount_;
    stats.executionTimeMs = totalExecutionTimeMs_;

    if (ecmaEngine_) {
        stats.memoryUsage = ecmaEngine_->getMemoryUsage();
    }

    return stats;
}

void ECMAScriptDataModelEngine::collectGarbage() {
    if (ecmaEngine_) {
        ecmaEngine_->collectGarbage();
    }
}

ECMAValue ECMAScriptDataModelEngine::dataValueToECMAValue(const DataValue &value) const {
    return std::visit(
        [](const auto &v) -> ECMAValue {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return ECMAValue(std::monostate{});
            } else if constexpr (std::is_same_v<T, bool>) {
                return ECMAValue(v);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return ECMAValue(static_cast<int>(v));
            } else if constexpr (std::is_same_v<T, double>) {
                return ECMAValue(v);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return ECMAValue(v);
            }
            return ECMAValue(std::monostate{});
        },
        value);
}

DataModelEngine::DataValue ECMAScriptDataModelEngine::ecmaValueToDataValue(const ECMAValue &value) const {
    return std::visit(
        [](const auto &v) -> DataValue {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return std::monostate{};
            } else if constexpr (std::is_same_v<T, bool>) {
                return v;
            } else if constexpr (std::is_same_v<T, int>) {
                return static_cast<int64_t>(v);
            } else if constexpr (std::is_same_v<T, double>) {
                return v;
            } else if constexpr (std::is_same_v<T, std::string>) {
                return v;
            }
            return std::monostate{};
        },
        value);
}

void ECMAScriptDataModelEngine::registerSCXMLBuiltins() {
    if (!ecmaEngine_) {
        return;
    }

    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::registerSCXMLBuiltins - Registering SCXML built-in functions");

    try {
        // SCXML 내장 함수들 등록 (In은 setStateCheckFunction에서 등록)

        // console.log 함수 등록
        ecmaEngine_->registerNativeFunction("console.log", [](const std::vector<ECMAValue> &args) -> ECMAValue {
            std::ostringstream oss;
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) {
                    oss << " ";
                }
                std::visit(
                    [&oss](const auto &v) {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, std::string>) {
                            oss << v;
                        } else if constexpr (std::is_same_v<T, int>) {
                            oss << v;
                        } else if constexpr (std::is_same_v<T, double>) {
                            oss << v;
                        } else if constexpr (std::is_same_v<T, bool>) {
                            oss << (v ? "true" : "false");
                        } else {
                            oss << "null";
                        }
                    },
                    args[i]);
            }
            std::cout << oss.str() << std::endl;
            return ECMAValue(std::monostate{});
        });

        SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::registerSCXMLBuiltins - SCXML built-ins registered");

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("ECMAScriptDataModelEngine::registerSCXMLBuiltins - Exception: " + std::string(e.what()));
    }
}

void ECMAScriptDataModelEngine::registerECMAScriptObjects() {
    if (!ecmaEngine_) {
        return;
    }

    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::registerECMAScriptObjects - Registering ECMAScript objects");

    try {
        // Math 객체 등록 (기본 함수들)
        std::string mathScript = R"(
            var Math = {
                random: function() { return 0.5; },  // 간단한 고정값으로 시작
                min: function() {
                    var min = arguments[0];
                    for (var i = 1; i < arguments.length; i++) {
                        if (arguments[i] < min) min = arguments[i];
                    }
                    return min;
                },
                max: function() {
                    var max = arguments[0];
                    for (var i = 1; i < arguments.length; i++) {
                        if (arguments[i] > max) max = arguments[i];
                    }
                    return max;
                }
            };
        )";
        SCXML::Runtime::RuntimeContext mathContext;
        ecmaEngine_->executeScript(mathScript, mathContext);

        SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::registerECMAScriptObjects - ECMAScript objects registered");

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("ECMAScriptDataModelEngine::registerECMAScriptObjects - Exception: " + std::string(e.what()));
    }
}

void ECMAScriptDataModelEngine::initializeSystemVariables() {
    if (!ecmaEngine_) {
        return;
    }

    SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::initializeSystemVariables - Initializing system variables");

    try {
        // 기본 시스템 변수들 설정
        ecmaEngine_->setVariable("_sessionid", ECMAValue(std::string("")));
        ecmaEngine_->setVariable("_name", ECMAValue(std::string("")));
        SCXML::Runtime::RuntimeContext systemContext;
        ecmaEngine_->executeScript("var _ioprocessors = {};", systemContext);
        ecmaEngine_->executeScript("var _event = null;", systemContext);

        SCXML::Common::Logger::debug("ECMAScriptDataModelEngine::initializeSystemVariables - System variables initialized");

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("ECMAScriptDataModelEngine::initializeSystemVariables - Exception: " + std::string(e.what()));
    }
}

DataModelEngine::DataModelResult
ECMAScriptDataModelEngine::ecmaResultToDataModelResult(const ECMAResult &result) const {
    if (result.success) {
        DataValue value = ecmaValueToDataValue(result.value);
        return DataModelResult::createSuccess(value);
    } else {
        return DataModelResult::createError(result.errorMessage);
    }
}

}  // namespace SCXML
