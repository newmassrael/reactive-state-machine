#include "model/SCXMLModel.h"
#include "model/ITransitionNode.h"
#include "common/Logger.h"
#include <algorithm>
#include <iostream>
#include <algorithm>
#include <unordered_set>
using namespace RSM;




SCXMLModel::SCXMLModel()
    : rootState_(nullptr)
{
    Logger::debug("SCXMLModel::Constructor - Creating SCXML model");
}


SCXMLModel::~SCXMLModel()
{
    Logger::debug("SCXMLModel::Destructor - Destroying SCXML model");
    // 스마트 포인터가 자원 정리를 담당
}


void SCXMLModel::setRootState(std::shared_ptr<IStateNode> rootState)
{
    Logger::debug("SCXMLModel::setRootState() - Setting root state: " +
                  (rootState ? rootState->getId() : "null"));
    rootState_ = rootState;
}


IStateNode *SCXMLModel::getRootState() const
{
    return rootState_.get();
}


void SCXMLModel::setName(const std::string &name)
{
    name_ = name;
}


const std::string &SCXMLModel::getName() const
{
    return name_;
}


void SCXMLModel::setInitialState(const std::string &initialState)
{
    Logger::debug("SCXMLModel::setInitialState() - Setting initial state: " + initialState);
    initialState_ = initialState;
}


const std::string &SCXMLModel::getInitialState() const
{
    return initialState_;
}


void SCXMLModel::setDatamodel(const std::string &datamodel)
{
    Logger::debug("SCXMLModel::setDatamodel() - Setting datamodel: " + datamodel);
    datamodel_ = datamodel;
}


const std::string &SCXMLModel::getDatamodel() const
{
    return datamodel_;
}


void SCXMLModel::addContextProperty(const std::string &name, const std::string &type)
{
    Logger::debug("SCXMLModel::addContextProperty() - Adding context property: " + name + " (" + type + ")");
    contextProperties_[name] = type;
}


const std::unordered_map<std::string, std::string> &SCXMLModel::getContextProperties() const
{
    return contextProperties_;
}


void SCXMLModel::addInjectPoint(const std::string &name, const std::string &type)
{
    Logger::debug("SCXMLModel::addInjectPoint() - Adding inject point: " + name + " (" + type + ")");
    injectPoints_[name] = type;
}


const std::unordered_map<std::string, std::string> &SCXMLModel::getInjectPoints() const
{
    return injectPoints_;
}


void SCXMLModel::addGuard(std::shared_ptr<IGuardNode> guard)
{
    if (guard)
    {
        Logger::debug("SCXMLModel::addGuard() - Adding guard: " + guard->getId());
        guards_.push_back(guard);
    }
}


const std::vector<std::shared_ptr<IGuardNode>> &SCXMLModel::getGuards() const
{
    return guards_;
}


void SCXMLModel::addState(std::shared_ptr<IStateNode> state)
{
    if (state)
    {
        Logger::debug("SCXMLModel::addState() - Adding state: " + state->getId());
        allStates_.push_back(state);
        stateIdMap_[state->getId()] = state.get();
    }
}


const std::vector<std::shared_ptr<IStateNode>> &SCXMLModel::getAllStates() const
{
    return allStates_;
}


IStateNode *SCXMLModel::findStateById(const std::string &id) const
{
    // 먼저 맵에서 찾기
    auto it = stateIdMap_.find(id);
    if (it != stateIdMap_.end())
    {
        return it->second;
    }


    // 맵에 없으면 모든 최상위 상태를 검색
    std::set<std::string> visitedStates; // 이미 방문한 상태 ID 추적
    for (const auto &state : allStates_)
    {
        if (state->getId() == id)
            return state.get();


        IStateNode *result = findStateByIdRecursive(state.get(), id, visitedStates);
        if (result)
            return result;
    }


    return nullptr;
}


