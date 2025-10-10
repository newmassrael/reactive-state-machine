#include "events/EventDescriptor.h"
#include "runtime/StateMachine.h"
#include "runtime/StateMachineBuilder.h"
#include <atomic>
#include <benchmark/benchmark.h>
#include <memory>
#include <sstream>
#include <string>

using namespace RSM;

// Thread-safe counter for unique session IDs
static std::atomic<uint64_t> globalSessionCounter{0};

static std::string generateUniqueSessionId() {
    uint64_t id = globalSessionCounter.fetch_add(1, std::memory_order_relaxed);
    return "sm_bench_session_" + std::to_string(id);
}

// ============================================================================
// SCXML Test Models
// ============================================================================

// Simple model: 3 states in a cycle (s1 -> s2 -> s3 -> s1)
static const char *SIMPLE_3_STATE_MODEL = R"(
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s1">
  <state id="s1">
    <transition event="e1" target="s2"/>
  </state>
  <state id="s2">
    <transition event="e2" target="s3"/>
  </state>
  <state id="s3">
    <transition event="e3" target="s1"/>
  </state>
</scxml>
)";

// Generate models with varying number of states
static std::string generateChainModel(int numStates) {
    std::stringstream ss;
    ss << R"(<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="s0">)";

    for (int i = 0; i < numStates; ++i) {
        ss << "\n  <state id=\"s" << i << "\">";
        if (i < numStates - 1) {
            ss << "\n    <transition event=\"e" << i << "\" target=\"s" << (i + 1) << "\"/>";
        } else {
            // Last state transitions back to first
            ss << "\n    <transition event=\"e" << i << "\" target=\"s0\"/>";
        }
        ss << "\n  </state>";
    }

    ss << "\n</scxml>";
    return ss.str();
}

// Nested state model (A contains A1, A2; B contains B1, B2)
static const char *NESTED_STATE_MODEL = R"(
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="A">
  <state id="A" initial="A1">
    <state id="A1">
      <transition event="toA2" target="A2"/>
    </state>
    <state id="A2">
      <transition event="toB" target="B"/>
    </state>
  </state>
  <state id="B" initial="B1">
    <state id="B1">
      <transition event="toB2" target="B2"/>
    </state>
    <state id="B2">
      <transition event="toA" target="A"/>
    </state>
  </state>
</scxml>
)";

// Parallel state model (2 concurrent regions)
static const char *PARALLEL_STATE_MODEL = R"(
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="P">
  <parallel id="P">
    <state id="R1" initial="R1S1">
      <state id="R1S1">
        <transition event="r1_next" target="R1S2"/>
      </state>
      <state id="R1S2">
        <transition event="r1_reset" target="R1S1"/>
      </state>
    </state>
    <state id="R2" initial="R2S1">
      <state id="R2S1">
        <transition event="r2_next" target="R2S2"/>
      </state>
      <state id="R2S2">
        <transition event="r2_reset" target="R2S1"/>
      </state>
    </state>
  </parallel>
</scxml>
)";

// ============================================================================
// Benchmark Fixture
// ============================================================================

class StateMachineFixture : public benchmark::Fixture {
protected:
    void SetUp(const ::benchmark::State & /*state*/) override {
        // Fixture setup if needed
    }

    void TearDown(const ::benchmark::State & /*state*/) override {
        // Cleanup if needed
    }

    std::shared_ptr<StateMachine> createStateMachine(const std::string &scxmlContent) {
        std::string sessionId = generateUniqueSessionId();

        StateMachineBuilder builder;
        auto sm = builder.withSessionId(sessionId).build();

        if (sm && sm->loadSCXMLFromString(scxmlContent) && sm->start()) {
            return sm;
        }
        return nullptr;
    }
};

// ============================================================================
// Micro-Benchmarks: Event Processing
// ============================================================================

