// SCXMLContext.cpp
#include "SCXMLContext.h"

void RSM::SCXMLContext::setDatamodelType(const std::string &datamodelType) {
    datamodelType_ = datamodelType;
}

const std::string &RSM::SCXMLContext::getDatamodelType() const {
    return datamodelType_;
}

void RSM::SCXMLContext::setBinding(const std::string &binding) {
    binding_ = binding;
}

const std::string &RSM::SCXMLContext::getBinding() const {
    return binding_;
}

void RSM::SCXMLContext::addNamespace(const std::string &prefix, const std::string &uri) {
    namespaces_[prefix] = uri;
}

const std::string &RSM::SCXMLContext::getNamespaceURI(const std::string &prefix) const {
    auto it = namespaces_.find(prefix);
    if (it != namespaces_.end()) {
        return it->second;
    }
    return emptyString_;
}

void RSM::SCXMLContext::setAttribute(const std::string &name, const std::string &value) {
    attributes_[name] = value;
}

const std::string &RSM::SCXMLContext::getAttribute(const std::string &name) const {
    auto it = attributes_.find(name);
    if (it != attributes_.end()) {
        return it->second;
    }
    return emptyString_;
}
