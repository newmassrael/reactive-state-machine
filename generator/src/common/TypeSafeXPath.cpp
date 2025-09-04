#include "common/TypeSafeXPath.h"
#include "common/Logger.h"

#ifdef HAVE_LIBXML2
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#endif

namespace SCXML {
namespace Common {

// XPathContext implementation
TypeSafeXPath::XPathContext::XPathContext(xmlDoc *doc) : context_(nullptr) {
#ifdef HAVE_LIBXML2
    context_ = xmlXPathNewContext(doc);
    if (context_) {
        // Register our variable resolver
        xmlXPathRegisterVariableLookup(context_, variableResolver, this);
    }
#endif
}

TypeSafeXPath::XPathContext::~XPathContext() {
#ifdef HAVE_LIBXML2
    if (context_) {
        xmlXPathFreeContext(context_);
    }
#endif
}

void TypeSafeXPath::XPathContext::setVariable(std::shared_ptr<XPathVariable> variable) {
    if (variable) {
        variables_[variable->getName()] = variable;
    }
}

std::shared_ptr<TypeSafeXPath::XPathVariable> TypeSafeXPath::XPathContext::getVariable(const std::string &name) const {
    auto it = variables_.find(name);
    return (it != variables_.end()) ? it->second : nullptr;
}

void TypeSafeXPath::XPathContext::removeVariable(const std::string &name) {
    variables_.erase(name);
}

xmlXPathObject *TypeSafeXPath::XPathContext::variableResolver(void *data, const unsigned char *name,
                                                              const unsigned char *nsUri) {
#ifdef HAVE_LIBXML2
    auto *self = static_cast<XPathContext *>(data);
    if (!self || !name) {
        return nullptr;
    }

    std::string varName(reinterpret_cast<const char *>(name));
    auto variable = self->getVariable(varName);

    if (!variable) {
        return nullptr;
    }

    switch (variable->getType()) {
    case XPathVariable::Type::STRING:
        return xmlXPathNewString(reinterpret_cast<const xmlChar *>(variable->getStringValue().c_str()));
    case XPathVariable::Type::NUMBER:
        return xmlXPathNewFloat(variable->getNumberValue());
    case XPathVariable::Type::BOOLEAN:
        return xmlXPathNewBoolean(variable->getBooleanValue() ? 1 : 0);
    default:
        return nullptr;
    }
#else
    return nullptr;
#endif
}

void TypeSafeXPath::XPathContext::freeXPathVariable(xmlXPathObject *obj) {
#ifdef HAVE_LIBXML2
    if (obj) {
        xmlXPathFreeObject(obj);
    }
#endif
}

// XPathObjectGuard implementation
TypeSafeXPath::XPathObjectGuard::~XPathObjectGuard() {
#ifdef HAVE_LIBXML2
    if (object_) {
        xmlXPathFreeObject(object_);
    }
#endif
}

// XPathEvaluator implementation
std::string TypeSafeXPath::XPathEvaluator::evaluateString(const std::string &expression) {
#ifdef HAVE_LIBXML2
    if (!context_ || !context_->getContext()) {
        Logger::error("XPath context not initialized");
        return "";
    }

    auto obj = evaluateExpression(expression);
    if (!obj.get()) {
        return "";
    }

    xmlChar *str = xmlXPathCastToString(obj.get());
    if (!str) {
        return "";
    }

    std::string result(reinterpret_cast<char *>(str));
    xmlFree(str);
    return result;
#else
    Logger::warning("XPath evaluation not available (libxml2 not found)");
    return "";
#endif
}

double TypeSafeXPath::XPathEvaluator::evaluateNumber(const std::string &expression) {
#ifdef HAVE_LIBXML2
    if (!context_ || !context_->getContext()) {
        Logger::error("XPath context not initialized");
        return 0.0;
    }

    auto obj = evaluateExpression(expression);
    if (!obj.get()) {
        return 0.0;
    }

    return xmlXPathCastToNumber(obj.get());
#else
    Logger::warning("XPath evaluation not available (libxml2 not found)");
    return 0.0;
#endif
}

bool TypeSafeXPath::XPathEvaluator::evaluateBoolean(const std::string &expression) {
#ifdef HAVE_LIBXML2
    if (!context_ || !context_->getContext()) {
        Logger::error("XPath context not initialized");
        return false;
    }

    auto obj = evaluateExpression(expression);
    if (!obj.get()) {
        return false;
    }

    return xmlXPathCastToBoolean(obj.get()) != 0;
#else
    Logger::warning("XPath evaluation not available (libxml2 not found)");
    return false;
#endif
}

TypeSafeXPath::XPathObjectGuard TypeSafeXPath::XPathEvaluator::evaluateExpression(const std::string &expression) {
#ifdef HAVE_LIBXML2
    if (!context_ || !context_->getContext()) {
        Logger::error("XPath context not initialized");
        return XPathObjectGuard(nullptr);
    }

    xmlChar *xpathExpr = xmlCharStrdup(expression.c_str());
    xmlXPathObject *result = xmlXPathEvalExpression(xpathExpr, context_->getContext());
    xmlFree(xpathExpr);

    if (!result) {
        Logger::error("XPath expression evaluation failed: " + expression);
    }

    return XPathObjectGuard(result);
#else
    Logger::warning("XPath evaluation not available (libxml2 not found)");
    return XPathObjectGuard(nullptr);
#endif
}

}  // namespace Common
}  // namespace SCXML