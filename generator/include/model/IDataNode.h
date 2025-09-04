#pragma once
#include "IExecutionContext.h"
#include <memory>
#include <string>
#include <vector>

namespace SCXML {
namespace Model {

// Forward declarations

/**
 * @brief Interface for SCXML data model elements
 *
 * This interface represents data declarations and definitions in SCXML,
 * including <data>, <donedata>, <content>, and related elements.
 */
class IDataNode {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IDataNode() = default;

    /**
     * @brief Get the data identifier
     * @return Data ID string
     */
    virtual const std::string &getId() const = 0;

    /**
     * @brief Initialize the data element
     * @param context Execution context for initialization
     * @return true if initialization succeeded
     */
    virtual bool initialize(SCXML::Model::IExecutionContext &context) = 0;

    /**
     * @brief Validate the data configuration
     * @return List of validation errors (empty if valid)
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Clone this data node
     * @return Deep copy of the data node
     */
    virtual std::shared_ptr<IDataNode> clone() const = 0;
};

}  // namespace Model
}  // namespace SCXML
