// SCXMLContext.cpp
#include "SCXMLContext.h"
using namespace RSM;


void SCXMLContext::setDatamodelType(const std::string &datamodelType)
{
    datamodelType_ = datamodelType;
}


const std::string &SCXMLContext::getDatamodelType() const
{
    return datamodelType_;
}


void SCXMLContext::setBinding(const std::string &binding)
{
    binding_ = binding;
}


const std::string &SCXMLContext::getBinding() const
{
    return binding_;
}


void SCXMLContext::addNamespace(const std::string &prefix, const std::string &uri)
{
    namespaces_[prefix] = uri;
}


const std::string &SCXMLContext::getNamespaceURI(const std::string &prefix) const
{
    auto it = namespaces_.find(prefix);
    if (it != namespaces_.end())
    {
        return it->second;
    }
    return emptyString_;
}


void SCXMLContext::setAttribute(const std::string &name, const std::string &value)
{
    attributes_[name] = value;
}


const std::string &SCXMLContext::getAttribute(const std::string &name) const
{
    auto it = attributes_.find(name);
    if (it != attributes_.end())
    {
        return it->second;
    }
    return emptyString_;
}
