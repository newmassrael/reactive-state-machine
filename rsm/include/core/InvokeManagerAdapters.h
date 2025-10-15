#pragma once

#include "runtime/InvokeExecutor.h"
#include <memory>
#include <string>
#include <vector>

/**
 * @file InvokeManagerAdapters.h
 * @brief Adapter pattern for Invoke management (Phase 3)
 *
 * Design principles (same as EventQueueAdapters):
 * 1. Minimal interface for InvokeProcessingAlgorithms
 * 2. Engine-specific implementations hidden behind uniform API
 * 3. Zero overhead through inline methods
 *
 * Required interface for InvokeProcessingAlgorithms:
 * - std::string getFinalizeScript(const std::string& childSessionId)
 * - std::vector<std::shared_ptr<StateMachine>>
 *   getAutoforwardSessions(const std::string& parentSessionId)
 */

namespace RSM::Core {

// Forward declaration
class StateMachine;

/**
 * @brief Interpreter engine invoke manager adapter
 *
 * Adapts InvokeExecutor (Interpreter's invoke management) to the unified
 * interface required by InvokeProcessingAlgorithms.
 *
 * Implementation notes:
 * - Direct delegation to InvokeExecutor methods
 * - InvokeExecutor handles all complexity (session tracking, finalize scripts)
 * - Adapter is just a thin wrapper for interface unification
 *
 * @example Usage in Interpreter StateMachine.cpp:
 * @code
 * RSM::Core::InterpreterInvokeManager adapter(invokeExecutor_);
 * RSM::Core::InvokeProcessingAlgorithms::processFinalize(
 *     event.originSessionId,
 *     adapter,
 *     *actionExecutor_
 * );
 * @endcode
 */
class InterpreterInvokeManager {
public:
    /**
     * @brief Constructor
     * @param executor InvokeExecutor shared pointer
     */
    explicit InterpreterInvokeManager(std::shared_ptr<InvokeExecutor> executor) : executor_(executor) {}

    /**
     * @brief Get finalize script for child session (W3C SCXML 6.5)
     * @param childSessionId Child session ID that sent event
     * @return Finalize script if exists, empty string otherwise
     */
    std::string getFinalizeScript(const std::string &childSessionId) const {
        if (!executor_) {
            return "";
        }
        return executor_->getFinalizeScriptForChildSession(childSessionId);
    }

    /**
     * @brief Get child sessions with autoforward enabled (W3C SCXML 6.4.1)
     * @param parentSessionId Parent session ID
     * @return Vector of child StateMachine shared_ptrs with autoforward=true
     */
    std::vector<std::shared_ptr<StateMachine>> getAutoforwardSessions(const std::string &parentSessionId) {
        if (!executor_) {
            return {};
        }
        return executor_->getAutoForwardSessions(parentSessionId);
    }

private:
    std::shared_ptr<InvokeExecutor> executor_;
};

/**
 * @brief JIT engine invoke manager adapter
 *
 * Adapts JIT's Policy class (containing activeInvokes_ map) to the unified
 * interface required by InvokeProcessingAlgorithms.
 *
 * Implementation notes:
 * - Policy stores invoke info in activeInvokes_ map (sessionId -> ChildSession)
 * - ChildSession contains: invokeId, finalizeScript, autoforward flag, StateMachine*
 * - Adapter extracts data from Policy's flat map structure
 *
 * @tparam Policy Generated policy class from StaticCodeGenerator
 *         Must provide:
 *         - std::unordered_map<std::string, ChildSession> activeInvokes_;
 *         - struct ChildSession { finalizeScript, autoforward, parentSessionId, stateMachine }
 *
 * @example Usage in generated JIT code:
 * @code
 * RSM::Core::JITInvokeManager<MyStateMachinePolicy> adapter(policy_);
 * RSM::Core::InvokeProcessingAlgorithms::processFinalize(
 *     getOriginSessionId(event),
 *     adapter,
 *     *this
 * );
 * @endcode
 */
template <typename Policy> class JITInvokeManager {
public:
    /**
     * @brief Constructor
     * @param policy Reference to Policy instance containing invoke data
     */
    explicit JITInvokeManager(Policy &policy) : policy_(policy) {}

    /**
     * @brief Get finalize script for child session (W3C SCXML 6.5)
     * @param childSessionId Child session ID that sent event
     * @return Finalize script if exists, empty string otherwise
     */
    std::string getFinalizeScript(const std::string &childSessionId) const {
        // Find invoke by matching child session ID
        for (const auto &[invokeId, session] : policy_.activeInvokes_) {
            if (session.sessionId == childSessionId) {
                return session.finalizeScript;
            }
        }
        return "";
    }

    /**
     * @brief Get child sessions with autoforward enabled (W3C SCXML 6.4.1)
     * @param parentSessionId Parent session ID
     * @return Vector of child StateMachine shared_ptrs with autoforward=true
     */
    std::vector<std::shared_ptr<StateMachine>> getAutoforwardSessions(const std::string &parentSessionId) {
        std::vector<std::shared_ptr<StateMachine>> result;

        for (auto &[sessionId, session] : policy_.activeInvokes_) {
            if (session.autoforward && session.parentSessionId == parentSessionId && session.stateMachine) {
                result.push_back(session.stateMachine);
            }
        }

        return result;
    }

private:
    Policy &policy_;
};

}  // namespace RSM::Core
