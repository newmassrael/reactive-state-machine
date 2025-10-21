#pragma once

#include <memory>
#include <string>
#include <utility>  // for std::pair
#include <vector>

namespace RSM {

class IDataModelItem;

/**
 * @brief Class storing information for <donedata> element
 *
 * This class stores SCXML <donedata> element information.
 * <donedata> contains data to be returned when entering <final> state.
 */
class DoneData {
public:
    /**
     * @brief Default constructor
     */
    DoneData() = default;

    /**
     * @brief Destructor
     */
    ~DoneData() = default;

    /**
     * @brief Set content of <content> element
     * @param content Content string
     */
    void setContent(const std::string &content) {
        content_ = content;
        hasContent_ = true;
    }

    /**
     * @brief Return <content> element content
     * @return Content string
     */
    const std::string &getContent() const {
        return content_;
    }

    /**
     * @brief Add <param> element
     * @param name Parameter name
     * @param location Data model location path
     */
    void addParam(const std::string &name, const std::string &location) {
        params_.push_back(std::make_pair(name, location));
    }

    /**
     * @brief Return <param> element list
     * @return List of parameter names and locations
     */
    const std::vector<std::pair<std::string, std::string>> &getParams() const {
        return params_;
    }

    /**
     * @brief Check if <donedata> element is empty
     * @return true if empty, false otherwise
     */
    bool isEmpty() const {
        return !hasContent_ && params_.empty();
    }

    /**
     * @brief Check if <content> element exists
     * @return true if <content> element exists, false otherwise
     */
    bool hasContent() const {
        return hasContent_;
    }

    void clearParams() {
        params_.clear();
    }

private:
    std::string content_;                                      // Content of <content> element
    std::vector<std::pair<std::string, std::string>> params_;  // <param> element list (name, location)
    bool hasContent_ = false;                                  // Whether <content> element exists
};

}  // namespace RSM