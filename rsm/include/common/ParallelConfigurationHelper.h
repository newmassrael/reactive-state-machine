#pragma once

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RSM {

/**
 * @brief Helper functions for parallel state configuration tracking
 *
 * W3C SCXML 3.4: Parallel states require tracking multiple active states simultaneously.
 * This helper provides configuration management shared between Interpreter and AOT engines.
 *
 * Configuration: Set of active atomic states (one per region in parallel states)
 */
class ParallelConfigurationHelper {
public:
    /**
     * @brief Configuration structure for tracking active states in parallel regions
     *
     * For static generation: Per-region state variables
     * For Interpreter: std::set<StateId>
     *
     * @tparam StateType State enum or identifier type
     */
    template <typename StateType> struct Configuration {
        // Map from region ID to active state in that region
        std::unordered_map<StateType, StateType> regionStates;

        /**
         * @brief Check if a state is in the configuration
         */
        bool contains(StateType state) const {
            for (const auto &[region, activeState] : regionStates) {
                if (activeState == state) {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Add a state to a region's configuration
         */
        void setRegionState(StateType region, StateType state) {
            regionStates[region] = state;
        }

        /**
         * @brief Get the active state in a region
         */
        std::optional<StateType> getRegionState(StateType region) const {
            auto it = regionStates.find(region);
            if (it != regionStates.end()) {
                return it->second;
            }
            return std::nullopt;
        }

        /**
         * @brief Remove a region from configuration
         */
        void removeRegion(StateType region) {
            regionStates.erase(region);
        }

        /**
         * @brief Get all active states across all regions
         */
        std::vector<StateType> getAllActiveStates() const {
            std::vector<StateType> states;
            states.reserve(regionStates.size());
            for (const auto &[region, state] : regionStates) {
                states.push_back(state);
            }
            return states;
        }

        /**
         * @brief Clear all regions
         */
        void clear() {
            regionStates.clear();
        }

        /**
         * @brief Get number of active regions
         */
        size_t size() const {
            return regionStates.size();
        }

        /**
         * @brief Check if configuration is empty
         */
        bool empty() const {
            return regionStates.empty();
        }
    };

    /**
     * @brief Initialize configuration for entering a parallel state
     *
     * W3C SCXML 3.4: When entering a parallel state, all child regions are entered
     * simultaneously to their initial states.
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with parallel state information
     * @param parallelState The parallel state being entered
     * @param configuration Configuration to update
     */
    template <typename StateType, typename PolicyType>
    static void enterParallelState(StateType parallelState, Configuration<StateType> &configuration) {
        auto regions = PolicyType::getParallelRegions(parallelState);

        for (auto region : regions) {
            // Get initial state for this region
            StateType initialState;
            if (PolicyType::isCompoundState(region)) {
                initialState = PolicyType::getInitialChild(region);
            } else {
                // Atomic state - use itself
                initialState = region;
            }

            // Add to configuration
            configuration.setRegionState(region, initialState);
        }
    }

    /**
     * @brief Exit a parallel state by clearing all region states
     *
     * W3C SCXML 3.4: When exiting a parallel state, all child regions are exited.
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with parallel state information
     * @param parallelState The parallel state being exited
     * @param configuration Configuration to update
     */
    template <typename StateType, typename PolicyType>
    static void exitParallelState(StateType parallelState, Configuration<StateType> &configuration) {
        auto regions = PolicyType::getParallelRegions(parallelState);

        for (auto region : regions) {
            configuration.removeRegion(region);
        }
    }

    /**
     * @brief Update configuration after a transition in a specific region
     *
     * @tparam StateType State enum or identifier type
     * @param region The region where transition occurred
     * @param newState The new active state in that region
     * @param configuration Configuration to update
     */
    template <typename StateType>
    static void updateRegionState(StateType region, StateType newState, Configuration<StateType> &configuration) {
        configuration.setRegionState(region, newState);
    }

    /**
     * @brief Get all states that are currently active (across all regions)
     *
     * Used for event processing - events are dispatched to all active states.
     *
     * @tparam StateType State enum or identifier type
     * @param configuration Current configuration
     * @return Vector of all active states
     */
    template <typename StateType>
    static std::vector<StateType> getActiveStates(const Configuration<StateType> &configuration) {
        return configuration.getAllActiveStates();
    }

    /**
     * @brief Check if a specific state is active in the configuration
     *
     * @tparam StateType State enum or identifier type
     * @param state State to check
     * @param configuration Current configuration
     * @return true if state is active
     */
    template <typename StateType> static bool isActive(StateType state, const Configuration<StateType> &configuration) {
        return configuration.contains(state);
    }
};

}  // namespace RSM
