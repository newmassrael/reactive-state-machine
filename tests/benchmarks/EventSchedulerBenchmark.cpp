#include "events/EventDescriptor.h"
#include "events/EventSchedulerImpl.h"
#include "events/IEventTarget.h"
#include <benchmark/benchmark.h>
#include <memory>
#include <random>
#include <vector>

using namespace RSM;

// ============================================================================
// Helper Functions
// ============================================================================
static EventDescriptor createSimpleEvent(const std::string &name) {
    EventDescriptor event;
    event.eventName = name;
    event.target = "#_internal";
    event.data = "";
    event.sendId = "";
    event.sessionId = "";
    event.type = "scxml";
    event.delay = std::chrono::milliseconds(0);
    return event;
}

// ============================================================================
// Mock Event Target
// ============================================================================
class BenchmarkEventTarget : public IEventTarget {
public:
    std::future<SendResult> send(const EventDescriptor & /*event*/) override {
        std::promise<SendResult> promise;
        promise.set_value(SendResult::success("benchmark_send_id"));
        return promise.get_future();
    }

    std::string getTargetType() const override {
        return "benchmark";
    }

    bool canHandle(const std::string & /*uri*/) const override {
        return true;
    }

    std::vector<std::string> validate() const override {
        return {};
    }

    std::string getDebugInfo() const override {
        return "BenchmarkEventTarget";
    }
};

// ============================================================================
// Benchmark Fixtures
// ============================================================================
class EventSchedulerFixture : public benchmark::Fixture {
protected:
    std::shared_ptr<EventSchedulerImpl> scheduler_;
    std::shared_ptr<IEventTarget> target_;
    std::mt19937 rng_{42};  // Fixed seed for reproducibility

    void SetUp(const ::benchmark::State & /*state*/) override {
        scheduler_ = std::make_shared<EventSchedulerImpl>(
            [](const EventDescriptor &, std::shared_ptr<IEventTarget>, const std::string &) { return true; });
        target_ = std::make_shared<BenchmarkEventTarget>();
    }

    void TearDown(const ::benchmark::State & /*state*/) override {
        scheduler_->shutdown(true);
        scheduler_.reset();
        target_.reset();
    }

    EventDescriptor createRandomEvent() {
        std::uniform_int_distribution<> dist(1, 1000);
        return createSimpleEvent("test.event." + std::to_string(dist(rng_)));
    }

    std::chrono::milliseconds randomDelay(int min_ms = 1, int max_ms = 100) {
        std::uniform_int_distribution<> dist(min_ms, max_ms);
        return std::chrono::milliseconds(dist(rng_));
    }
};

// ============================================================================
// Micro-Benchmarks: Individual Operations
// ============================================================================

