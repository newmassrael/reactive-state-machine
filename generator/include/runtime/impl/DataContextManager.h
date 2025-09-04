#pragma once

#include "../interfaces/IDataContextManager.h"
#include <memory>
#include <mutex>
#include <stack>
#include <unordered_map>

namespace SCXML {

namespace Model {
class DocumentModel;
}

// Forward declaration
class DataModelEngine;

namespace Runtime {

/**
 * @brief Default implementation of data context management
 */
class DataContextManager : public IDataContextManager {
public:
    DataContextManager();
    virtual ~DataContextManager() = default;

    // IDataContextManager interface implementation
    void initializeFromModel(std::shared_ptr<Model::DocumentModel> model) override;
    void setDataValue(const std::string &id, const std::string &value) override;
    std::string getDataValue(const std::string &id) const override;
    bool hasDataValue(const std::string &id) const override;
    std::string evaluateExpression(const std::string &expression) const override;
    bool evaluateCondition(const std::string &condition) const override;
    void setSessionName(const std::string &name) override;
    std::string getSessionName() const override;
    std::string getScriptBasePath() const override;
    SCXML::DataModelEngine *getDataModelEngine() const override;
    void clearAllData() override;
    std::unordered_map<std::string, std::string> getAllData() const override;

    // Additional implementation methods (not in interface)
    void setDataValue(const std::string &id, const DataValue &value);
    DataValue getDataValueVariant(const std::string &id) const;
    void removeDataValue(const std::string &id);

    void assignToLocation(const std::string &location, const std::string &value);
    std::string getLocationValue(const std::string &location) const;

    void pushDataContext();
    void popDataContext();
    size_t getContextDepth() const;



    // Additional methods for internal use
    void setScriptBasePath(const std::string &basePath);
    void setDataModelEngine(SCXML::DataModelEngine *engine);

private:
    mutable std::mutex dataMutex_;

    // Current data context
    std::unordered_map<std::string, DataValue> currentData_;

    // Context stack for scoped data
    std::stack<std::unordered_map<std::string, DataValue>> contextStack_;

    std::string sessionName_;
    std::string scriptBasePath_;
    SCXML::DataModelEngine *dataModelEngine_;

    // Helper methods
    std::string dataValueToString(const DataValue &value) const;
    DataValue stringToDataValue(const std::string &value) const;

    // Expression evaluation helpers
    bool isSimpleExpression(const std::string &expression) const;
    std::string evaluateSimpleExpression(const std::string &expression) const;
    bool evaluateSimpleCondition(const std::string &condition) const;
};

}  // namespace Runtime
}  // namespace SCXML