#include "runtime/impl/DataContextManager.h"
#include "common/Logger.h"
#include "model/DocumentModel.h"
#include "runtime/DataModelEngine.h"

namespace SCXML {
namespace Runtime {

DataContextManager::DataContextManager() : dataModelEngine_(nullptr) {
    SCXML::Common::Logger::debug("DataContextManager::Constructor - Creating data context manager");
}

void DataContextManager::initializeFromModel(std::shared_ptr<Model::DocumentModel> /* model */) {
    // Clear existing data contexts
    while (!contextStack_.empty()) {
        contextStack_.pop();
    }
    currentData_.clear();
    
    SCXML::Common::Logger::debug("DataContextManager::initializeFromModel - Initialized with model");
}

// Default destructor is already declared in header

void DataContextManager::setDataValue(const std::string &id, const std::string &value) {
    // Original logic: delegate to DataModelEngine
    if (!dataModelEngine_) {
        SCXML::Common::Logger::error("DataContextManager::setDataValue - No data model engine available");
        return;
    }

    auto result = dataModelEngine_->setValue(id, value);
    if (!result.success) {
        SCXML::Common::Logger::error("DataContextManager::setDataValue - Failed to set value for: " + id + " = " + value);
    } else {
        SCXML::Common::Logger::debug("DataContextManager::setDataValue - Set '" + id + "' = '" + value + "'");
    }
}

void DataContextManager::setDataValue(const std::string &id, const DataValue &value) {
    // Convert DataValue to string and use original logic
    std::string stringValue = dataValueToString(value);
    setDataValue(id, stringValue);
    SCXML::Common::Logger::debug("DataContextManager::setDataValue - Set variant for: " + id);
}

std::string DataContextManager::getDataValue(const std::string &id) const {
    // Original logic: delegate to DataModelEngine
    if (!dataModelEngine_) {
        SCXML::Common::Logger::warning("DataContextManager::getDataValue - No data model engine available");
        return "";
    }

    auto result = dataModelEngine_->getValue(id);
    if (result.success) {
        return dataModelEngine_->valueToString(result.value);
    } else {
        SCXML::Common::Logger::debug("DataContextManager::getDataValue - Variable not found: " + id);
        return "";
    }
}

DataValue DataContextManager::getDataValueVariant(const std::string &id) const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    auto it = currentData_.find(id);
    return (it != currentData_.end()) ? it->second : DataValue{};
}

bool DataContextManager::hasDataValue(const std::string &id) const {
    // Original logic: check with DataModelEngine
    if (!dataModelEngine_) {
        return false;
    }

    auto result = dataModelEngine_->getValue(id);
    return result.success;
}

void DataContextManager::removeDataValue(const std::string &id) {
    // Implement removal through DataModelEngine if possible
    if (dataModelEngine_) {
        // Most DataModelEngines don't have explicit remove, so we could set to empty
        dataModelEngine_->setValue(id, "");
    }
    SCXML::Common::Logger::debug("DataContextManager::removeDataValue - Removed: " + id);
}

std::unordered_map<std::string, std::string> DataContextManager::getAllData() const {
    // Original logic: get all variable names from DataModelEngine
    std::unordered_map<std::string, std::string> result;

    if (dataModelEngine_) {
        auto variableNames = dataModelEngine_->getVariableNames();
        for (const auto &name : variableNames) {
            auto valueResult = dataModelEngine_->getValue(name);
            if (valueResult.success) {
                result[name] = dataModelEngine_->valueToString(valueResult.value);
            }
        }
    }

    return result;
}

void DataContextManager::clearAllData() {
    // Clear through DataModelEngine if possible
    if (dataModelEngine_) {
        auto variableNames = dataModelEngine_->getVariableNames();
        for (const auto &name : variableNames) {
            dataModelEngine_->setValue(name, "");
        }
    }
    SCXML::Common::Logger::debug("DataContextManager::clearAllData - Cleared all data");
}

std::string DataContextManager::evaluateExpression(const std::string &expression) const {
    if (!dataModelEngine_) {
        SCXML::Common::Logger::warning("DataContextManager::evaluateExpression - No data model engine available");
        return expression;  // Fallback to literal
    }

    // Original logic: delegate to DataModelEngine
    // Note: We'll need to pass RuntimeContext reference from caller
    // For now, use simplified evaluation
    SCXML::Common::Logger::debug("DataContextManager::evaluateExpression - Evaluating: " + expression);

    // Simple literal return for now - would need RuntimeContext reference for full evaluation
    return expression;
}

bool DataContextManager::evaluateCondition(const std::string &condition) const {
    if (!dataModelEngine_) {
        SCXML::Common::Logger::warning("DataContextManager::evaluateCondition - No data model engine available");
        return !condition.empty();  // Fallback logic
    }

    // Original logic: evaluate as expression and check truthiness
    // Note: We'll need RuntimeContext reference for full evaluation
    SCXML::Common::Logger::debug("DataContextManager::evaluateCondition - Evaluating: " + condition);

    // Simple boolean logic for now - would need RuntimeContext reference for full evaluation
    return !condition.empty() && condition != "false" && condition != "0";
}

void DataContextManager::assignToLocation(const std::string &location, const std::string &value) {
    setDataValue(location, value);
}

std::string DataContextManager::getLocationValue(const std::string &location) const {
    return getDataValue(location);
}

void DataContextManager::pushDataContext() {
    std::lock_guard<std::mutex> lock(dataMutex_);
    contextStack_.push(currentData_);
    SCXML::Common::Logger::debug("DataContextManager::pushDataContext - Context depth: " + std::to_string(contextStack_.size()));
}

void DataContextManager::popDataContext() {
    std::lock_guard<std::mutex> lock(dataMutex_);
    if (!contextStack_.empty()) {
        currentData_ = contextStack_.top();
        contextStack_.pop();
        SCXML::Common::Logger::debug("DataContextManager::popDataContext - Context depth: " + std::to_string(contextStack_.size()));
    }
}

size_t DataContextManager::getContextDepth() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return contextStack_.size();
}



