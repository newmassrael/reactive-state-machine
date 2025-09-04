#include "model/ParsingContext.h"

namespace SCXML {
namespace Model {

void ParsingContext::setDatamodelType(const std::string &datamodelType) {
    datamodelType_ = datamodelType;
}

const std::string &ParsingContext::getDatamodelType() const {
    return datamodelType_;
}

void ParsingContext::setBinding(const std::string &binding) {
    binding_ = binding;
}

const std::string &ParsingContext::getBinding() const {
    return binding_;
}

void ParsingContext::addNamespace(const std::string &prefix, const std::string &uri) {
    namespaces_[prefix] = uri;
}

const std::string &ParsingContext::getNamespaceURI(const std::string &prefix) const {
    auto it = namespaces_.find(prefix);
    if (it != namespaces_.end()) {
        return it->second;
    }
    return emptyString_;
}

void ParsingContext::setAttribute(const std::string &name, const std::string &value) {
    attributes_[name] = value;
}

const std::string &ParsingContext::getAttribute(const std::string &name) const {
    auto it = attributes_.find(name);
    if (it != attributes_.end()) {
        return it->second;
    }
    return emptyString_;
}

}  // namespace Model
}  // namespace SCXML