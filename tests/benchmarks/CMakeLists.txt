cmake_minimum_required(VERSION 3.14)

# ============================================================================
# Dependencies
# ============================================================================
find_package(Threads REQUIRED)

# ============================================================================
# Google Benchmark Setup
# ============================================================================
find_package(benchmark QUIET)

if(NOT benchmark_FOUND)
    message(STATUS "Google Benchmark not found, using FetchContent to download")

    include(FetchContent)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    # Benchmark options
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googlebenchmark)
else()
    message(STATUS "Using system-installed Google Benchmark")
endif()

# ============================================================================
# Benchmark Executable
# ============================================================================

# EventScheduler Benchmarks
add_executable(benchmark_event_scheduler
    EventSchedulerBenchmark.cpp
)
target_link_libraries(benchmark_event_scheduler
    PRIVATE
        rsm_unified
        benchmark::benchmark
        benchmark::benchmark_main
        Threads::Threads
)

# ============================================================================
# CTest Integration
# ============================================================================

# Quick benchmark test (default, runs with ctest)
add_test(
    NAME benchmark_event_scheduler_quick
    COMMAND benchmark_event_scheduler
        --benchmark_min_time=0.1s
        --benchmark_filter=Quick
        --benchmark_format=console
)

# Full benchmark test (disabled by default, manual execution only)
add_test(
    NAME benchmark_event_scheduler_full
    COMMAND benchmark_event_scheduler
)

# Configure test properties
# CTest executes tests with higher COST first, so quick (1000) runs before full (500)
set_tests_properties(benchmark_event_scheduler_quick PROPERTIES
    LABELS "benchmark;quick;performance"
    TIMEOUT 300
    COST 1000  # Runs before full benchmark
)

# Disable full benchmark from default ctest run
set_tests_properties(benchmark_event_scheduler_full PROPERTIES
    LABELS "benchmark;full;performance;manual"
    TIMEOUT 600
    COST 500  # Lower cost = runs after quick benchmark
    DISABLED TRUE  # Excluded from default ctest execution
)

# ============================================================================
# Custom Benchmark Targets
# ============================================================================

# Run benchmark with standard settings
add_custom_target(run_benchmarks
    COMMAND benchmark_event_scheduler --benchmark_format=console
    DEPENDS benchmark_event_scheduler
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Generate JSON results
add_custom_target(benchmark_json
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/benchmark_results
    COMMAND benchmark_event_scheduler --benchmark_format=json > ${CMAKE_BINARY_DIR}/benchmark_results/event_scheduler.json
    DEPENDS benchmark_event_scheduler
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Quick performance check (fewer iterations)
add_custom_target(benchmark_quick
    COMMAND benchmark_event_scheduler --benchmark_min_time=0.1s --benchmark_filter=Quick
    DEPENDS benchmark_event_scheduler
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
