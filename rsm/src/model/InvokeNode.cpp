// InvokeNode.cpp
#include "InvokeNode.h"
#include "common/Logger.h"

RSM::InvokeNode::InvokeNode(const std::string &id)
    : id_(id), autoForward_(false) {
  Logger::debug("RSM::InvokeNode::Constructor - Creating invoke node: " + id);
}

RSM::InvokeNode::~InvokeNode() {
  Logger::debug("RSM::InvokeNode::Destructor - Destroying invoke node: " + id_);
}

const std::string &RSM::InvokeNode::getId() const { return id_; }

const std::string &RSM::InvokeNode::getType() const { return type_; }

const std::string &RSM::InvokeNode::getSrc() const { return src_; }

bool RSM::InvokeNode::isAutoForward() const { return autoForward_; }

void RSM::InvokeNode::setType(const std::string &type) {
  Logger::debug("RSM::InvokeNode::setType() - Setting type for " + id_ + ": " +
                type);
  type_ = type;
}

void RSM::InvokeNode::setSrc(const std::string &src) {
  Logger::debug("RSM::InvokeNode::setSrc() - Setting src for " + id_ + ": " +
                src);
  src_ = src;
}

void RSM::InvokeNode::setAutoForward(bool autoForward) {
  Logger::debug("RSM::InvokeNode::setAutoForward() - Setting autoForward for " +
                id_ + ": " + (autoForward ? "true" : "false"));
  autoForward_ = autoForward;
}

void RSM::InvokeNode::setIdLocation(const std::string &idLocation) {
  Logger::debug("RSM::InvokeNode::setIdLocation() - Setting idLocation for " +
                id_ + ": " + idLocation);
  idLocation_ = idLocation;
}

void RSM::InvokeNode::setNamelist(const std::string &namelist) {
  Logger::debug("RSM::InvokeNode::setNamelist() - Setting namelist for " + id_ +
                ": " + namelist);
  namelist_ = namelist;
}

void RSM::InvokeNode::addParam(const std::string &name, const std::string &expr,
                               const std::string &location) {
  Logger::debug("RSM::InvokeNode::addParam() - Adding param to " + id_ +
                ": name=" + name);
  params_.push_back(std::make_tuple(name, expr, location));
}

void RSM::InvokeNode::setContent(const std::string &content) {
  Logger::debug("RSM::InvokeNode::setContent() - Setting content for " + id_);
  content_ = content;
}

void RSM::InvokeNode::setFinalize(const std::string &finalizeContent) {
  Logger::debug("RSM::InvokeNode::setFinalize() - Setting finalize for " + id_);
  finalize_ = finalizeContent;
}

const std::string &RSM::InvokeNode::getIdLocation() const {
  return idLocation_;
}

const std::string &RSM::InvokeNode::getNamelist() const { return namelist_; }

const std::string &RSM::InvokeNode::getContent() const { return content_; }

const std::string &RSM::InvokeNode::getFinalize() const { return finalize_; }

const std::vector<std::tuple<std::string, std::string, std::string>> &
RSM::InvokeNode::getParams() const {
  return params_;
}
