#include "scripting/JSEngine.h"
#include <atomic>
#include <benchmark/benchmark.h>
#include <memory>
#include <random>
#include <sstream>
#include <vector>

using namespace RSM;

// ============================================================================
// Thread-safe counter for unique session IDs
// ============================================================================
static std::atomic<uint64_t> globalSessionCounter{0};

static std::string generateUniqueSessionId() {
    uint64_t id = globalSessionCounter.fetch_add(1, std::memory_order_relaxed);
    return "bench_session_" + std::to_string(id);
}

static std::string generateSimpleScript(int complexity) {
    std::stringstream ss;
    ss << "var result = 0;\n";
    for (int i = 0; i < complexity; ++i) {
        ss << "result += " << i << ";\n";
    }
    ss << "result;";
    return ss.str();
}

// ============================================================================
// Benchmark Fixtures
// ============================================================================
class JSEngineFixture : public benchmark::Fixture {
protected:
    JSEngine *engine_;
    std::mt19937 rng_{42};

    void SetUp(const ::benchmark::State & /*state*/) override {
        engine_ = &JSEngine::instance();
    }

    void TearDown(const ::benchmark::State & /*state*/) override {
        // Singleton cleanup not needed between tests
    }

    int randomComplexity(int min_ops = 1, int max_ops = 50) {
        std::uniform_int_distribution<> dist(min_ops, max_ops);
        return dist(rng_);
    }
};

// ============================================================================
// Micro-Benchmarks: Session Management
// ============================================================================

