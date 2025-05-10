// InvokeNode.cpp
#include "InvokeNode.h"
#include "Logger.h"

InvokeNode::InvokeNode(const std::string &id)
    : id_(id), autoForward_(false)
{
    Logger::debug("InvokeNode::Constructor - Creating invoke node: " + id);
}

InvokeNode::~InvokeNode()
{
    Logger::debug("InvokeNode::Destructor - Destroying invoke node: " + id_);
}

const std::string &InvokeNode::getId() const
{
    return id_;
}

const std::string &InvokeNode::getType() const
{
    return type_;
}

const std::string &InvokeNode::getSrc() const
{
    return src_;
}

bool InvokeNode::isAutoForward() const
{
    return autoForward_;
}

void InvokeNode::setType(const std::string &type)
{
    Logger::debug("InvokeNode::setType() - Setting type for " + id_ + ": " + type);
    type_ = type;
}

void InvokeNode::setSrc(const std::string &src)
{
    Logger::debug("InvokeNode::setSrc() - Setting src for " + id_ + ": " + src);
    src_ = src;
}

void InvokeNode::setAutoForward(bool autoForward)
{
    Logger::debug("InvokeNode::setAutoForward() - Setting autoForward for " + id_ + ": " +
                  (autoForward ? "true" : "false"));
    autoForward_ = autoForward;
}

void InvokeNode::setIdLocation(const std::string &idLocation)
{
    Logger::debug("InvokeNode::setIdLocation() - Setting idLocation for " + id_ + ": " + idLocation);
    idLocation_ = idLocation;
}

void InvokeNode::setNamelist(const std::string &namelist)
{
    Logger::debug("InvokeNode::setNamelist() - Setting namelist for " + id_ + ": " + namelist);
    namelist_ = namelist;
}

void InvokeNode::addParam(const std::string &name, const std::string &expr, const std::string &location)
{
    Logger::debug("InvokeNode::addParam() - Adding param to " + id_ + ": name=" + name);
    params_.push_back(std::make_tuple(name, expr, location));
}

void InvokeNode::setContent(const std::string &content)
{
    Logger::debug("InvokeNode::setContent() - Setting content for " + id_);
    content_ = content;
}

void InvokeNode::setFinalize(const std::string &finalizeContent)
{
    Logger::debug("InvokeNode::setFinalize() - Setting finalize for " + id_);
    finalize_ = finalizeContent;
}

const std::string &InvokeNode::getIdLocation() const
{
    return idLocation_;
}

const std::string &InvokeNode::getNamelist() const
{
    return namelist_;
}

const std::string &InvokeNode::getContent() const
{
    return content_;
}

const std::string &InvokeNode::getFinalize() const
{
    return finalize_;
}

const std::vector<std::tuple<std::string, std::string, std::string>> &InvokeNode::getParams() const
{
    return params_;
}
