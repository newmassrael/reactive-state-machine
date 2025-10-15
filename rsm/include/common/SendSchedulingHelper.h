#pragma once

#include <atomic>
#include <chrono>
#include <queue>
#include <regex>
#include <string>
#include <unordered_set>

namespace RSM {

/**
 * @brief Helper for W3C SCXML <send> delay parsing and scheduling
 *
 * Single Source of Truth for send action delay logic shared between:
 * - Interpreter engine (ActionExecutorImpl)
 * - JIT engine (StaticCodeGenerator)
 *
 * W3C SCXML References:
 * - 6.2: Send element delay/delayexpr semantics
 * - 3.12: Event scheduling and delayed delivery
 *
 * Phase 4: Timer support for delayed send events
 */
class SendSchedulingHelper {
public:
    /**
     * @brief Parse W3C SCXML delay string to milliseconds
     *
     * W3C SCXML 6.2: Delay formats - "5s", "100ms", "2min", ".5s", "0.5s"
     *
     * Single Source of Truth for delay parsing logic.
     * Used by both Interpreter and JIT engines to ensure consistent timing.
     *
     * @param delayStr Delay specification (e.g., "5s", "100ms", "2min")
     * @return Delay in milliseconds, 0 if invalid or empty
     */
    static std::chrono::milliseconds parseDelayString(const std::string &delayStr) {
        if (delayStr.empty()) {
            return std::chrono::milliseconds{0};
        }

        // Parse delay formats: "5s", "100ms", "2min", "1h", ".5s", "0.5s"
        std::regex delayPattern(R"((\d*\.?\d+)\s*(ms|s|min|h|sec|seconds?|minutes?|hours?)?)");
        std::smatch match;

        if (!std::regex_match(delayStr, match, delayPattern)) {
            return std::chrono::milliseconds{0};  // Invalid format
        }

        double value = std::stod(match[1].str());
        std::string unit = match[2].str();

        // Convert to milliseconds based on unit
        if (unit.empty() || unit == "s" || unit == "sec" || unit == "second" || unit == "seconds") {
            return std::chrono::milliseconds(static_cast<long long>(value * 1000));
        } else if (unit == "ms") {
            return std::chrono::milliseconds(static_cast<long long>(value));
        } else if (unit == "min" || unit == "minute" || unit == "minutes") {
            return std::chrono::milliseconds(static_cast<long long>(value * 60000));
        } else if (unit == "h" || unit == "hour" || unit == "hours") {
            return std::chrono::milliseconds(static_cast<long long>(value * 3600000));
        }

        return std::chrono::milliseconds{0};  // Unknown unit
    }

    /**
     * @brief Scheduled event structure for delayed send
     *
     * Stores event with its scheduled fire time and optional sendid for cancellation.
     * Used by JIT engine to implement W3C SCXML delayed event delivery.
     *
     * W3C SCXML 6.2.5: sendid enables event cancellation via <cancel sendidexpr="..."/>
     */
    template <typename EventType> struct ScheduledEvent {
        EventType event;
        std::chrono::steady_clock::time_point fireTime;
        std::string sendId;  // W3C SCXML 6.2.5: Unique identifier for cancellation

        ScheduledEvent(EventType evt, std::chrono::steady_clock::time_point fire, std::string id = "")
            : event(evt), fireTime(fire), sendId(std::move(id)) {}

        // Comparator for priority queue (earlier times have higher priority)
        bool operator>(const ScheduledEvent &other) const {
            return fireTime > other.fireTime;
        }
    };