// Measure scheduleEvent() throughput
BENCHMARK_F(EventSchedulerFixture, QuickScheduleEvent)(benchmark::State &state) {
    EventDescriptor event = createSimpleEvent("benchmark.event");

    for (auto _ : state) {
        auto future = scheduler_->scheduleEvent(event, std::chrono::milliseconds(100), target_);
        benchmark::DoNotOptimize(future.get());
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("Single-threaded schedule");
}

// Measure cancelEvent() throughput
BENCHMARK_F(EventSchedulerFixture, QuickCancelEvent)(benchmark::State &state) {
    // Pre-schedule events to cancel
    std::vector<std::string> sendIds;
    for (int i = 0; i < 1000; ++i) {
        auto future =
            scheduler_->scheduleEvent(createSimpleEvent("test.event"), std::chrono::milliseconds(10000), target_);
        sendIds.push_back(future.get());
    }

    size_t idx = 0;
    for (auto _ : state) {
        bool cancelled = scheduler_->cancelEvent(sendIds[idx++ % sendIds.size()]);
        benchmark::DoNotOptimize(cancelled);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("Cancel pre-scheduled events");
}

// Measure hasEvent() throughput (read-only operation)
BENCHMARK_F(EventSchedulerFixture, QuickHasEvent)(benchmark::State &state) {
    // Pre-schedule some events
    std::vector<std::string> sendIds;
    for (int i = 0; i < 100; ++i) {
        auto future =
            scheduler_->scheduleEvent(createSimpleEvent("test.event"), std::chrono::milliseconds(10000), target_);
        sendIds.push_back(future.get());
    }

    size_t idx = 0;
    for (auto _ : state) {
        bool exists = scheduler_->hasEvent(sendIds[idx++ % sendIds.size()]);
        benchmark::DoNotOptimize(exists);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("Read-only lookup");
}

// ============================================================================
// Scalability Benchmarks: Thread Contention
// ============================================================================

// Measure contention with concurrent scheduleEvent()
BENCHMARK_F(EventSchedulerFixture, ConcurrentSchedule)(benchmark::State &state) {
    EventDescriptor event = createSimpleEvent("benchmark.concurrent");

    for (auto _ : state) {
        auto future = scheduler_->scheduleEvent(event, randomDelay(50, 150), target_);
        benchmark::DoNotOptimize(future.get());
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("threads=" + std::to_string(state.threads()));
}

BENCHMARK_REGISTER_F(EventSchedulerFixture, ConcurrentSchedule)
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->Threads(16)
    ->UseRealTime();

// Measure mixed read/write contention
BENCHMARK_F(EventSchedulerFixture, MixedOperations)(benchmark::State &state) {
    // Pre-schedule baseline events
    std::vector<std::string> sendIds;
    for (int i = 0; i < 100; ++i) {
        auto future =
            scheduler_->scheduleEvent(createSimpleEvent("test.event"), std::chrono::milliseconds(10000), target_);
        sendIds.push_back(future.get());
    }

    std::uniform_int_distribution<> op_dist(0, 9);
    size_t sendid_idx = 0;

    for (auto _ : state) {
        int op = op_dist(rng_);

        if (op < 5) {
            // 50% schedule
            auto future = scheduler_->scheduleEvent(createRandomEvent(), randomDelay(), target_);
            benchmark::DoNotOptimize(future.get());
        } else if (op < 8) {
            // 30% cancel
            bool cancelled = scheduler_->cancelEvent(sendIds[sendid_idx++ % sendIds.size()]);
            benchmark::DoNotOptimize(cancelled);
        } else {
            // 20% hasEvent
            bool exists = scheduler_->hasEvent(sendIds[sendid_idx++ % sendIds.size()]);
            benchmark::DoNotOptimize(exists);
        }
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("50% schedule, 30% cancel, 20% read | threads=" + std::to_string(state.threads()));
}

BENCHMARK_REGISTER_F(EventSchedulerFixture, MixedOperations)
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->UseRealTime();

// ============================================================================
// Stress Tests: High Load Scenarios
// ============================================================================

// Burst scheduling (many events at once)
BENCHMARK_F(EventSchedulerFixture, BurstSchedule)(benchmark::State &state) {
    const int burst_size = state.range(0);

    for (auto _ : state) {
        std::vector<std::future<std::string>> futures;
        futures.reserve(burst_size);

        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < burst_size; ++i) {
            futures.push_back(scheduler_->scheduleEvent(createRandomEvent(), randomDelay(), target_));
        }

        // Wait for all to complete
        for (auto &f : futures) {
            benchmark::DoNotOptimize(f.get());
        }

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        state.SetIterationTime(elapsed.count() / 1e6);
    }

    state.SetItemsProcessed(state.iterations() * burst_size);
    state.SetLabel("burst_size=" + std::to_string(burst_size));
}

BENCHMARK_REGISTER_F(EventSchedulerFixture, BurstSchedule)->Arg(10)->Arg(100)->Arg(1000)->UseManualTime();

// Session-based cancellation performance
BENCHMARK_F(EventSchedulerFixture, SessionCancellation)(benchmark::State &state) {
    const int events_per_session = 100;
    const std::string session_id = "benchmark_session";

    for (auto _ : state) {
        state.PauseTiming();

        // Schedule events for session
        for (int i = 0; i < events_per_session; ++i) {
            scheduler_->scheduleEvent(createRandomEvent(), std::chrono::milliseconds(10000), target_,
                                      "",  // auto sendId
                                      session_id);
        }

        state.ResumeTiming();

        // Cancel all events for session
        size_t cancelled = scheduler_->cancelEventsForSession(session_id);
        benchmark::DoNotOptimize(cancelled);
    }

    state.SetItemsProcessed(state.iterations() * events_per_session);
    state.SetLabel("events_per_session=" + std::to_string(events_per_session));
}

// ============================================================================
// Latency Benchmarks
// ============================================================================

// Measure end-to-end latency for scheduling
BENCHMARK_F(EventSchedulerFixture, ScheduleLatency)(benchmark::State &state) {
    EventDescriptor event = createSimpleEvent("latency.test");
    std::vector<double> latencies;

    for (auto _ : state) {
        auto start = std::chrono::steady_clock::now();

        auto future = scheduler_->scheduleEvent(event, std::chrono::milliseconds(100), target_);
        std::string sendId = future.get();

        auto end = std::chrono::steady_clock::now();
        auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        latencies.push_back(latency_us);
        benchmark::DoNotOptimize(sendId);
    }

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
// Memory Efficiency Benchmarks
// ============================================================================

// Measure memory overhead of scheduled events
BENCHMARK_F(EventSchedulerFixture, MemoryOverhead)(benchmark::State &state) {
    const int num_events = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();

        std::vector<std::future<std::string>> futures;
        futures.reserve(num_events);

        state.ResumeTiming();

        // Schedule many events
        for (int i = 0; i < num_events; ++i) {
            futures.push_back(
                scheduler_->scheduleEvent(createRandomEvent(), std::chrono::milliseconds(10000), target_));
        }

        // Verify count
        size_t count = scheduler_->getScheduledEventCount();
        benchmark::DoNotOptimize(count);

        state.PauseTiming();

        // Cleanup: collect sendIds first to avoid holding futures during cancellation
        std::vector<std::string> sendIds;
        sendIds.reserve(futures.size());
        for (auto &f : futures) {
            sendIds.push_back(f.get());
        }

        for (const auto &sendId : sendIds) {
            scheduler_->cancelEvent(sendId);
        }

        state.ResumeTiming();
    }

    state.SetLabel("events=" + std::to_string(num_events));
}

BENCHMARK_REGISTER_F(EventSchedulerFixture, MemoryOverhead)->Arg(100)->Arg(1000)->Arg(10000);

BENCHMARK_MAIN();
