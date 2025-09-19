// INodeFactory.h
#pragma once

#include <memory>
#include <string>

#include "types.h"

// 인터페이스 정의

namespace RSM {

class IStateNode;
class ITransitionNode;
class IGuardNode;
class IActionNode;
class IDataModelItem;
class IInvokeNode;

// 팩토리 인터페이스 정의
class INodeFactory {
public:
    virtual ~INodeFactory() = default;
    virtual std::shared_ptr<IStateNode> createStateNode(const std::string &id, const Type) = 0;
    virtual std::shared_ptr<ITransitionNode> createTransitionNode(const std::string &event,
                                                                  const std::string &target) = 0;
    virtual std::shared_ptr<IGuardNode> createGuardNode(const std::string &id, const std::string &target) = 0;
    virtual std::shared_ptr<IActionNode> createActionNode(const std::string &name) = 0;
    virtual std::shared_ptr<IDataModelItem> createDataModelItem(const std::string &id,
                                                                const std::string &expr = "") = 0;
    virtual std::shared_ptr<IInvokeNode> createInvokeNode(const std::string &id) = 0;
};

}  // namespace RSM