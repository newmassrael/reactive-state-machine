// InvokeNode.cpp
#include "InvokeNode.h"
#include "common/Logger.h"

RSM::InvokeNode::InvokeNode(const std::string &id) : id_(id), autoForward_(false) {
    LOG_DEBUG("Creating invoke node: {}", id);
}

RSM::InvokeNode::~InvokeNode() {
    LOG_DEBUG("Destroying invoke node: {}", id_);
}

const std::string &RSM::InvokeNode::getId() const {
    return id_;
}

const std::string &RSM::InvokeNode::getType() const {
    return type_;
}

const std::string &RSM::InvokeNode::getSrc() const {
    return src_;
}

bool RSM::InvokeNode::isAutoForward() const {
    return autoForward_;
}

void RSM::InvokeNode::setType(const std::string &type) {
    LOG_DEBUG("Setting type for {}: {}", id_, type);
    type_ = type;
}

void RSM::InvokeNode::setSrc(const std::string &src) {
    LOG_DEBUG("Setting src for {}: {}", id_, src);
    src_ = src;
}

void RSM::InvokeNode::setAutoForward(bool autoForward) {
    LOG_DEBUG("Setting autoForward for {}: {}", id_, (autoForward ? "true" : "false"));
    autoForward_ = autoForward;
}

void RSM::InvokeNode::setIdLocation(const std::string &idLocation) {
    LOG_DEBUG("Setting idLocation for {}: {}", id_, idLocation);
    idLocation_ = idLocation;
}

void RSM::InvokeNode::setNamelist(const std::string &namelist) {
    LOG_DEBUG("Setting namelist for {}: {}", id_, namelist);
    namelist_ = namelist;
}

void RSM::InvokeNode::addParam(const std::string &name, const std::string &expr, const std::string &location) {
    LOG_DEBUG("Adding param to {}: name={}", id_, name);
    params_.push_back(std::make_tuple(name, expr, location));
}

void RSM::InvokeNode::setContent(const std::string &content) {
    LOG_DEBUG("Setting content for {}", id_);
    content_ = content;
}

void RSM::InvokeNode::setFinalize(const std::string &finalizeContent) {
    LOG_DEBUG("Setting finalize for {}", id_);
    finalize_ = finalizeContent;
}

const std::string &RSM::InvokeNode::getIdLocation() const {
    return idLocation_;
}

const std::string &RSM::InvokeNode::getNamelist() const {
    return namelist_;
}

const std::string &RSM::InvokeNode::getContent() const {
    return content_;
}

const std::string &RSM::InvokeNode::getFinalize() const {
    return finalize_;
}

const std::vector<std::tuple<std::string, std::string, std::string>> &RSM::InvokeNode::getParams() const {
    return params_;
}

// W3C SCXML 1.0: typeexpr attribute support for dynamic type evaluation
void RSM::InvokeNode::setTypeExpr(const std::string &typeExpr) {
    typeExpr_ = typeExpr;
    LOG_DEBUG("InvokeNode: Set typeexpr to '{}'", typeExpr);
}

const std::string &RSM::InvokeNode::getTypeExpr() const {
    return typeExpr_;
}

// W3C SCXML 1.0: srcexpr attribute support for dynamic source evaluation
void RSM::InvokeNode::setSrcExpr(const std::string &srcExpr) {
    srcExpr_ = srcExpr;
    LOG_DEBUG("InvokeNode: Set srcexpr to '{}'", srcExpr);
}

const std::string &RSM::InvokeNode::getSrcExpr() const {
    return srcExpr_;
}
