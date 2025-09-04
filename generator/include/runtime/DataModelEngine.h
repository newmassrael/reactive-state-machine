#pragma once

#include "../common/Result.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace SCXML {

namespace Model {
class IDataModelItem;
}
// Forward declarations
namespace Runtime {
class RuntimeContext;
}

namespace Core {
class IDataModelItem;
}

// Using declaration for interface
using SCXML::Model::IDataModelItem;

class ExecutableContentProcessor;

/**
 * @brief SCXML Data Model Engine
 *
 * This class implements the complete SCXML data model according to the W3C specification.
 * It supports multiple data model types (ECMAScript, XPath, null) and provides
 * comprehensive data storage, retrieval, and manipulation capabilities.
 */
class DataModelEngine {
public:
    /**
     * @brief Supported data model types
     */
    enum class DataModelType {
        ECMASCRIPT,  // JavaScript/ECMAScript data model
        XPATH,       // XPath data model
        NULL_MODEL,  // Null data model (no expressions)
        AUTO_DETECT  // Auto-detect from content
    };

    /**
     * @brief Forward declaration for recursive data structures
     */
    class DataArray;
    class DataObject;

    /**
     * @brief Data value types supported by the data model
     */
    using DataValue = std::variant<std::monostate,              // null/undefined
                                   bool,                        // boolean
                                   int64_t,                     // integer
                                   double,                      // floating point
                                   std::string,                 // string
                                   std::shared_ptr<DataArray>,  // array
                                   std::shared_ptr<DataObject>  // object
                                   >;

    /**
     * @brief Array data structure for JavaScript-like array support
     */
    class DataArray {
    public:
        using Container = std::vector<DataValue>;

        DataArray() = default;

        explicit DataArray(size_t size) : elements_(size) {}

        DataArray(std::initializer_list<DataValue> init) : elements_(init) {}

        // Element access
        DataValue &at(size_t index) {
            return elements_.at(index);
        }

        const DataValue &at(size_t index) const {
            return elements_.at(index);
        }

        DataValue &operator[](size_t index) {
            return elements_[index];
        }

        const DataValue &operator[](size_t index) const {
            return elements_[index];
        }

        // Capacity
        size_t size() const {
            return elements_.size();
        }

        bool empty() const {
            return elements_.empty();
        }

        void resize(size_t size) {
            elements_.resize(size);
        }

        void reserve(size_t capacity) {
            elements_.reserve(capacity);
        }

        // Modifiers
        void push_back(const DataValue &value) {
            elements_.push_back(value);
        }

        void push_back(DataValue &&value) {
            elements_.push_back(std::move(value));
        }

        template <typename... Args> void emplace_back(Args &&...args) {
            elements_.emplace_back(std::forward<Args>(args)...);
        }

        void pop_back() {
            elements_.pop_back();
        }

        void clear() {
            elements_.clear();
        }

        // Iterators
        Container::iterator begin() {
            return elements_.begin();
        }

        Container::iterator end() {
            return elements_.end();
        }

        Container::const_iterator begin() const {
            return elements_.begin();
        }

        Container::const_iterator end() const {
            return elements_.end();
        }

        Container::const_iterator cbegin() const {
            return elements_.cbegin();
        }

        Container::const_iterator cend() const {
            return elements_.cend();
        }

        // Array operations
        void insert(size_t index, const DataValue &value) {
            if (index <= elements_.size()) {
                elements_.insert(elements_.begin() + static_cast<long>(index), value);
            }
        }

        void erase(size_t index) {
            if (index < elements_.size()) {
                elements_.erase(elements_.begin() + static_cast<long>(index));
            }
        }

        // Find element index (-1 if not found)
        int64_t indexOf(const DataValue &value) const;

        // Array to string representation
        std::string toString() const;

    private:
        Container elements_;
    };

    /**
     * @brief Object data structure for JavaScript-like object support
     */
    class DataObject {
    public:
        using Container = std::unordered_map<std::string, DataValue>;