IStateNode *SCXMLModel::findStateByIdRecursive(IStateNode *state, const std::string &id, std::set<std::string> &visitedStates) const
{
    if (!state)
        return nullptr;


    // 이미 방문한 상태는 건너뛰기
    if (visitedStates.find(state->getId()) != visitedStates.end())
        return nullptr;


    visitedStates.insert(state->getId());


    // 현재 상태 확인
    if (state->getId() == id)
    {
        return state;
    }


    // 자식 상태 검색
    for (const auto &child : state->getChildren())
    {
        IStateNode *result = findStateByIdRecursive(child.get(), id, visitedStates);
        if (result)
        {
            return result;
        }
    }


    return nullptr;
}


void SCXMLModel::addDataModelItem(std::shared_ptr<IDataModelItem> dataItem)
{
    if (dataItem)
    {
        Logger::debug("SCXMLModel::addDataModelItem() - Adding data model item: " + dataItem->getId());
        dataModelItems_.push_back(dataItem);
    }
}


const std::vector<std::shared_ptr<IDataModelItem>> &SCXMLModel::getDataModelItems() const
{
    return dataModelItems_;
}


bool SCXMLModel::validateStateRelationships() const
{
    Logger::info("SCXMLModel::validateStateRelationships() - Validating state relationships");


    // 모든 상태에 대해 검증
    for (const auto &state : allStates_)
    {
        // 부모 상태 검증
        IStateNode *parent = state->getParent();
        if (parent)
        {
            // 부모가 실제로 이 상태를 자식으로 가지고 있는지 확인
            bool foundAsChild = false;
            for (const auto &childState : parent->getChildren())
            {
                if (childState.get() == state.get())
                {
                    foundAsChild = true;
                    break;
                }
            }


            if (!foundAsChild)
            {
                Logger::error("SCXMLModel::validateStateRelationships() - State '" + state->getId() +
                              "' has parent '" + parent->getId() + "' but is not in parent's children list");
                return false;
            }
        }


        // 모든 전환의 타겟 상태가 존재하는지 확인
        for (const auto &transition : state->getTransitions())
        {
            const auto targets = transition->getTargets();
            for (const auto &target : targets)
            {
                IStateNode *targetState = findStateById(target);
                if (!targetState)
                {
                    Logger::error("SCXMLModel::validateStateRelationships() - Transition in state '" +
                                  state->getId() + "' references non-existent target state '" +
                                  target + "'");
                    return false;
                }
            }
        }


        // 초기 상태가 존재하는지 확인
        if (!state->getInitialState().empty())
        {
            if (state->getChildren().empty())
            {
                Logger::warning("SCXMLModel::validateStateRelationships() - State '" + state->getId() +
                                "' has initialState but no children");
            }
            else
            {
                bool initialStateExists = false;
                for (const auto &child : state->getChildren())
                {
                    if (child->getId() == state->getInitialState())
                    {
                        initialStateExists = true;
                        break;
                    }
                }


                if (!initialStateExists)
                {
                    Logger::error("SCXMLModel::validateStateRelationships() - State '" + state->getId() +
                                  "' references non-existent initial state '" + state->getInitialState() + "'");
                    return false;
                }
            }
        }
    }


    Logger::info("SCXMLModel::validateStateRelationships() - All state relationships are valid");
    return true;
}