// Measure createSession() throughput
BENCHMARK_F(JSEngineFixture, SessionCreation)(benchmark::State &state) {
    for (auto _ : state) {
        std::string sessionId = generateUniqueSessionId();
        bool created = engine_->createSession(sessionId);
        benchmark::DoNotOptimize(created);

        // Cleanup immediately
        if (created) {
            engine_->destroySession(sessionId);
        }
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("Single-threaded session create/destroy");
}

// Measure hasSession() throughput
BENCHMARK_F(JSEngineFixture, SessionLookup)(benchmark::State &state) {
    // Create a persistent session for lookup tests
    std::string sessionId = generateUniqueSessionId();
    engine_->createSession(sessionId);

    for (auto _ : state) {
        bool exists = engine_->hasSession(sessionId);
        benchmark::DoNotOptimize(exists);
    }

    engine_->destroySession(sessionId);

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("Read-only session lookup");
}

// ============================================================================
// Micro-Benchmarks: Script Execution
// ============================================================================

// Measure simple expression evaluation
BENCHMARK_F(JSEngineFixture, SimpleExpression)(benchmark::State &state) {
    std::string sessionId = generateUniqueSessionId();
    engine_->createSession(sessionId);

    std::string script = "1 + 2 * 3";

    for (auto _ : state) {
        auto result = engine_->evaluateExpression(sessionId, script);
        benchmark::DoNotOptimize(result);
    }

    engine_->destroySession(sessionId);

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("Simple arithmetic expression");
}

// Measure variable operations
BENCHMARK_F(JSEngineFixture, VariableOperations)(benchmark::State &state) {
    std::string sessionId = generateUniqueSessionId();
    engine_->createSession(sessionId);

    for (auto _ : state) {
        engine_->setVariable(sessionId, "testVar", "42");
        auto result = engine_->getVariable(sessionId, "testVar");
        benchmark::DoNotOptimize(result);
    }

    engine_->destroySession(sessionId);

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("Set and get variable");
}

// Measure script execution with varying complexity
BENCHMARK_F(JSEngineFixture, ScriptComplexity)(benchmark::State &state) {
    std::string sessionId = generateUniqueSessionId();
    engine_->createSession(sessionId);

    const int complexity = state.range(0);
    std::string script = generateSimpleScript(complexity);

    for (auto _ : state) {
        auto result = engine_->evaluateExpression(sessionId, script);
        benchmark::DoNotOptimize(result);
    }

    engine_->destroySession(sessionId);

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("complexity=" + std::to_string(complexity));
}

BENCHMARK_REGISTER_F(JSEngineFixture, ScriptComplexity)->Arg(1)->Arg(10)->Arg(50)->Arg(100);

// ============================================================================
// Scalability Benchmarks: Concurrent Operations
// ============================================================================

// Measure contention on session creation (each thread creates unique sessions)
BENCHMARK_F(JSEngineFixture, ConcurrentSessionCreation)(benchmark::State &state) {
    for (auto _ : state) {
        std::string sessionId = generateUniqueSessionId();
        bool created = engine_->createSession(sessionId);
        benchmark::DoNotOptimize(created);

        if (created) {
            engine_->destroySession(sessionId);
        }
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("threads=" + std::to_string(state.threads()));
}

BENCHMARK_REGISTER_F(JSEngineFixture, ConcurrentSessionCreation)
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->UseRealTime();

// Measure script execution across different sessions (no shared state)
BENCHMARK_F(JSEngineFixture, ConcurrentScriptExecution)(benchmark::State &state) {
    // Each thread gets its own session
    std::string sessionId = generateUniqueSessionId();

    if (state.thread_index() == 0) {
        engine_->createSession(sessionId);
    }

    std::string script = "Math.sqrt(1234567) + Math.sin(0.5)";

    for (auto _ : state) {
        auto result = engine_->evaluateExpression(sessionId, script);
        benchmark::DoNotOptimize(result);
    }

    if (state.thread_index() == 0) {
        engine_->destroySession(sessionId);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("threads=" + std::to_string(state.threads()));
}

BENCHMARK_REGISTER_F(JSEngineFixture, ConcurrentScriptExecution)
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->UseRealTime();

// Measure worst-case: multiple threads accessing same session (serialization bottleneck)
BENCHMARK_F(JSEngineFixture, ConcurrentSameSession)(benchmark::State &state) {
    static std::string sharedSessionId;
    static std::once_flag initFlag;

    std::call_once(initFlag, [this]() {
        sharedSessionId = generateUniqueSessionId();
        engine_->createSession(sharedSessionId);
    });

    std::string script = "1 + 2 + 3";

    for (auto _ : state) {
        auto result = engine_->evaluateExpression(sharedSessionId, script);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("shared session | threads=" + std::to_string(state.threads()));
}

BENCHMARK_REGISTER_F(JSEngineFixture, ConcurrentSameSession)
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->UseRealTime();

// ============================================================================
// Mixed Workload Benchmarks
// ============================================================================

// Realistic workload: mix of session operations and script execution
BENCHMARK_F(JSEngineFixture, MixedWorkload)(benchmark::State &state) {
    // Create a pool of sessions for this thread
    std::vector<std::string> sessionPool;
    for (int i = 0; i < 5; ++i) {
        std::string sid = generateUniqueSessionId();
        engine_->createSession(sid);
        sessionPool.push_back(sid);
    }

    std::uniform_int_distribution<> op_dist(0, 9);
    std::uniform_int_distribution<> session_dist(0, sessionPool.size() - 1);

    for (auto _ : state) {
        int op = op_dist(rng_);
        std::string sessionId = sessionPool[session_dist(rng_)];

        if (op < 2) {
            // 20% session creation/destruction
            std::string newSession = generateUniqueSessionId();
            bool created = engine_->createSession(newSession);
            benchmark::DoNotOptimize(created);
            if (created) {
                engine_->destroySession(newSession);
            }
        } else if (op < 4) {
            // 20% session lookup
            bool exists = engine_->hasSession(sessionId);
            benchmark::DoNotOptimize(exists);
        } else if (op < 8) {
            // 40% script execution
            auto result = engine_->evaluateExpression(sessionId, "42 * 2");
            benchmark::DoNotOptimize(result);
        } else {
            // 20% variable operations
            engine_->setVariable(sessionId, "v", "10");
            auto result = engine_->getVariable(sessionId, "v");
            benchmark::DoNotOptimize(result);
        }
    }

    // Cleanup
    for (const auto &sid : sessionPool) {
        engine_->destroySession(sid);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("20% create, 20% lookup, 40% exec, 20% var | threads=" + std::to_string(state.threads()));
}

BENCHMARK_REGISTER_F(JSEngineFixture, MixedWorkload)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->UseRealTime();

// ============================================================================
// Latency Benchmarks
// ============================================================================

// Measure end-to-end latency for script execution
BENCHMARK_F(JSEngineFixture, ScriptExecutionLatency)(benchmark::State &state) {
    std::string sessionId = generateUniqueSessionId();
    engine_->createSession(sessionId);

    std::string script = "Math.pow(2, 10) + Math.sqrt(256)";
    std::vector<double> latencies;

    for (auto _ : state) {
        auto start = std::chrono::steady_clock::now();

        auto result = engine_->evaluateExpression(sessionId, script);
        benchmark::DoNotOptimize(result);

        auto end = std::chrono::steady_clock::now();
        auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        latencies.push_back(latency_us);
    }

    engine_->destroySession(sessionId);

    // Calculate statistics
    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        double p50 = latencies[latencies.size() * 50 / 100];
        double p95 = latencies[latencies.size() * 95 / 100];
        double p99 = latencies[latencies.size() * 99 / 100];

        state.counters["p50_us"] = p50;
        state.counters["p95_us"] = p95;
        state.counters["p99_us"] = p99;
    }
    state.SetLabel("Latency percentiles (us)");
}

// ============================================================================
// Stress Tests
// ============================================================================

// Test with many active sessions
BENCHMARK_F(JSEngineFixture, ManySessionsStress)(benchmark::State &state) {
    const int num_sessions = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();

        // Create many sessions
        std::vector<std::string> sessionIds;
        sessionIds.reserve(num_sessions);
        for (int i = 0; i < num_sessions; ++i) {
            std::string sessionId = generateUniqueSessionId();
            sessionIds.push_back(sessionId);
            engine_->createSession(sessionId);
        }

        state.ResumeTiming();

        // Execute script in random session
        std::string sessionId = sessionIds[rng_() % sessionIds.size()];
        auto result = engine_->evaluateExpression(sessionId, "42");
        benchmark::DoNotOptimize(result);

        state.PauseTiming();

        // Cleanup
        for (const auto &sid : sessionIds) {
            engine_->destroySession(sid);
        }

        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("sessions=" + std::to_string(num_sessions));
}

BENCHMARK_REGISTER_F(JSEngineFixture, ManySessionsStress)->Arg(10)->Arg(50)->Arg(100)->Arg(500);

BENCHMARK_MAIN();