        DataObject() = default;

        DataObject(std::initializer_list<std::pair<const std::string, DataValue>> init) : properties_(init) {}

        // Property access
        DataValue &at(const std::string &key) {
            return properties_.at(key);
        }

        const DataValue &at(const std::string &key) const {
            return properties_.at(key);
        }

        DataValue &operator[](const std::string &key) {
            return properties_[key];
        }

        const DataValue &operator[](const std::string &key) const {
            return properties_.at(key);
        }

        // Property management
        bool hasProperty(const std::string &key) const {
            return properties_.find(key) != properties_.end();
        }

        bool setProperty(const std::string &key, const DataValue &value) {
            properties_[key] = value;
            return true;
        }

        bool removeProperty(const std::string &key) {
            return properties_.erase(key) > 0;
        }

        // Capacity
        size_t size() const {
            return properties_.size();
        }

        bool empty() const {
            return properties_.empty();
        }

        void clear() {
            properties_.clear();
        }

        // Get all property names
        std::vector<std::string> getPropertyNames() const {
            std::vector<std::string> names;
            names.reserve(properties_.size());
            for (const auto &pair : properties_) {
                names.push_back(pair.first);
            }
            return names;
        }

        // Iterators
        Container::iterator begin() {
            return properties_.begin();
        }

        Container::iterator end() {
            return properties_.end();
        }

        Container::const_iterator begin() const {
            return properties_.begin();
        }

        Container::const_iterator end() const {
            return properties_.end();
        }

        Container::const_iterator cbegin() const {
            return properties_.cbegin();
        }

        Container::const_iterator cend() const {
            return properties_.cend();
        }

        // Object to string representation
        std::string toString() const;

    private:
        Container properties_;
    };

    /**
     * @brief Data model scope
     */
    enum class Scope {
        GLOBAL,  // Global scope (accessible from all states)
        LOCAL,   // Local scope (state-specific)
        SESSION  // Session scope (persists across state machine instances)
    };

    /**
     * @brief Data model operation result
     */
    struct DataModelResult {
        bool success;              // Whether operation succeeded
        std::string errorMessage;  // Error message if operation failed
        DataValue value;           // Result value for get operations

        DataModelResult() : success(false) {}

        static DataModelResult createSuccess(const DataValue &val = std::monostate{}) {
            DataModelResult result;
            result.success = true;
            result.value = val;
            return result;
        }

        static DataModelResult createError(const std::string &error) {
            DataModelResult result;
            result.success = false;
            result.errorMessage = error;
            return result;
        }
    };

    /**
     * @brief Construct a new Data Model Engine
     * @param type Data model type to use
     */
    explicit DataModelEngine(DataModelType type = DataModelType::ECMASCRIPT);

    /**
     * @brief Destructor
     */
    ~DataModelEngine();

    // ========== Data Model Management ==========

