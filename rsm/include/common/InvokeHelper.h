#pragma once

#include "common/Logger.h"
#include <algorithm>
#include <string>
#include <vector>

namespace RSM {

/**
 * @brief Single Source of Truth for W3C SCXML 6.4 invoke lifecycle management
 *
 * ARCHITECTURE.md Compliance:
 * - Zero Duplication: Shared between Interpreter and AOT engines
 * - Single Source of Truth: W3C SCXML 6.4 defer/cancel/execute algorithm
 * - Helper Function Pattern: Follows SendHelper, ForeachHelper, GuardHelper
 *
 * W3C SCXML 6.4: Invoke elements in states entered-but-not-exited during a macrostep
 * are executed at the end of that macrostep. This ensures correct timing:
 * 1. Entry: Defer invoke (add to pending list)
 * 2. Exit: Cancel pending invoke (remove from pending list)
 * 3. Macrostep End: Execute all pending invokes (entered-and-not-exited states only)
 *
 * This pattern prevents invoking in states that are immediately exited, which would
 * violate W3C SCXML semantics (e.g., test 422 - invoke in s11 should not execute
 * because s11 is exited before macrostep completes).
 */
class InvokeHelper {
public:
    /**
     * @brief W3C SCXML 6.4: Defer invoke execution until macrostep end
     *
     * Template parameters:
     * @tparam PendingContainer Container type (std::vector<PendingInvoke>)
     * @tparam InvokeInfo Invoke information structure (must have invokeId, state fields)
     *
     * @param pending Container to store deferred invokes
     * @param invokeInfo Invoke information to defer
     *
     * Usage (AOT):
     * @code
     * struct PendingInvoke { std::string invokeId; State state; };
     * std::vector<PendingInvoke> pending;
     * InvokeHelper::deferInvoke(pending, {"s1_invoke_0", State::S1});
     * @endcode
     *
     * Usage (Interpreter):
     * @code
     * struct PendingInvoke { std::string invokeId; int stateId; };
     * std::vector<PendingInvoke> pending;
     * InvokeHelper::deferInvoke(pending, {"invoke_1", 42});
     * @endcode
     */
    template <typename PendingContainer, typename InvokeInfo>
    static void deferInvoke(PendingContainer &pending, const InvokeInfo &invokeInfo) {
        pending.push_back(invokeInfo);
        // Note: state logging omitted - enum (AOT) not fmt-formattable, string (Interpreter) handled separately
        LOG_DEBUG("InvokeHelper: Deferred invoke {}", invokeInfo.invokeId);
    }

    /**
     * @brief W3C SCXML 6.4: Cancel pending invokes for exited state
     *
     * When a state is exited during a macrostep, its pending invokes must be cancelled
     * to ensure only entered-and-not-exited states have their invokes executed.
     *
     * Template parameters:
     * @tparam PendingContainer Container type (std::vector<PendingInvoke>)
     * @tparam State State type (enum for AOT, int for Interpreter)
     *
     * @param pending Container of deferred invokes
     * @param state State being exited
     *
     * Implementation: Uses std::remove_if + erase idiom for efficient batch removal
     *
     * Usage (AOT):
     * @code
     * InvokeHelper::cancelInvokesForState(pendingInvokes_, State::S11);
     * @endcode
     *
     * Usage (Interpreter):
     * @code
     * InvokeHelper::cancelInvokesForState(pendingInvokes_, 42);
     * @endcode
     */
    template <typename PendingContainer, typename State>
    static void cancelInvokesForState(PendingContainer &pending, State state) {
        auto it = std::remove_if(pending.begin(), pending.end(), [state](const auto &p) { return p.state == state; });

        if (it != pending.end()) {
            // Log cancellations for debugging (state omitted - not fmt-formattable for AOT enums)
            for (auto i = it; i != pending.end(); ++i) {
                LOG_DEBUG("InvokeHelper: Cancelled pending invoke {}", i->invokeId);
            }
            pending.erase(it, pending.end());
        }
    }

    /**
     * @brief W3C SCXML 6.4: Execute all pending invokes at macrostep end
     *
     * After a macrostep completes (stable configuration reached), all invokes that
     * were deferred during entry actions are executed. This ensures correct W3C SCXML
     * semantics where only entered-and-not-exited states have active invokes.
     *
     * Template parameters:
     * @tparam PendingContainer Container type (std::vector<PendingInvoke>)
     * @tparam Executor Functor for executing individual invokes
     *
     * @param pending Container of deferred invokes (will be cleared)
     * @param executor Callable that executes a single invoke
     *                 Signature: void(const InvokeInfo&)
     *
     * Pattern: Copy-and-clear to prevent iterator invalidation during execution
     * (executing an invoke may trigger events that modify the pending list)
     *
     * Usage (AOT):
     * @code
     * InvokeHelper::executePendingInvokes(pendingInvokes_,
     *     [this, &engine](const PendingInvoke& p) {
     *         // Execute invoke for p.invokeId
     *         if (child_s1_invoke_0_) {
     *             child_s1_invoke_0_->initialize();
     *             if (child_s1_invoke_0_->isInFinalState()) {
     *                 engine.raise(Event::Done_invoke);
     *             }
     *         }
     *     });
     * @endcode
     *
     * Usage (Interpreter):
     * @code
     * InvokeHelper::executePendingInvokes(pendingInvokes_,
     *     [this](const PendingInvoke& p) {
     *         auto child = createChildStateMachine(p.invokeId);
     *         child->start();
     *     });
     * @endcode
     */
    template <typename PendingContainer, typename Executor>
    static void executePendingInvokes(PendingContainer &pending, Executor executor) {
        if (pending.empty()) {
            return;
        }

        LOG_DEBUG("InvokeHelper: Executing {} pending invokes", pending.size());

        // W3C SCXML 6.4: Copy pending list to prevent iterator invalidation
        // Child state machines may raise events during initialization
        auto invokesToExecute = pending;
        pending.clear();

        // Execute each pending invoke
        for (const auto &invokeInfo : invokesToExecute) {
            // Note: state logging omitted - enum (AOT) not fmt-formattable, string (Interpreter) logged separately
            LOG_DEBUG("InvokeHelper: Starting invoke {}", invokeInfo.invokeId);
            try {
                executor(invokeInfo);
            } catch (const std::exception &e) {
                LOG_ERROR("InvokeHelper: Failed to execute invoke {}: {}", invokeInfo.invokeId, e.what());
                // Continue with remaining invokes (don't fail entire macrostep)
            }
        }
    }

    /**
     * @brief W3C SCXML 6.4: Get count of pending invokes
     *
     * Utility for monitoring and debugging.
     *
     * @tparam PendingContainer Container type
     * @param pending Container of deferred invokes
     * @return Number of pending invokes
     */
    template <typename PendingContainer> static size_t getPendingCount(const PendingContainer &pending) {
        return pending.size();
    }

    /**
     * @brief W3C SCXML 6.4: Check if specific invoke is pending
     *
     * Utility for debugging and assertions.
     *
     * @tparam PendingContainer Container type
     * @param pending Container of deferred invokes
     * @param invokeId Invoke ID to check
     * @return true if invoke is pending
     */
    template <typename PendingContainer>
    static bool isInvokePending(const PendingContainer &pending, const std::string &invokeId) {
        return std::any_of(pending.begin(), pending.end(),
                           [&invokeId](const auto &p) { return p.invokeId == invokeId; });
    }
};

}  // namespace RSM
