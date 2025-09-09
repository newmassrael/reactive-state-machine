#include "runtime/StateConfiguration.h"
#include "common/Logger.h"
#include "core/types.h"
#include "model/DocumentModel.h"
#include "model/IStateNode.h"
#include <algorithm>
#include <sstream>

namespace SCXML {
namespace Runtime {

StateConfiguration::StateConfiguration() : model_(nullptr) {
    SCXML::Common::Logger::debug("StateConfiguration created");
}

bool StateConfiguration::initialize(std::shared_ptr<Model::DocumentModel> model) {
    if (!model) {
        SCXML::Common::Logger::error("StateConfiguration::initialize - null model provided");
        return false;
    }

    model_ = model;
    activeStates_.clear();

    SCXML::Common::Logger::debug("StateConfiguration initialized with model");
    return true;
}

void StateConfiguration::clear() {
    activeStates_.clear();
    SCXML::Common::Logger::debug("StateConfiguration cleared");
}

// ====== State Management ======

bool StateConfiguration::addState(const std::string &stateId) {
    if (stateId.empty()) {
        return false;
    }

    auto result = activeStates_.insert(stateId);
    if (result.second) {
        SCXML::Common::Logger::debug("StateConfiguration::addState - added state: " + stateId);
    }
    return result.second;
}

bool StateConfiguration::removeState(const std::string &stateId) {
    if (stateId.empty()) {
        return false;
    }

    size_t removed = activeStates_.erase(stateId);
    if (removed > 0) {
        SCXML::Common::Logger::debug("StateConfiguration::removeState - removed state: " + stateId);
    }
    return removed > 0;
}

void StateConfiguration::addStates(const std::vector<std::string> &stateIds) {
    for (const auto &stateId : stateIds) {
        addState(stateId);
    }
}

void StateConfiguration::removeStates(const std::vector<std::string> &stateIds) {
    for (const auto &stateId : stateIds) {
        removeState(stateId);
    }
}

// ====== State Queries ======

bool StateConfiguration::isActive(const std::string &stateId) const {
    return activeStates_.find(stateId) != activeStates_.end();
}

const std::set<std::string> &StateConfiguration::getActiveStates() const {
    return activeStates_;
}

std::vector<std::string> StateConfiguration::getActiveStatesVector() const {
    return std::vector<std::string>(activeStates_.begin(), activeStates_.end());
}

bool StateConfiguration::isEmpty() const {
    return activeStates_.empty();
}

size_t StateConfiguration::size() const {
    return activeStates_.size();
}

// ====== Hierarchy Operations ======

std::vector<std::string> StateConfiguration::getActiveAtomicStates() const {
    std::vector<std::string> atomicStates;

    for (const auto &stateId : activeStates_) {
        if (isAtomicState(stateId)) {
            atomicStates.push_back(stateId);
        }
    }

    return atomicStates;
}

std::vector<std::string> StateConfiguration::getActiveCompoundStates() const {
    std::vector<std::string> compoundStates;

    for (const auto &stateId : activeStates_) {
        if (isCompoundState(stateId)) {
            compoundStates.push_back(stateId);
        }
    }

    return compoundStates;
}

bool StateConfiguration::areAncestorsActive(const std::string &stateId) const {
    if (!model_) {
        return false;
    }

    auto ancestors = getAncestors(stateId);
    for (const auto &ancestor : ancestors) {
        if (!isActive(ancestor)) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> StateConfiguration::getStatesInDocumentOrder() const {
    std::vector<std::string> states = getActiveStatesVector();

    // Sort by document order
    std::sort(states.begin(), states.end(),
              [this](const std::string &a, const std::string &b) { return getDocumentOrder(a) < getDocumentOrder(b); });

    return states;
}

std::vector<std::string> StateConfiguration::getStatesInReverseDocumentOrder() const {
    auto states = getStatesInDocumentOrder();
    std::reverse(states.begin(), states.end());
    return states;
}

// ====== Final State Checking ======

bool StateConfiguration::hasActiveFinalStates() const {
    for (const auto &stateId : activeStates_) {
        if (isFinalState(stateId)) {
            return true;
        }
    }
    return false;
}

bool StateConfiguration::isInFinalConfiguration() const {
    if (activeStates_.empty()) {
        return false;
    }

    // Check if all atomic states are final states
    auto atomicStates = getActiveAtomicStates();
    for (const auto &stateId : atomicStates) {
        if (!isFinalState(stateId)) {
            return false;
        }
    }

    return !atomicStates.empty();
}

std::vector<std::string> StateConfiguration::getActiveFinalStates() const {
    std::vector<std::string> finalStates;

    for (const auto &stateId : activeStates_) {
        if (isFinalState(stateId)) {
            finalStates.push_back(stateId);
        }
    }

    return finalStates;
}

// ====== Validation and Debugging ======

std::vector<std::string> StateConfiguration::validate() const {
    std::vector<std::string> errors;

    if (!model_) {
        errors.push_back("No model available for validation");
        return errors;
    }

    // Check for orphaned states (states with inactive ancestors)
    auto orphaned = findOrphanedStates();
    for (const auto &stateId : orphaned) {
        errors.push_back("Orphaned state (inactive ancestors): " + stateId);
    }

    // Check for conflicting states (mutually exclusive states that are both active)
    auto conflicts = findConflictingStates();
    for (const auto &conflict : conflicts) {
        errors.push_back("Conflicting state: " + conflict);
    }

    return errors;
}

std::string StateConfiguration::toString() const {
    if (activeStates_.empty()) {
        return "{}";
    }

    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto &stateId : activeStates_) {
        if (!first) {
            oss << ", ";
        }
        oss << stateId;
        first = false;
    }
    oss << "}";

    return oss.str();
}

bool StateConfiguration::equals(const StateConfiguration &other) const {
    return activeStates_ == other.activeStates_;
}

// ====== Private Helper Methods ======

bool StateConfiguration::isAtomicState(const std::string &stateId) const {
    if (!model_) {
        return false;
    }

    auto stateNode = getStateNode(stateId);
    if (!stateNode) {
        return false;
    }

    // Atomic state has no child states
    return stateNode->getChildren().empty();
}

bool StateConfiguration::isCompoundState(const std::string &stateId) const {
    if (!model_) {
        return false;
    }

    auto stateNode = getStateNode(stateId);
    if (!stateNode) {
        return false;
    }

    // Compound state has child states
    return !stateNode->getChildren().empty();
}

bool StateConfiguration::isFinalState(const std::string &stateId) const {
    if (!model_) {
        return false;
    }

    auto stateNode = getStateNode(stateId);
    if (!stateNode) {
        return false;
    }

    return stateNode->getType() == SCXML::Type::FINAL;
}

std::vector<std::string> StateConfiguration::getAncestors(const std::string &stateId) const {
    std::vector<std::string> ancestors;

    if (!model_) {
        return ancestors;
    }

    auto stateNode = getStateNode(stateId);
    if (!stateNode) {
        return ancestors;
    }

    // Walk up the hierarchy
    auto parent = stateNode->getParent();
    while (parent) {
        ancestors.push_back(parent->getId());
        parent = parent->getParent();
    }

    return ancestors;
}

std::shared_ptr<Model::IStateNode> StateConfiguration::getStateNode(const std::string &stateId) const {
    if (!model_) {
        return nullptr;
    }

    auto rawPtr = model_->findStateById(stateId);
    if (!rawPtr) {
        return nullptr;
    }

    // For now, we'll create a wrapper shared_ptr
    // In a full implementation, the model should return shared_ptr directly
    return std::shared_ptr<Model::IStateNode>(rawPtr, [](Model::IStateNode *) {
        // Don't delete - the model owns the object
    });
}

int StateConfiguration::getDocumentOrder(const std::string &stateId) const {
    if (!model_) {
        return 0;
    }

    // For now, use a simple hash-based ordering
    // In a full implementation, the model should track document order
    return static_cast<int>(std::hash<std::string>{}(stateId) % 10000);
}

bool StateConfiguration::isValidConfiguration() const {
    return validate().empty();
}

std::vector<std::string> StateConfiguration::findOrphanedStates() const {
    std::vector<std::string> orphaned;

    for (const auto &stateId : activeStates_) {
        if (!areAncestorsActive(stateId)) {
            orphaned.push_back(stateId);
        }
    }

    return orphaned;
}

std::vector<std::string> StateConfiguration::findConflictingStates() const {
    std::vector<std::string> conflicts;

    // This is a simplified implementation
    // In a full implementation, we would check for mutual exclusion rules
    // based on the SCXML state hierarchy and parallel regions

    return conflicts;
}

}  // namespace Runtime
}  // namespace SCXML