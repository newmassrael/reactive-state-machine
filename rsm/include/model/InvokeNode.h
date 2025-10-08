// InvokeNode.h
#pragma once

#include "IInvokeNode.h"
#include <memory>
#include <string>
#include <vector>

namespace RSM {

class InvokeNode : public IInvokeNode {
public:
    InvokeNode(const std::string &id);
    virtual ~InvokeNode();

    // IInvokeNode 인터페이스 구현
    virtual const std::string &getId() const override;
    virtual const std::string &getType() const override;
    virtual const std::string &getSrc() const override;
    virtual bool isAutoForward() const override;

    // 설정자 메서드들
    void setType(const std::string &type) override;
    void setSrc(const std::string &src) override;
    void setAutoForward(bool autoForward) override;
    void setIdLocation(const std::string &idLocation) override;
    void setNamelist(const std::string &namelist) override;

    // 파라미터 및 콘텐츠 관련 메서드들
    void addParam(const std::string &name, const std::string &expr, const std::string &location) override;
    void setContent(const std::string &content) override;
    void setFinalize(const std::string &finalizeContent) override;

    // 추가 접근자 메서드들
    const std::string &getIdLocation() const override;
    const std::string &getNamelist() const;
    const std::string &getContent() const override;
    const std::string &getFinalize() const override;
    const std::vector<std::tuple<std::string, std::string, std::string>> &getParams() const override;

    // W3C SCXML 1.0: typeexpr attribute support for dynamic type evaluation
    void setTypeExpr(const std::string &typeExpr) override;
    const std::string &getTypeExpr() const override;

    // W3C SCXML 1.0: srcexpr attribute support for dynamic source evaluation
    void setSrcExpr(const std::string &srcExpr) override;
    const std::string &getSrcExpr() const override;

    // W3C SCXML test 530: content expr attribute support for dynamic content evaluation
    void setContentExpr(const std::string &contentExpr) override;
    const std::string &getContentExpr() const override;

    // W3C SCXML 6.4: State ID for invoke ID generation (test 224)
    void setStateId(const std::string &stateId) override;
    const std::string &getStateId() const override;

private:
    std::string id_;
    std::string type_;
    std::string src_;
    std::string idLocation_;
    std::string namelist_;
    std::string content_;
    std::string finalize_;
    std::string typeExpr_;
    std::string srcExpr_;
    std::string contentExpr_;  // W3C SCXML test 530: expr attribute for content element
    std::string stateId_;      // W3C SCXML 6.4: Parent state ID for invoke ID generation (test 224)
    bool autoForward_;
    std::vector<std::tuple<std::string, std::string, std::string>> params_;  // name, expr, location
};

}  // namespace RSM