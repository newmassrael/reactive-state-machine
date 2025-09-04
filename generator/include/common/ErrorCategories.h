#ifndef ERRORCATEGORIES_H
#define ERRORCATEGORIES_H

#include <stdexcept>
#include <string>

namespace SCXML {
namespace Common {

enum class ErrorCategory {
    PARSING_ERROR,
    RUNTIME_ERROR,
    VALIDATION_ERROR,
    NETWORK_ERROR,
    JAVASCRIPT_ERROR,
    DATAMODEL_ERROR,
    IO_ERROR,
    SYSTEM_ERROR
};

class SCXMLException : public std::exception {
public:
    SCXMLException(ErrorCategory category, const std::string &message, const std::string &context = "")
        : category_(category), message_(message), context_(context) {
        fullMessage_ = formatMessage();
    }

    const char *what() const noexcept override {
        return fullMessage_.c_str();
    }

    ErrorCategory getCategory() const {
        return category_;
    }

    const std::string &getMessage() const {
        return message_;
    }

    const std::string &getContext() const {
        return context_;
    }

private:
    ErrorCategory category_;
    std::string message_;
    std::string context_;
    std::string fullMessage_;

    std::string formatMessage() const {
        std::string prefix = getCategoryString(category_);
        std::string result = "[" + prefix + "] " + message_;
        if (!context_.empty()) {
            result += " (Context: " + context_ + ")";
        }
        return result;
    }

    static std::string getCategoryString(ErrorCategory category) {
        switch (category) {
        case ErrorCategory::PARSING_ERROR:
            return "PARSING";
        case ErrorCategory::RUNTIME_ERROR:
            return "RUNTIME";
        case ErrorCategory::VALIDATION_ERROR:
            return "VALIDATION";
        case ErrorCategory::NETWORK_ERROR:
            return "NETWORK";
        case ErrorCategory::JAVASCRIPT_ERROR:
            return "JAVASCRIPT";
        case ErrorCategory::DATAMODEL_ERROR:
            return "DATAMODEL";
        case ErrorCategory::IO_ERROR:
            return "IO";
        case ErrorCategory::SYSTEM_ERROR:
            return "SYSTEM";
        default:
            return "UNKNOWN";
        }
    }
};

}  // namespace Common
}  // namespace SCXML

#endif  // ERRORCATEGORIES_H