    /**
     * @brief Initialize data model from SCXML data items
     * @param dataItems Vector of data model items from SCXML
     * @param context Runtime context
     * @return Result of initialization
     */
    DataModelResult initializeFromDataItems(const std::vector<std::shared_ptr<IDataModelItem>> &dataItems,
                                            SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Set data model type
     * @param type Data model type
     */
    void setDataModelType(DataModelType type);

    /**
     * @brief Get current data model type
     * @return Current data model type
     */
    DataModelType getDataModelType() const {
        return dataModelType_;
    }

    /**
     * @brief Get ECMAScript engine if available
     * @return Raw pointer to ECMAScript engine, nullptr if not using ECMAScript
     */
    class IECMAScriptEngine *getECMAScriptEngine() const;

    /**
     * @brief Clear all data in the model
     * @param scope Scope to clear (optional, clears all if not specified)
     */
    void clear(std::optional<Scope> scope = std::nullopt);

    // ========== Data Operations ==========

    /**
     * @brief Set data value
     * @param location Data location path (e.g., "user.name", "items[0]")
     * @param value Value to set
     * @param scope Data scope
     * @return Result of operation
     */
    DataModelResult setValue(const std::string &location, const DataValue &value, Scope scope = Scope::GLOBAL);

    /**
     * @brief Get data value
     * @param location Data location path
     * @param scope Data scope (searches all scopes if not specified)
     * @return Result containing the value
     */
    DataModelResult getValue(const std::string &location, std::optional<Scope> scope = std::nullopt) const;

    /**
     * @brief Check if data location exists
     * @param location Data location path
     * @param scope Data scope (searches all scopes if not specified)
     * @return true if location exists
     */
    bool hasValue(const std::string &location, std::optional<Scope> scope = std::nullopt) const;

    /**
     * @brief Remove data value
     * @param location Data location path
     * @param scope Data scope (searches all scopes if not specified)
     * @return Result of operation
     */
    DataModelResult removeValue(const std::string &location, std::optional<Scope> scope = std::nullopt);

    // ========== Expression Evaluation ==========

    /**
     * @brief Evaluate expression in current data model context
     * @param expression Expression to evaluate
     * @param context Runtime context
     * @return Result containing evaluated value
     */
    DataModelResult evaluateExpression(const std::string &expression, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Evaluate condition expression (returns boolean)
     * @param condition Condition expression
     * @param context Runtime context
     * @return true if condition evaluates to true
     */
    bool evaluateCondition(const std::string &condition, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute script in current data model context
     * @param script Script to execute
     * @param context Runtime context
     * @return Result of script execution
     */
    DataModelResult executeScript(const std::string &script, SCXML::Runtime::RuntimeContext &context);

    // ========== Data Type Utilities ==========

    /**
     * @brief Convert DataValue to string representation
     * @param value Value to convert
     * @return String representation
     */
    std::string valueToString(const DataValue &value) const;

    /**
     * @brief Convert DataValue to boolean
     * @param value Value to convert
     * @return Boolean representation
     */
    bool valueToBool(const DataValue &value) const;

    /**
     * @brief Convert DataValue to numeric types
     * @param value Value to convert
     * @return Numeric value (0 if conversion fails)
     */
    double valueToNumber(const DataValue &value) const;

    /**
     * @brief Parse string value to DataValue with type detection
     * @param str String to parse
     * @return Parsed DataValue
     */
    DataValue parseValue(const std::string &str) const;

    /**
     * @brief Get type name of DataValue
     * @param value Value to check
     * @return Type name string
     */
    std::string getValueType(const DataValue &value) const;

    // ========== Complex Data Operations ==========

    /**
     * @brief Create array from expression or literal
     * @param arrayExpr Array expression (e.g., "[1,2,3]" or "items")
     * @param context Runtime context
     * @return Result containing array value
     */
    DataModelResult createArray(const std::string &arrayExpr, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Create object from expression or literal
     * @param objectExpr Object expression (e.g., "{a:1,b:2}" or "config")
     * @param context Runtime context
     * @return Result containing object value
     */
    DataModelResult createObject(const std::string &objectExpr, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Access array element
     * @param arrayLocation Location of array
     * @param index Array index
     * @param scope Data scope
     * @return Result containing element value
     */
    DataModelResult getArrayElement(const std::string &arrayLocation, int64_t index,
                                    std::optional<Scope> scope = std::nullopt) const;

    /**
     * @brief Set array element
     * @param arrayLocation Location of array
     * @param index Array index
     * @param value Value to set
     * @param scope Data scope
     * @return Result of operation
     */
    DataModelResult setArrayElement(const std::string &arrayLocation, int64_t index, const DataValue &value,
                                    Scope scope = Scope::GLOBAL);

    /**
     * @brief Get object property
     * @param objectLocation Location of object
     * @param property Property name
     * @param scope Data scope
     * @return Result containing property value
     */
    DataModelResult getObjectProperty(const std::string &objectLocation, const std::string &property,
                                      std::optional<Scope> scope = std::nullopt) const;

    /**
     * @brief Set object property
     * @param objectLocation Location of object
     * @param property Property name
     * @param value Value to set
     * @param scope Data scope
     * @return Result of operation
     */
    DataModelResult setObjectProperty(const std::string &objectLocation, const std::string &property,
                                      const DataValue &value, Scope scope = Scope::GLOBAL);

    // ========== Variable Scoping and Inheritance ==========

    /**
     * @brief Push new local scope (create scope stack)
     * @param scopeId Unique identifier for the scope
     */
    void pushScope(const std::string &scopeId = "");

    /**
     * @brief Pop current local scope
     * @return true if scope was popped, false if no scope to pop
     */
    bool popScope();

    /**
     * @brief Get current scope depth
     * @return Number of nested scopes
     */
    size_t getScopeDepth() const;

    /**
     * @brief Set variable in specific scope with inheritance
     * @param location Variable location
     * @param value Value to set
     * @param scope Target scope
     * @param allowInheritance Whether to allow inheritance from parent scopes
     * @return Result of operation
     */
    DataModelResult setValueWithInheritance(const std::string &location, const DataValue &value,
                                            Scope scope = Scope::LOCAL, bool allowInheritance = true);

    /**
     * @brief Get variable with scope inheritance
     * @param location Variable location
     * @param searchOrder Custom scope search order
     * @return Result containing the value and scope where found
     */
    DataModelResult getValueWithInheritance(const std::string &location,
                                            const std::vector<Scope> &searchOrder = {Scope::LOCAL, Scope::GLOBAL,
                                                                                     Scope::SESSION});

    /**
     * @brief Check variable existence with inheritance
     * @param location Variable location
     * @param searchOrder Custom scope search order
     * @return Scope where variable exists, or nullopt if not found
     */
    std::optional<Scope> findVariableScope(const std::string &location,
                                           const std::vector<Scope> &searchOrder = {Scope::LOCAL, Scope::GLOBAL,
                                                                                    Scope::SESSION});

    /**
     * @brief Copy variables from one scope to another
     * @param fromScope Source scope
     * @param toScope Target scope
     * @param filter Optional filter pattern (regex)
     * @return Number of variables copied
     */
    size_t copyScope(Scope fromScope, Scope toScope, const std::string &filter = ".*");

    /**
     * @brief Merge scopes with conflict resolution
     * @param primaryScope Primary scope (wins conflicts)
     * @param secondaryScope Secondary scope
     * @param targetScope Target scope for merged result
     * @return Number of variables merged
     */
    size_t mergeScopes(Scope primaryScope, Scope secondaryScope, Scope targetScope);

    // ========== Data Binding and Reactivity ==========

    /**
     * @brief Callback function type for data change notifications
     */
    using DataChangeCallback = std::function<void(const std::string &location, const DataValue &oldValue,
                                                  const DataValue &newValue, Scope scope)>;

    /**
     * @brief Register callback for data changes
     * @param location Variable location to watch (supports wildcards)
     * @param callback Callback function
     * @param scope Scope to watch (all scopes if not specified)
     * @return Unique callback ID for removal
     */
    size_t onDataChange(const std::string &location, DataChangeCallback callback,
                        std::optional<Scope> scope = std::nullopt);

    /**
     * @brief Remove data change callback
     * @param callbackId Callback ID returned by onDataChange
     * @return true if callback was removed
     */
    bool removeDataChangeCallback(size_t callbackId);

    /**
     * @brief Create bidirectional binding between two locations
     * @param location1 First variable location
     * @param location2 Second variable location
     * @param scope1 Scope for first variable
     * @param scope2 Scope for second variable
     * @return Binding ID for removal
     */
    size_t createBinding(const std::string &location1, const std::string &location2, Scope scope1 = Scope::GLOBAL,
                         Scope scope2 = Scope::GLOBAL);

    /**
     * @brief Remove bidirectional binding
     * @param bindingId Binding ID returned by createBinding
     * @return true if binding was removed
     */
    bool removeBinding(size_t bindingId);

    /**
     * @brief Create computed property that updates automatically
     * @param location Target location for computed value
     * @param expression ECMAScript expression for computation
     * @param dependencies List of dependency locations
     * @param targetScope Scope for computed property
     * @return Computed property ID for removal
     */
    size_t createComputed(const std::string &location, const std::string &expression,
                          const std::vector<std::string> &dependencies, Scope targetScope = Scope::GLOBAL);

    /**
     * @brief Remove computed property
     * @param computedId Computed property ID
     * @return true if computed property was removed
     */
    bool removeComputed(size_t computedId);

    /**
     * @brief Trigger manual update of all reactive dependencies
     * @param location Location that changed
     * @param scope Scope of the change
     */
    void triggerReactiveUpdate(const std::string &location, Scope scope);

    /**
     * @brief Get all active bindings
     * @return Map of binding IDs to binding descriptions
     */
    std::unordered_map<size_t, std::string> getActiveBindings() const;

    /**
     * @brief Get all active computed properties
     * @return Map of computed IDs to computed descriptions
     */
    std::unordered_map<size_t, std::string> getActiveComputed() const;

    // ========== Data Model Introspection ==========

    /**
     * @brief Get all variable names in scope
     * @param scope Data scope (all scopes if not specified)
     * @return Vector of variable names
     */
    std::vector<std::string> getVariableNames(std::optional<Scope> scope = std::nullopt) const;

    /**
     * @brief Get data model statistics
     * @return Statistics as key-value pairs
     */
    std::unordered_map<std::string, std::string> getStatistics() const;

    /**
     * @brief Export data model to JSON string
     * @param scope Data scope to export
     * @return JSON representation
     */
    std::string exportToJSON(std::optional<Scope> scope = std::nullopt) const;

    /**
     * @brief Import data model from JSON string
     * @param json JSON data
     * @param scope Target scope
     * @return Result of import operation
     */
    DataModelResult importFromJSON(const std::string &json, Scope scope = Scope::GLOBAL);

protected:
    /**
     * @brief Parse location path (e.g., "user.profile.name" or "items[0].title")
     * @param location Location string
     * @return Parsed location components
     */
    struct LocationPath {
        std::vector<std::string> segments;  // Path segments
        std::vector<int64_t> indices;       // Array indices (-1 if not array access)
        bool isValid;                       // Whether parsing succeeded

        LocationPath() : isValid(false) {}
    };

    LocationPath parseLocation(const std::string &location) const;

    /**
     * @brief Get data storage for scope
     * @param scope Data scope
     * @return Reference to data storage
     */
    std::unordered_map<std::string, DataValue> &getDataStorage(Scope scope);
    const std::unordered_map<std::string, DataValue> &getDataStorage(Scope scope) const;

    /**
     * @brief Navigate to location in data structure
     * @param storage Data storage
     * @param path Parsed location path
     * @param createPath Whether to create missing path elements
     * @return Pointer to target value (nullptr if not found/created)
     */
    DataValue *navigateToLocation(std::unordered_map<std::string, DataValue> &storage, const LocationPath &path,
                                  bool createPath = false);

    const DataValue *navigateToLocation(const std::unordered_map<std::string, DataValue> &storage,
                                        const LocationPath &path) const;

    /**
     * @brief Initialize expression evaluators for data model type
     */
    void initializeEvaluators();

private:
    // Data model configuration
    DataModelType dataModelType_;

    // Data storage by scope
    std::unordered_map<std::string, DataValue> globalData_;
    std::unordered_map<std::string, DataValue> localData_;
    std::unordered_map<std::string, DataValue> sessionData_;

    // Scope management
    std::vector<std::unordered_map<std::string, DataValue>> scopeStack_;
    std::vector<std::string> scopeIds_;

    // Reactivity system
    struct DataChangeListener {
        size_t id;
        std::string locationPattern;
        DataChangeCallback callback;
        std::optional<Scope> scope;
    };

    struct DataBinding {
        size_t id;
        std::string location1;
        std::string location2;
        Scope scope1;
        Scope scope2;
        bool updating;  // Prevent circular updates
    };

    struct ComputedProperty {
        size_t id;
        std::string location;
        std::string expression;
        std::vector<std::string> dependencies;
        Scope targetScope;
        DataValue lastValue;
    };

    std::vector<DataChangeListener> dataChangeListeners_;
    std::vector<DataBinding> dataBindings_;
    std::vector<ComputedProperty> computedProperties_;
    size_t nextCallbackId_;

    // Expression evaluators using PIMPL pattern for type safety
    struct EvaluatorImpl;
    std::unique_ptr<EvaluatorImpl> evaluatorImpl_;

    // Type conversion functions
    std::unordered_map<std::string, std::function<DataValue(const std::string &)>> typeConverters_;

    /**
     * @brief Initialize type converters
     */
    void initializeTypeConverters();

    /**
     * @brief Check if location matches pattern (supports wildcards)
     * @param location Variable location
     * @param pattern Pattern to match against
     * @return true if matches
     */
    bool matchesPattern(const std::string &location, const std::string &pattern) const;

    /**
     * @brief Notify data change listeners
     * @param location Variable location
     * @param oldValue Previous value
     * @param newValue New value
     * @param scope Variable scope
     */
    void notifyDataChange(const std::string &location, const DataValue &oldValue, const DataValue &newValue,
                          Scope scope);

    /**
     * @brief Update computed properties dependent on location
     * @param location Variable location that changed
     * @param scope Variable scope
     */
    void updateComputedProperties(const std::string &location, Scope scope);

    /**
     * @brief Update bidirectional bindings
     * @param location Variable location that changed
     * @param newValue New value
     * @param scope Variable scope
     */
    void updateBindings(const std::string &location, const DataValue &newValue, Scope scope);

    /**
     * @brief Navigate to nested value within a DataValue
     * @param value Parent value to navigate in
     * @param key Key to navigate to
     * @param createPath Whether to create path if it doesn't exist
     * @param isLeaf Whether this is the final segment in path
     * @return Pointer to nested value or nullptr
     */
    DataValue *navigateNestedValue(DataValue &value, const std::string &key, bool createPath, bool isLeaf);

    /**
     * @brief Navigate to nested value (const version)
     * @param value Parent value to navigate in
     * @param key Key to navigate to
     * @return Const pointer to nested value or nullptr
     */
    const DataValue *navigateNestedValue(const DataValue &value, const std::string &key) const;

    /**
     * @brief Remove nested value from a DataValue
     * @param value Parent value to remove from
     * @param key Key to remove
     * @return true if removal succeeded
     */
    bool removeNestedValue(DataValue &value, const std::string &key);

    /**
     * @brief Set nested value within a DataValue
     * @param value Parent value to set in
     * @param key Key to set
     * @param newValue Value to set
     * @return true if setting succeeded
     */
    bool setNestedValue(DataValue &value, const std::string &key, const DataValue &newValue);

    /**
     * @brief Convert DataValue to object if needed for nested access
     * @param value Value to convert
     * @return true if conversion succeeded or was not needed
     */
    bool ensureObjectForNesting(DataValue &value);

    /**
     * @brief Convert string key to array index if applicable
     * @param key String key
     * @return Optional array index
     */
    std::optional<size_t> parseArrayIndex(const std::string &key) const;

    /**
     * @brief Log data model operation
     * @param operation Operation name
     * @param location Data location
     * @param success Whether operation succeeded
     */
    void logOperation(const std::string &operation, const std::string &location, bool success) const;
};

}  // namespace SCXML