#pragma once

#include <libxml++/libxml++.h>
#include <string>
#include <vector>

/**
 * @brief Interface for XInclude processing
 */

namespace RSM {

class IXIncludeProcessor {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IXIncludeProcessor() = default;

    /**
     * @brief Execute XInclude processing
     * @param doc libxml++ document object
     * @return Success status
     */
    virtual bool process(xmlpp::Document *doc) = 0;

    /**
     * @brief Set base search path
     * @param basePath Base search path
     */
    virtual void setBasePath(const std::string &basePath) = 0;

    /**
     * @brief Return error messages that occurred during processing
     * @return List of error messages
     */
    virtual const std::vector<std::string> &getErrorMessages() const = 0;
};

}  // namespace RSM