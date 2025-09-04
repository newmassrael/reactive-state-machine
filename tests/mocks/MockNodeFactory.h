#pragma once

#include <gmock/gmock.h>
#include <memory>
#include "model/INodeFactory.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "model/IGuardNode.h"
#include "model/IActionNode.h"
#include "model/IDataModelItem.h"
#include "model/IInvokeNode.h"
#include "core/types.h"

class MockNodeFactory : public SCXML::Model::INodeFactory
{
public:
    MOCK_METHOD2(createStateNode, std::shared_ptr<SCXML::Model::IStateNode>(const std::string &, const SCXML::Type));
    MOCK_METHOD2(createTransitionNode, std::shared_ptr<SCXML::Model::ITransitionNode>(const std::string &, const std::string &));
    MOCK_METHOD2(createGuardNode, std::shared_ptr<SCXML::Model::IGuardNode>(const std::string &, const std::string &));
    MOCK_METHOD1(createActionNode, std::shared_ptr<SCXML::Model::IActionNode>(const std::string &));
    MOCK_METHOD2(createDataModelItem, std::shared_ptr<SCXML::Model::IDataModelItem>(const std::string &, const std::string &));
    MOCK_METHOD1(createInvokeNode, std::shared_ptr<SCXML::Model::IInvokeNode>(const std::string &));
};
