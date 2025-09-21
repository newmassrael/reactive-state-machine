#pragma once

#include "ConcurrentStateTypes.h"
#include <memory>
#include <string>
#include <vector>

namespace RSM {

// Forward declarations
struct EventDescriptor;
class IStateNode;

/**
 * @brief Interface for concurrent regions in parallel states
 *
 * A concurrent region represents an independent execution path within
 * a parallel state. Each region maintains its own state configuration
 * and processes events independently.
 *
 * SCXML Compliance:
 * - Each region operates independently
 * - Regions can reach final states individually
 * - All regions must complete for parallel state completion
 */
class IConcurrentRegion {
public:
    virtual ~IConcurrentRegion() = default;

    /**
     * @brief Get the unique identifier for this region
     * @return Region ID string
     */
    virtual const std::string &getId() const = 0;

    /**
     * @brief Activate this region
     * @return Operation result indicating success or failure
     */
    virtual ConcurrentOperationResult activate() = 0;

    /**
     * @brief Deactivate this region
     * @return Operation result indicating success or failure
     */
    virtual ConcurrentOperationResult deactivate() = 0;

    /**
     * @brief Check if this region is currently active
     * @return true if region is active
     */
    virtual bool isActive() const = 0;

    /**
     * @brief Check if this region has reached a final state
     * @return true if region is in a final state
     */
    virtual bool isInFinalState() const = 0;

    /**
     * @brief Get current status of this region
     * @return Current region status
     */
    virtual ConcurrentRegionStatus getStatus() const = 0;

    /**
     * @brief Get information about this region
     * @return Region information structure
     */
    virtual ConcurrentRegionInfo getInfo() const = 0;

    /**
     * @brief Process an event in this region
     * @param event Event to process
     * @return Operation result with any generated events
     */
    virtual ConcurrentOperationResult processEvent(const EventDescriptor &event) = 0;

    /**
     * @brief Get the root state node for this region
     * @return Shared pointer to root state node
     */
    virtual std::shared_ptr<IStateNode> getRootState() const = 0;

    /**
     * @brief Set the root state node for this region
     * @param rootState Root state node for this region
     */
    virtual void setRootState(std::shared_ptr<IStateNode> rootState) = 0;

    /**
     * @brief Get currently active states in this region
     * @return Vector of active state IDs
     */
    virtual std::vector<std::string> getActiveStates() const = 0;

    /**
     * @brief Reset this region to its initial state
     * @return Operation result
     */
    virtual ConcurrentOperationResult reset() = 0;

    /**
     * @brief Validate the configuration of this region
     * @return Vector of validation error messages (empty if valid)
     */
    virtual std::vector<std::string> validate() const = 0;
};

}  // namespace RSM