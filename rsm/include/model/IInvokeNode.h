#pragma once

#include <memory>
#include <string>
#include <vector>

namespace RSM {

class IInvokeNode {
public:
    virtual ~IInvokeNode() = default;

    virtual const std::string &getId() const = 0;
    virtual const std::string &getType() const = 0;
    virtual const std::string &getSrc() const = 0;
    virtual bool isAutoForward() const = 0;
    virtual void setType(const std::string &type) = 0;
    virtual void setSrc(const std::string &src) = 0;
    virtual void setIdLocation(const std::string &idLocation) = 0;
    virtual void setNamelist(const std::string &namelist) = 0;
    virtual void setAutoForward(bool autoForward) = 0;
    virtual void addParam(const std::string &name, const std::string &expr, const std::string &location) = 0;
    virtual void setContent(const std::string &content) = 0;
    virtual void setFinalize(const std::string &finalize) = 0;
    virtual const std::string &getIdLocation() const = 0;
    virtual const std::string &getNamelist() const = 0;
    virtual const std::vector<std::tuple<std::string, std::string, std::string>> &getParams() const = 0;
    virtual const std::string &getContent() const = 0;
    virtual const std::string &getFinalize() const = 0;

    // W3C SCXML 1.0: typeexpr attribute support for dynamic type evaluation
    virtual void setTypeExpr(const std::string &typeExpr) = 0;
    virtual const std::string &getTypeExpr() const = 0;

    // W3C SCXML 1.0: srcexpr attribute support for dynamic source evaluation
    virtual void setSrcExpr(const std::string &srcExpr) = 0;
    virtual const std::string &getSrcExpr() const = 0;

    // W3C SCXML test 530: content expr attribute support for dynamic content evaluation
    virtual void setContentExpr(const std::string &contentExpr) = 0;
    virtual const std::string &getContentExpr() const = 0;

    // W3C SCXML 6.4: State ID for invoke ID generation in "stateid.platformid" format (test 224)
    virtual void setStateId(const std::string &stateId) = 0;
    virtual const std::string &getStateId() const = 0;
};

}  // namespace RSM