#ifndef TYPESAFEXPATH_H
#define TYPESAFEXPATH_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

// Forward declarations for libxml2
struct _xmlDoc;
struct _xmlNode;
struct _xmlXPathContext;
struct _xmlXPathObject;
struct _xmlChar;

typedef struct _xmlDoc xmlDoc;
typedef struct _xmlNode xmlNode;
typedef struct _xmlXPathContext xmlXPathContext;
typedef struct _xmlXPathObject xmlXPathObject;
// Forward declaration - actual typedef handled by libxml2 headers
struct _xmlChar;

namespace SCXML {
namespace Common {

/**
 * @brief Type-safe wrapper for XPath operations
 */
class TypeSafeXPath {
public:
    /**
     * @brief XPath variable with type safety
     */
    class XPathVariable {
    public:
        enum class Type { STRING, NUMBER, BOOLEAN, NODE_SET };

        XPathVariable(const std::string &name, const std::string &value)
            : name_(name), type_(Type::STRING), stringValue_(value) {}

        XPathVariable(const std::string &name, double value) : name_(name), type_(Type::NUMBER), numberValue_(value) {}

        XPathVariable(const std::string &name, bool value) : name_(name), type_(Type::BOOLEAN), boolValue_(value) {}

        const std::string &getName() const {
            return name_;
        }

        Type getType() const {
            return type_;
        }

        const std::string &getStringValue() const {
            return stringValue_;
        }

        double getNumberValue() const {
            return numberValue_;
        }

        bool getBooleanValue() const {
            return boolValue_;
        }

    private:
        std::string name_;
        Type type_;
        std::string stringValue_;
        double numberValue_ = 0.0;
        bool boolValue_ = false;
    };

    /**
     * @brief Type-safe XPath context
     */
    class XPathContext {
    public:
        XPathContext(xmlDoc *doc);
        ~XPathContext();

        void setVariable(std::shared_ptr<XPathVariable> variable);
        std::shared_ptr<XPathVariable> getVariable(const std::string &name) const;
        void removeVariable(const std::string &name);

        xmlXPathContext *getContext() const {
            return context_;
        }

        // Static callback for libxml2 variable resolution
        static xmlXPathObject *variableResolver(void *data, const unsigned char *name, const unsigned char *nsUri);

    private:
        xmlXPathContext *context_;
        std::unordered_map<std::string, std::shared_ptr<XPathVariable>> variables_;

        // Custom deleter for XPath variables
        static void freeXPathVariable(xmlXPathObject *obj);
    };

    /**
     * @brief RAII wrapper for XPath objects
     */
    class XPathObjectGuard {
    public:
        XPathObjectGuard(xmlXPathObject *obj) : object_(obj) {}

        ~XPathObjectGuard();

        xmlXPathObject *get() const {
            return object_;
        }

        xmlXPathObject *release() {
            xmlXPathObject *obj = object_;
            object_ = nullptr;
            return obj;
        }

    private:
        xmlXPathObject *object_;
    };

    /**
     * @brief Type-safe XPath evaluator
     */
    class XPathEvaluator {
    public:
        XPathEvaluator(std::shared_ptr<XPathContext> context) : context_(context) {}

        std::string evaluateString(const std::string &expression);
        double evaluateNumber(const std::string &expression);
        bool evaluateBoolean(const std::string &expression);

        // Evaluate and return raw XPath object (with RAII wrapper)
        XPathObjectGuard evaluateExpression(const std::string &expression);

    private:
        std::shared_ptr<XPathContext> context_;
    };

    /**
     * @brief Factory methods for type-safe XPath components
     */
    static std::shared_ptr<XPathContext> createContext(xmlDoc *doc) {
        return std::make_shared<XPathContext>(doc);
    }

    static std::shared_ptr<XPathEvaluator> createEvaluator(std::shared_ptr<XPathContext> context) {
        return std::make_shared<XPathEvaluator>(context);
    }

    static std::shared_ptr<XPathVariable> createStringVariable(const std::string &name, const std::string &value) {
        return std::make_shared<XPathVariable>(name, value);
    }

    static std::shared_ptr<XPathVariable> createNumberVariable(const std::string &name, double value) {
        return std::make_shared<XPathVariable>(name, value);
    }

    static std::shared_ptr<XPathVariable> createBooleanVariable(const std::string &name, bool value) {
        return std::make_shared<XPathVariable>(name, value);
    }
};

}  // namespace Common

}  // namespace SCXML

#endif  // TYPESAFEXPATH_H