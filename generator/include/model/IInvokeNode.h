#pragma once

#include <string>
#include <memory>
#include <vector>

class IInvokeNode
{
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
    virtual const std::vector<std::tuple<std::string, std::string, std::string>> &getParams() const = 0;
    virtual const std::string &getContent() const = 0;
    virtual const std::string &getFinalize() const = 0;
};
