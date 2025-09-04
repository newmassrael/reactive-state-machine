#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>

namespace SCXML {

// Forward declarations
class DataModelEngine;

namespace Model {
class DocumentModel;
}

namespace Runtime {

using DataValue = std::variant<std::monostate, std::string, int, double, bool>;

// Using declarations for Model namespace
using SCXML::Model::DocumentModel;

/**
 * @brief Interface for data context management operations
 */
class IDataContextManager {
public:
    virtual ~IDataContextManager() = default;

    // Core interface methods
    virtual void initializeFromModel(std::shared_ptr<DocumentModel> model) = 0;

    // Additional methods needed by RuntimeContext
    virtual void setDataValue(const std::string &id, const std::string &value) = 0;
    virtual std::string getDataValue(const std::string &id) const = 0;
    virtual bool hasDataValue(const std::string &id) const = 0;
    virtual std::string evaluateExpression(const std::string &expression) const = 0;
    virtual bool evaluateCondition(const std::string &condition) const = 0;
    virtual void setSessionName(const std::string &name) = 0;
    virtual std::string getSessionName() const = 0;
    virtual std::string getScriptBasePath() const = 0;
    virtual SCXML::DataModelEngine *getDataModelEngine() const = 0;
    virtual void clearAllData() = 0;
    virtual std::unordered_map<std::string, std::string> getAllData() const = 0;
};

}  // namespace Runtime
}  // namespace SCXML