// Measure processEvent() throughput with simple transitions
BENCHMARK_F(StateMachineFixture, EventProcessing)(benchmark::State &state) {
    auto sm = createStateMachine(SIMPLE_3_STATE_MODEL);
    if (!sm) {
        state.SkipWithError("Failed to create StateMachine");
        return;
    }

    const std::vector<std::string> events = {"e1", "e2", "e3"};
    size_t eventIndex = 0;

    for (auto _ : state) {
        std::string eventName = events[eventIndex % events.size()];
        auto result = sm->processEvent(eventName);
        benchmark::DoNotOptimize(result.success);

        eventIndex++;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("3-state cycle event processing");
}

// Measure event processing with varying model complexity
BENCHMARK_F(StateMachineFixture, EventProcessingScalability)(benchmark::State &state) {
    const int numStates = state.range(0);
    std::string model = generateChainModel(numStates);

    auto sm = createStateMachine(model);
    if (!sm) {
        state.SkipWithError("Failed to create StateMachine");
        return;
    }

    size_t eventIndex = 0;

    for (auto _ : state) {
        std::string eventName = "e" + std::to_string(eventIndex % numStates);
        auto result = sm->processEvent(eventName);
        benchmark::DoNotOptimize(result.success);

        eventIndex++;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel(std::to_string(numStates) + " states");
}

BENCHMARK_REGISTER_F(StateMachineFixture, EventProcessingScalability)->Arg(5)->Arg(10)->Arg(20);

// ============================================================================
// State Transition Benchmarks
// ============================================================================

// Measure simple state transition (A -> B)
BENCHMARK_F(StateMachineFixture, SimpleTransition)(benchmark::State &state) {
    auto sm = createStateMachine(SIMPLE_3_STATE_MODEL);
    if (!sm) {
        state.SkipWithError("Failed to create StateMachine");
        return;
    }

    bool toggle = false;

    for (auto _ : state) {
        // Alternate between e1 and e2 to create transitions
        std::string eventName = toggle ? "e1" : "e2";
        auto result = sm->processEvent(eventName);
        benchmark::DoNotOptimize(result.success);

        toggle = !toggle;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("Simple A->B->A transitions");
}

// Measure nested state transitions (deep hierarchy)
BENCHMARK_F(StateMachineFixture, NestedTransition)(benchmark::State &state) {
    auto sm = createStateMachine(NESTED_STATE_MODEL);
    if (!sm) {
        state.SkipWithError("Failed to create StateMachine");
        return;
    }

    const std::vector<std::string> events = {"toA2", "toB", "toB2", "toA"};
    size_t eventIndex = 0;

    for (auto _ : state) {
        std::string eventName = events[eventIndex % events.size()];
        auto result = sm->processEvent(eventName);
        benchmark::DoNotOptimize(result.success);

        eventIndex++;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("Nested state transitions");
}

// ============================================================================
// Parallel State Benchmarks
// ============================================================================

// Measure event processing in parallel states (multiple regions active)
BENCHMARK_F(StateMachineFixture, ParallelStateEvent)(benchmark::State &state) {
    auto sm = createStateMachine(PARALLEL_STATE_MODEL);
    if (!sm) {
        state.SkipWithError("Failed to create StateMachine");
        return;
    }

    const std::vector<std::string> events = {"r1_next", "r2_next", "r1_reset", "r2_reset"};
    size_t eventIndex = 0;

    for (auto _ : state) {
        std::string eventName = events[eventIndex % events.size()];
        auto result = sm->processEvent(eventName);
        benchmark::DoNotOptimize(result.success);

        eventIndex++;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("Parallel state (2 regions) event processing");
}

// ============================================================================
// Latency Benchmarks
// ============================================================================

// Measure end-to-end latency for event processing
BENCHMARK_F(StateMachineFixture, EventProcessingLatency)(benchmark::State &state) {
    auto sm = createStateMachine(SIMPLE_3_STATE_MODEL);
    if (!sm) {
        state.SkipWithError("Failed to create StateMachine");
        return;
    }

    const std::vector<std::string> events = {"e1", "e2", "e3"};
    size_t eventIndex = 0;
    std::vector<double> latencies;

    for (auto _ : state) {
        auto start = std::chrono::steady_clock::now();

        std::string eventName = events[eventIndex % events.size()];
        auto result = sm->processEvent(eventName);
        benchmark::DoNotOptimize(result.success);

        auto end = std::chrono::steady_clock::now();
        auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        latencies.push_back(latency_us);

        eventIndex++;
    }

    // Calculate latency percentiles
    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        double p50 = latencies[latencies.size() * 50 / 100];
        double p95 = latencies[latencies.size() * 95 / 100];
        double p99 = latencies[latencies.size() * 99 / 100];

        state.counters["p50_us"] = p50;
        state.counters["p95_us"] = p95;
        state.counters["p99_us"] = p99;
    }

    state.SetLabel("Event processing latency percentiles");
}

// ============================================================================
// StateMachine Creation Benchmark (Baseline)
// ============================================================================

// Measure StateMachine creation overhead (for context)
BENCHMARK_F(StateMachineFixture, StateMachineCreation)(benchmark::State &state) {
    for (auto _ : state) {
        auto sm = createStateMachine(SIMPLE_3_STATE_MODEL);
        benchmark::DoNotOptimize(sm);

        // Explicit cleanup
        sm.reset();
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("StateMachine creation and destruction");
}

BENCHMARK_MAIN();
