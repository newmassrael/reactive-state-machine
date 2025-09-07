#include "core/FinalNode.h"
#include "common/Logger.h"
#include <sstream>

namespace SCXML {
namespace Core {

FinalNode::FinalNode(const std::string &id) : StateNode(id, Type::FINAL) {
    SCXML::Common::Logger::debug("FinalNode::Constructor - Creating final state: " + id);
}

void FinalNode::setDoneData(std::shared_ptr<DoneDataNode> doneData) {
    std::lock_guard<std::mutex> lock(finalMutex_);
    doneData_ = doneData;
    SCXML::Common::Logger::debug("FinalNode::setDoneData - Set done data for final state: " + getId());
}

std::shared_ptr<DoneDataNode> FinalNode::getDoneDataNode() const {
    std::lock_guard<std::mutex> lock(finalMutex_);
    return doneData_;
}

SCXML::Common::Result<void> FinalNode::enter(SCXML::Model::IExecutionContext &context) {
    SCXML::Common::Logger::debug("FinalNode::enter - Entering final state: " + getId());

    try {
        // Generate done.state.{id} event
        auto doneResult = generateDoneEvent(context);
        if (doneResult.isFailure()) {
            auto errors = doneResult.getErrors();
            std::string errorMsg = errors.empty() ? "Unknown error" : errors[0].message;
            SCXML::Common::Logger::warning("FinalNode::enter - Failed to generate done event: " + errorMsg);
            // Continue - event generation failure doesn't prevent final state entry
        }

        // Check if parent state is now complete
        checkParentCompletion(context);

        SCXML::Common::Logger::debug("FinalNode::enter - Successfully entered final state: " + getId());
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        std::string error = "FinalNode::enter - Exception during final state entry: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        return SCXML::Common::Result<void>::failure(error);
    }
}

SCXML::Common::Result<void> FinalNode::exit(SCXML::Model::IExecutionContext &/*context*/) {
    SCXML::Common::Logger::debug("FinalNode::exit - Exiting final state: " + getId());

    try {
        // Note: Final states typically don't have exit actions in SCXML
        // but we support them for completeness. Action execution will be
        // handled by the state machine processor with proper context conversion.
        
        SCXML::Common::Logger::debug("FinalNode::exit - Successfully exited final state: " + getId());
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        std::string error = "FinalNode::exit - Exception during final state exit: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        return SCXML::Common::Result<void>::failure(error);
    }
}

std::vector<std::string> FinalNode::validate() const {
    std::vector<std::string> errors;

    // SCXML W3C 사양: Final 상태는 전환을 가질 수 없음
    if (!getTransitions().empty()) {
        errors.push_back("Final state '" + getId() + "' cannot have transitions (SCXML specification violation)");
    }

    // SCXML W3C 사양: Final 상태는 자식 상태를 가질 수 없음
    if (!getChildren().empty()) {
        errors.push_back("Final state '" + getId() + "' cannot have child states (SCXML specification violation)");
    }

    // SCXML W3C 사양: Final 상태는 invoke를 가질 수 없음
    if (!getInvoke().empty()) {
        errors.push_back("Final state '" + getId() + "' cannot have invoke elements (SCXML specification violation)");
    }

    // Done data validation
    if (doneData_) {
        auto doneDataErrors = doneData_->validate();
        for (const auto &error : doneDataErrors) {
            errors.push_back("Final state '" + getId() + "' done data error: " + error);
        }
    }

    return errors;
}

std::shared_ptr<IStateNode> FinalNode::clone() const {
    auto cloned = std::make_shared<FinalNode>(getId());
    
    // Clone done data if present
    if (doneData_) {
        auto clonedDoneData = std::dynamic_pointer_cast<DoneDataNode>(doneData_->clone());
        cloned->setDoneData(clonedDoneData);
    }

    // Clone entry/exit actions (inherited from StateNode)
    for (const auto &action : getEntryActionNodes()) {
        if (action) {
            cloned->addEntryActionNode(action->clone());
        }
    }

    for (const auto &action : getExitActionNodes()) {
        if (action) {
            cloned->addExitActionNode(action->clone());
        }
    }

    SCXML::Common::Logger::debug("FinalNode::clone - Cloned final state: " + getId());
    return cloned;
}

SCXML::Common::Result<void> FinalNode::generateDoneEvent(SCXML::Model::IExecutionContext &context) {
    try {
        // SCXML W3C 사양: done.state.{id} 이벤트 생성
        std::string eventName = "done.state." + getId();
        std::string eventData;

        // Done data가 있으면 이벤트 데이터로 포함
        if (doneData_) {
            eventData = doneData_->generateDoneData(context);
        }

        // 이벤트 발송
        auto sendResult = context.raiseEvent(eventName, eventData);
        if (sendResult.isFailure()) {
            auto errors = sendResult.getErrors();
            std::string errorMsg = errors.empty() ? "Unknown error" : errors[0].message;
            return SCXML::Common::Result<void>::failure("Failed to raise done event: " + errorMsg);
        }

        SCXML::Common::Logger::debug("FinalNode::generateDoneEvent - Generated done event: " + eventName);
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        std::string error = "FinalNode::generateDoneEvent - Exception: " + std::string(e.what());
        return SCXML::Common::Result<void>::failure(error);
    }
}

void FinalNode::checkParentCompletion(SCXML::Model::IExecutionContext &context) {
    // 부모 상태가 있는 경우, 부모의 완료 상태 확인
    auto parent = getParent();
    if (!parent) {
        // 최상위 final 상태 - 전체 state machine 완료
        SCXML::Common::Logger::info("FinalNode::checkParentCompletion - Top-level final state reached, state machine complete");
        
        // 전체 state machine 완료 이벤트 생성
        try {
            auto sendResult = context.raiseEvent("done.state.scxml", "");
            if (sendResult.isFailure()) {
                SCXML::Common::Logger::warning("FinalNode::checkParentCompletion - Failed to raise scxml completion event");
            }
        } catch (const std::exception &e) {
            SCXML::Common::Logger::warning("FinalNode::checkParentCompletion - Exception raising completion event: " + 
                            std::string(e.what()));
        }
        return;
    }

    // 부모가 compound/parallel 상태인 경우 완료 여부 확인
    try {
        bool parentComplete = false;
        
        if (parent->getType() == Type::PARALLEL) {
            // Parallel 상태: 모든 child region이 final 상태여야 완료
            parentComplete = true;
            for (const auto &sibling : parent->getChildren()) {
                if (sibling && !sibling->isFinalState()) {
                    // 활성 상태인 sibling이 final이 아니면 아직 완료되지 않음
                    if (context.isStateActive(sibling->getId())) {
                        parentComplete = false;
                        break;
                    }
                }
            }
        } else if (parent->getType() == Type::COMPOUND) {
            // Compound 상태: 현재 활성 상태가 final이면 완료
            parentComplete = true; // 현재 final 상태이므로 compound는 완료됨
        }

        if (parentComplete) {
            SCXML::Common::Logger::debug("FinalNode::checkParentCompletion - Parent state completed: " + parent->getId());
            
            // 부모 상태에 대한 done.state.{parent_id} 이벤트 생성
            std::string parentDoneEvent = "done.state." + parent->getId();
            auto sendResult = context.raiseEvent(parentDoneEvent, "");
            if (sendResult.isFailure()) {
                SCXML::Common::Logger::warning("FinalNode::checkParentCompletion - Failed to raise parent done event");
            }
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::warning("FinalNode::checkParentCompletion - Exception checking parent completion: " + 
                        std::string(e.what()));
    }
}

}  // namespace Core
}  // namespace SCXML