std::vector<std::string> SCXMLModel::findMissingStateIds() const
{
    Logger::info("SCXMLModel::findMissingStateIds() - Looking for missing state IDs");


    std::vector<std::string> missingIds;
    std::unordered_set<std::string> existingIds;


    // 모든 상태 ID 수집
    for (const auto &state : allStates_)
    {
        existingIds.insert(state->getId());
    }


    // 참조된 상태 ID 확인
    for (const auto &state : allStates_)
    {
        // 초기 상태 확인
        if (!state->getInitialState().empty() && existingIds.find(state->getInitialState()) == existingIds.end())
        {
            missingIds.push_back(state->getInitialState());
            Logger::warning("SCXMLModel::findMissingStateIds() - Missing state ID referenced as initial state: " +
                            state->getInitialState());
        }


        // 전환 타겟 확인
        for (const auto &transition : state->getTransitions())
        {
            const auto targets = transition->getTargets();
            for (const auto &target : targets)
            {
                if (!target.empty() && existingIds.find(target) == existingIds.end())
                {
                    missingIds.push_back(target);
                    Logger::warning("SCXMLModel::findMissingStateIds() - Missing state ID referenced as transition target: " +
                                    target);
                }
            }
        }
    }


    // 중복 제거
    std::sort(missingIds.begin(), missingIds.end());
    missingIds.erase(std::unique(missingIds.begin(), missingIds.end()), missingIds.end());


    Logger::info("SCXMLModel::findMissingStateIds() - Found " + std::to_string(missingIds.size()) + " missing state IDs");
    return missingIds;
}


void SCXMLModel::printModelStructure() const
{
    Logger::info("SCXMLModel::printModelStructure() - Printing model structure");
    std::cout << "SCXML Model Structure:\n";
    std::cout << "======================\n";
    std::cout << "Initial State: " << initialState_ << "\n";
    std::cout << "Datamodel: " << datamodel_ << "\n\n";


    std::cout << "Context Properties:\n";
    for (const auto &[name, type] : contextProperties_)
    {
        std::cout << "  " << name << ": " << type << "\n";
    }


    std::cout << "\nInject Points:\n";
    for (const auto &[name, type] : injectPoints_)
    {
        std::cout << "  " << name << ": " << type << "\n";
    }


    std::cout << "\nGuards:\n";
    for (const auto &guard : guards_)
    {
        std::cout << "  " << guard->getId() << ":\n";


        if (!guard->getCondition().empty())
        {
            std::cout << "    Condition: " << guard->getCondition() << "\n";
        }


        if (!guard->getTargetState().empty())
        {
            std::cout << "    Target State: " << guard->getTargetState() << "\n";
        }


        std::cout << "    Dependencies:\n";
        for (const auto &dep : guard->getDependencies())
        {
            std::cout << "      " << dep << "\n";
        }


        if (!guard->getExternalClass().empty())
        {
            std::cout << "    External Class: " << guard->getExternalClass() << "\n";
        }


        if (guard->isReactive())
        {
            std::cout << "    Reactive: Yes\n";
        }
    }


    std::cout << "\nState Hierarchy:\n";
    if (rootState_)
    {
        printStateHierarchy(rootState_.get(), 0);
    }


    Logger::info("SCXMLModel::printModelStructure() - Model structure printed");
}


void SCXMLModel::printStateHierarchy(IStateNode *state, int depth) const
{
    if (!state)
        return;


    // 들여쓰기 생성
    std::string indent(depth * 2, ' ');


    // 현재 상태 정보 출력
    std::cout << indent << "State: " << state->getId() << std::endl;


    // 자식 상태 재귀적으로 출력
    for (const auto &child : state->getChildren())
    {
        printStateHierarchy(child.get(), depth + 1);
    }
}


void SCXMLModel::setBinding(const std::string &binding)
{
    Logger::debug("SCXMLModel::setBinding() - Setting binding mode: " + binding);
    binding_ = binding;
}


const std::string &SCXMLModel::getBinding() const
{
    return binding_;
}


void SCXMLModel::addSystemVariable(std::shared_ptr<IDataModelItem> systemVar)
{
    if (systemVar)
    {
        Logger::debug("SCXMLModel::addSystemVariable() - Adding system variable: " + systemVar->getId());
        systemVariables_.push_back(systemVar);
    }
}


const std::vector<std::shared_ptr<IDataModelItem>> &SCXMLModel::getSystemVariables() const
{
    return systemVariables_;
}