    /**
     * @brief Simple event scheduler for JIT-generated state machines
     *
     * Provides basic delayed event delivery without full EventSchedulerImpl overhead.
     * Follows "You don't pay for what you don't use" philosophy.
     *
     * Thread-safety: Not thread-safe (JIT state machines are single-threaded)
     * Performance: O(log n) insert, O(log n) pop
     */
    template <typename EventType> class SimpleScheduler {
    public:
        using ScheduledEventType = ScheduledEvent<EventType>;

        /**
         * @brief Schedule an event for future delivery
         *
         * W3C SCXML 6.2: Send element with delay/delayexpr
         * W3C SCXML 6.2.5: Returns sendid for event tracking and cancellation
         *
         * @param event Event to schedule
         * @param delay Delay before delivery
         * @param sendId Optional sendid for cancellation (generated if empty)
         * @return The sendid assigned to this event (for cancellation)
         */
        std::string scheduleEvent(EventType event, std::chrono::milliseconds delay, const std::string &sendId = "") {
            auto fireTime = std::chrono::steady_clock::now() + delay;

            // W3C SCXML 6.2.5: Generate unique sendid if not provided
            std::string actualSendId = sendId;
            if (actualSendId.empty()) {
                actualSendId = generateUniqueSendId();
            }

            queue_.push(ScheduledEventType(event, fireTime, actualSendId));
            return actualSendId;
        }

        /**
         * @brief Check if any events are ready to fire
         *
         * @return true if events ready, false otherwise
         */
        bool hasReadyEvents() const {
            if (queue_.empty()) {
                return false;
            }
            return queue_.top().fireTime <= std::chrono::steady_clock::now();
        }

        /**
         * @brief Get next ready event (skips cancelled events)
         *
         * W3C SCXML 6.2.5: Cancelled events are automatically filtered out
         *
         * @param outEvent Output parameter for event
         * @return true if event retrieved, false if no ready events
         */
        bool popReadyEvent(EventType &outEvent) {
            while (!queue_.empty()) {
                if (queue_.top().fireTime > std::chrono::steady_clock::now()) {
                    return false;  // No ready events yet
                }

                auto scheduledEvent = queue_.top();
                queue_.pop();

                // W3C SCXML 6.2.5: Skip cancelled events
                if (!scheduledEvent.sendId.empty() && isCancelled(scheduledEvent.sendId)) {
                    cancelledSendIds_.erase(scheduledEvent.sendId);  // Clean up
                    continue;                                        // Skip this event
                }

                outEvent = scheduledEvent.event;
                return true;
            }
            return false;
        }

        /**
         * @brief Check if scheduler has any pending events
         *
         * @return true if pending events exist
         */
        bool hasPendingEvents() const {
            return !queue_.empty();
        }

        /**
         * @brief Cancel a scheduled event by sendid
         *
         * W3C SCXML 6.2.5: <cancel sendidexpr="..."/> cancels pending delayed send
         *
         * @param sendId The sendid of the event to cancel
         * @return true if event was found and cancelled, false otherwise
         */
        bool cancelEvent(const std::string &sendId) {
            if (sendId.empty()) {
                return false;
            }

            // Note: std::priority_queue doesn't support removal
            // Store cancelled sendids and filter during popReadyEvent()
            cancelledSendIds_.insert(sendId);
            return true;
        }

        /**
         * @brief Check if a sendid has been cancelled
         *
         * @param sendId The sendid to check
         * @return true if cancelled, false otherwise
         */
        bool isCancelled(const std::string &sendId) const {
            return cancelledSendIds_.find(sendId) != cancelledSendIds_.end();
        }

        /**
         * @brief Clear all scheduled events and cancellation records
         */
        void clear() {
            while (!queue_.empty()) {
                queue_.pop();
            }
            cancelledSendIds_.clear();
        }

    private:
        /**
         * @brief Generate unique sendid for event tracking
         * @return Unique sendid string
         */
        static std::string generateUniqueSendId() {
            static std::atomic<uint64_t> counter{0};
            return "sendid_" + std::to_string(++counter);
        }

        std::priority_queue<ScheduledEventType, std::vector<ScheduledEventType>, std::greater<ScheduledEventType>>
            queue_;
        std::unordered_set<std::string> cancelledSendIds_;  // W3C SCXML 6.2.5: Track cancelled events
    };
};

}  // namespace RSM
