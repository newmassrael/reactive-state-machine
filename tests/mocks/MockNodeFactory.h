#pragma once

#include <gmock/gmock.h>
#include "INodeFactory.h"
#include "IStateNode.h"
#include "ITransitionNode.h"
#include "IGuardNode.h"
#include "IActionNode.h"
#include "IDataModelItem.h"
#include "IInvokeNode.h"
#include <memory>
#include <string>

class MockNodeFactory : public INodeFactory
{
public:
    MOCK_METHOD2(createStateNode, std::shared_ptr<IStateNode>(const std::string &, const Type));
    MOCK_METHOD2(createTransitionNode, std::shared_ptr<ITransitionNode>(const std::string &, const std::string &));
    MOCK_METHOD2(createGuardNode, std::shared_ptr<IGuardNode>(const std::string &, const std::string &));
    MOCK_METHOD1(createActionNode, std::shared_ptr<IActionNode>(const std::string &));
    MOCK_METHOD2(createDataModelItem, std::shared_ptr<IDataModelItem>(const std::string &, const std::string &));
    MOCK_METHOD1(createInvokeNode, std::shared_ptr<IInvokeNode>(const std::string &));
};
