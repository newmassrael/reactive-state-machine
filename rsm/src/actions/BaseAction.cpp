#include "actions/BaseAction.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>

namespace RSM {

BaseAction::BaseAction(const std::string &id) : id_(id) {}

std::string BaseAction::getId() const {
    return id_;
}

void BaseAction::setId(const std::string &id) {
    id_ = id;
}

std::string BaseAction::getDescription() const {
    std::string desc = getActionType();
    if (!id_.empty()) {
        desc += " (id: " + id_ + ")";
    }

    std::string specific = getSpecificDescription();
    if (!specific.empty()) {
        desc += " - " + specific;
    }

    return desc;
}

std::vector<std::string> BaseAction::validate() const {
    std::vector<std::string> errors;

    // Common validations
    if (!id_.empty() &&
        id_.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos) {
        errors.push_back("Action ID contains invalid characters: " + id_);
    }

    // Add action-specific validations
    auto specificErrors = validateSpecific();
    errors.insert(errors.end(), specificErrors.begin(), specificErrors.end());

    return errors;
}

bool BaseAction::isEmptyString(const std::string &str) const {
    return trimString(str).empty();
}

std::string BaseAction::trimString(const std::string &str) const {
    auto start = str.begin();
    auto end = str.end();

    // Trim leading whitespace
    start = std::find_if(start, end, [](unsigned char ch) { return !std::isspace(ch); });

    // Trim trailing whitespace
    end = std::find_if(str.rbegin(), std::string::const_reverse_iterator(start), [](unsigned char ch) {
              return !std::isspace(ch);
          }).base();

    return std::string(start, end);
}

std::string BaseAction::generateUniqueId(const std::string &prefix) {
    // SCXML Compliance: Generate unique ID each time action is executed
    // Use timestamp + atomic counter for uniqueness across sessions
    static std::atomic<uint64_t> counter{0};

    auto timestamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();

    auto currentCounter = counter.fetch_add(1);

    return prefix + "_" + std::to_string(timestamp) + "_" + std::to_string(currentCounter);
}

}  // namespace RSM