void DataContextManager::setSessionName(const std::string &name) {
    sessionName_ = name;
}

std::string DataContextManager::getSessionName() const {
    return sessionName_;
}

std::string DataContextManager::getScriptBasePath() const {
    return scriptBasePath_;
}

SCXML::DataModelEngine *DataContextManager::getDataModelEngine() const {
    return dataModelEngine_;
}

void DataContextManager::setDataModelEngine(SCXML::DataModelEngine *engine) {
    dataModelEngine_ = engine;
}

void DataContextManager::setScriptBasePath(const std::string &basePath) {
    scriptBasePath_ = basePath;
}

std::string DataContextManager::dataValueToString(const DataValue &value) const {
    return std::visit(
        [](const auto &v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return v;
            } else if constexpr (std::is_same_v<T, int>) {
                return std::to_string(v);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::to_string(v);
            } else if constexpr (std::is_same_v<T, bool>) {
                return v ? "true" : "false";
            } else {
                return "";
            }
        },
        value);
}

DataValue DataContextManager::stringToDataValue(const std::string &value) const {
    // Simple conversion - could be enhanced with type detection
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }

    // Try to parse as number
    try {
        if (value.find('.') != std::string::npos) {
            return std::stod(value);
        } else {
            return std::stoi(value);
        }
    } catch (...) {
        return value;  // Return as string if parsing fails
    }
}

bool DataContextManager::isSimpleExpression(const std::string &expression) const {
    // Simple heuristic - could be enhanced
    return expression.find_first_of("()[]{}+-*/<>=!&|") == std::string::npos;
}

std::string DataContextManager::evaluateSimpleExpression(const std::string &expression) const {
    // Placeholder for simple variable lookup
    return getDataValue(expression);
}

bool DataContextManager::evaluateSimpleCondition(const std::string &condition) const {
    // Placeholder for simple condition evaluation
    return !condition.empty() && condition != "false" && condition != "0";
}

}  // namespace Runtime
}  // namespace SCXML