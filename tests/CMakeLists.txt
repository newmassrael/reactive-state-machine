cmake_minimum_required(VERSION 3.14)

# GoogleTest 설정 - FetchContent 방식
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "GTest not found on system, using FetchContent to download")

    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    # GoogleTest 옵션 설정 (빌드 시간 단축)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    set(BUILD_GTEST ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    # 별칭 생성 (find_package와 동일한 인터페이스 제공)
    add_library(GTest::GTest ALIAS gtest)
    add_library(GTest::Main ALIAS gtest_main)
    add_library(GTest::gtest ALIAS gtest)
    add_library(GTest::gtest_main ALIAS gtest_main)
    add_library(GTest::gmock ALIAS gmock)
    add_library(GTest::gmock_main ALIAS gmock_main)
else()
    message(STATUS "Using system-installed GTest")
endif()

# ============================================================================
# RSM 테스트 설정: std::cout 사용 금지 적용
# ============================================================================
function(apply_rsm_test_policies target_name)
    # DisableStdOut.h를 모든 테스트 타겟에 강제 포함
    target_compile_options(${target_name}
        PRIVATE
            -include ${CMAKE_SOURCE_DIR}/rsm/include/common/DisableStdOut.h
    )

    # RSM 테스트 정책 정의
    target_compile_definitions(${target_name}
        PRIVATE
            RSM_TEST_TARGET=1  # 테스트 타겟 식별용
    )
endfunction()

# ============================================================================
# Performance Benchmarks (Optional)
# ============================================================================
# 모든 테스트 타겟 이름 리스트
set(RSM_TEST_TARGETS
    rsm_scxml_tests
    js_engine_tests
    state_machine_tests
    states_tests
    action_tests
    history_tests
    core_parser_tests
    integration_tests
    concurrency_tests
    txml_converter_tests
    w3c_test_cli
    w3c_full_runner
    w3c_simple_tests
    static_codegen_tests
    # simple_scxml_engine_tests  # Temporarily disabled
)

# ============================================================================
# 1. RSM SCXML Builtin Tests (현재 작동하는 테스트)
# ============================================================================
add_executable(rsm_scxml_tests
    simple_scxml_test.cpp
    core/TypeRegistryTest.cpp
    concurrency/TypeRegistryThreadSafetyTest.cpp
    common/EventDataHelperTest.cpp
)

target_include_directories(rsm_scxml_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(rsm_scxml_tests
    PRIVATE
        rsm_unified
        GTest::gtest_main
        pthread
)

# RSM 테스트 정책 적용 (cout 금지 등)
apply_rsm_test_policies(rsm_scxml_tests)

# CTest 등록은 하단에서 최적화된 순서로 일괄 처리

# ============================================================================
# 2. JavaScript Engine Tests (완전히 RSM으로 업데이트됨)
# ============================================================================
add_executable(js_engine_tests
    engine/SessionManagementTest.cpp
    engine/DataModelTest.cpp
    engine/EventSystemTest.cpp
    engine/JSEngineBasicTest.cpp
    engine/JSResultTypeConversionTest.cpp
    mocks/MockEventRaiser.cpp
)

target_include_directories(js_engine_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(js_engine_tests
    PRIVATE
        rsm_unified
        GTest::gtest_main
        pthread
)

# RSM 테스트 정책 적용
apply_rsm_test_policies(js_engine_tests)

# ============================================================================
# 3. StateMachine Tests
# ============================================================================
add_executable(state_machine_tests
    engine/StateMachineTest.cpp
)

target_include_directories(state_machine_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(state_machine_tests
    PRIVATE
        rsm_unified
        GTest::gtest_main
        pthread
)

# ============================================================================
# 3.5. States Tests (ConcurrentRegion Unit Tests)
# ============================================================================
add_executable(states_tests
    states/ConcurrentRegionTest.cpp
    mocks/MockActionExecutor.cpp
)

target_include_directories(states_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(states_tests
    PRIVATE
        rsm_unified
        GTest::gtest_main
        pthread
)

# ============================================================================
# 4. Action System Tests (SCXML Executable Content)
# ============================================================================

add_executable(action_tests
    actions/ScriptActionTest.cpp
    actions/AssignActionTest.cpp
    runtime/ActionExecutorImplTest.cpp
    mocks/MockActionExecutor.cpp
    mocks/MockEventRaiser.cpp
)

target_include_directories(action_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(action_tests
    PRIVATE
        rsm_unified
        GTest::gtest_main
        pthread
)

# Note: ActionTests will be registered below with all other tests

# ============================================================================
# 4.5. History Manager Tests (SOLID SCXML W3C Compliance)
# ============================================================================

add_executable(history_tests
    runtime/HistoryManagerTest.cpp
)

target_include_directories(history_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(history_tests
    PRIVATE
        rsm_unified
        GTest::gtest_main
        pthread
)

# ============================================================================
# 4.6. ReadySCXMLEngine API Tests (High-level Facade) - TEMPORARILY DISABLED
# ============================================================================

# Temporarily disabled - will be re-enabled when converting existing tests
# add_executable(simple_scxml_engine_tests
#     api/ReadySCXMLEngineTest.cpp
# )
#
# target_include_directories(simple_scxml_engine_tests
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}
#         ${CMAKE_SOURCE_DIR}/rsm/include
#         ${CMAKE_SOURCE_DIR}/tests
# )
#
# target_link_libraries(simple_scxml_engine_tests
#     PRIVATE
#         rsm_unified
#         GTest::gtest_main
#         pthread
# )

# ============================================================================
# 5. Core Parser Tests (RSM namespace updated)
# ============================================================================

# Use same libxml++ configuration as RSM
set(LIBXMLXX_INSTALL_DIR ${CMAKE_BINARY_DIR}/libxmlxx_install)
set(LIBXMLXX_INCLUDE_DIRS
    ${LIBXMLXX_INSTALL_DIR}/include/libxml++-5.0
    ${LIBXMLXX_INSTALL_DIR}/lib/libxml++-5.0/include
)
set(LIBXMLXX_LIBRARIES ${LIBXMLXX_INSTALL_DIR}/lib/libxml++-5.0.so)

add_executable(core_parser_tests
    core/SCXMLParserBasicTest.cpp
)

add_dependencies(core_parser_tests libxmlxx_external rsm_unified)

target_include_directories(core_parser_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tests
        ${LIBXMLXX_INCLUDE_DIRS}
        /usr/include/libxml2
)

target_link_libraries(core_parser_tests
    PRIVATE
        rsm_unified
        GTest::gtest_main
        ${LIBXMLXX_LIBRARIES}
        xml2
        pthread
)

# ============================================================================
# 5. Concurrent States Foundation Tests
# ============================================================================
# 테스트 파일들이 제거되어 비활성화됨
# add_executable(concurrent_foundation_tests
#     states/concurrent/foundation/ConcurrentStateFoundationTest.cpp
#     mocks/MockActionExecutor.cpp
# )

# ============================================================================
# 6. Concurrent States Orchestration Tests
# ============================================================================
# add_executable(concurrent_orchestration_tests
#     states/concurrent/orchestration/ParallelRegionOrchestratorTest.cpp
#     mocks/MockActionExecutor.cpp
# )

# ============================================================================
# 7. Concurrent Event Broadcasting Tests
# ============================================================================
# add_executable(concurrent_event_tests
#     states/concurrent/events/ConcurrentEventBroadcasterTest.cpp
#     mocks/MockActionExecutor.cpp
# )

# ============================================================================
# 8. Concurrent Completion Monitoring Tests
# ============================================================================
# add_executable(concurrent_monitoring_tests
#     states/concurrent/monitoring/ConcurrentCompletionMonitorTest.cpp
#     mocks/MockActionExecutor.cpp
# )

# ============================================================================
# 9. External Transition Handling Tests
# ============================================================================
# add_executable(external_transition_tests
#     states/concurrent/transitions/ExternalTransitionHandlerTest.cpp
#     mocks/MockActionExecutor.cpp
# )

# 테스트 설정들이 제거되어 비활성화됨

# ============================================================================
# 6. Integration Tests (RSM architecture)
# ============================================================================
add_executable(integration_tests
    integration/StateMachineIntegrationTest.cpp
    integration/ActionIntegrationTest.cpp
    integration/SCXMLEventTest.cpp
    integration/EventSchedulingTest.cpp
    integration/HttpEventTargetTest.cpp
    integration/SimpleMockHttpServer.cpp
    integration/ParallelStateIntegrationTest.cpp
    integration/SCXMLParallelParsingTest.cpp
    integration/ParallelStateEndToEndTest.cpp
    integration/SCXMLParallelComplianceTest.cpp
    integration/ConcurrentCompletionMonitoringTest.cpp
    integration/SCXMLForeachIntegrationTest.cpp
    integration/HistoryStateIntegrationTest.cpp
    integration/StaticCodegenIntegrationTest.cpp
    w3c/W3CHttpTestServer.cpp
    mocks/MockActionExecutor.cpp
    mocks/MockEventRaiser.cpp
)

add_dependencies(integration_tests libxmlxx_external rsm_unified)

target_include_directories(integration_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tests
        ${LIBXMLXX_INCLUDE_DIRS}
        /usr/include/libxml2
)

target_link_libraries(integration_tests
    PRIVATE
        rsm_unified
        GTest::gtest_main
        GTest::gmock
        ${LIBXMLXX_LIBRARIES}
        xml2
        pthread
)

# ============================================================================
# 6. Concurrency and Deadlock Tests
# ============================================================================
add_executable(concurrency_tests
    concurrency/DeadlockReproductionTest.cpp
    mocks/MockEventRaiser.cpp
)

target_include_directories(concurrency_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(concurrency_tests
    PRIVATE
        rsm_unified
        GTest::gtest_main
        GTest::gmock
        pthread
)

# ============================================================================
# 7. TXML Converter Tests (W3C SCXML Compliance Testing)
# ============================================================================
add_executable(txml_converter_tests
    w3c/TXMLConverterTest.cpp
    w3c/impl/TXMLConverter.cpp
)

target_include_directories(txml_converter_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tests
        ${LIBXMLXX_INCLUDE_DIRS}
        /usr/include/libxml2
)

target_link_libraries(txml_converter_tests
    PRIVATE
        rsm_unified
        GTest::gtest_main
        ${LIBXMLXX_LIBRARIES}
        xml2
        pthread
)

add_dependencies(txml_converter_tests libxmlxx_external rsm_unified)

# ============================================================================
# 8. W3C SCXML Compliance Tests (SOLID Architecture)
# ============================================================================
# Output directory for generated static W3C test code (used by both w3c_test_cli and w3c_static_test_cli)
set(STATIC_W3C_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/w3c_static_generated")

# Generate C++ code for W3C tests BEFORE defining executables
include(${CMAKE_SOURCE_DIR}/cmake/RSMStaticW3CTest.cmake)
rsm_generate_static_w3c_test(144 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(147 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(148 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(149 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(150 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(151 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(152 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(153 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(155 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(156 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(158 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(159 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(172 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(173 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(174 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(175 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(176 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(178 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(179 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(183 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(185 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(186 ${STATIC_W3C_OUTPUT_DIR})
# W3C SCXML 6.4: test187 uses dynamic invoke - generates Interpreter wrapper
rsm_generate_static_w3c_test(187 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(189 ${STATIC_W3C_OUTPUT_DIR})
rsm_generate_static_w3c_test(190 ${STATIC_W3C_OUTPUT_DIR})
# W3C SCXML 6.4: Dynamic invoke tests (inline content, srcexpr, contentExpr) - generate Interpreter wrappers
rsm_generate_static_w3c_test(191 ${STATIC_W3C_OUTPUT_DIR})  # inline <content>
rsm_generate_static_w3c_test(192 ${STATIC_W3C_OUTPUT_DIR})  # inline <content>
rsm_generate_static_w3c_test(193 ${STATIC_W3C_OUTPUT_DIR})  # type attribute for queue routing (W3C SCXML 6.2.4)
rsm_generate_static_w3c_test(194 ${STATIC_W3C_OUTPUT_DIR})  # invalid target raises error.execution (W3C SCXML 6.2)
rsm_generate_static_w3c_test(198 ${STATIC_W3C_OUTPUT_DIR})  # default event processor type (W3C SCXML 6.2)
rsm_generate_static_w3c_test(199 ${STATIC_W3C_OUTPUT_DIR})  # unsupported send type raises error.execution (W3C SCXML 6.2)
rsm_generate_static_w3c_test(200 ${STATIC_W3C_OUTPUT_DIR})  # SCXML event processor support (W3C SCXML 6.2)
rsm_generate_static_w3c_test(201 ${STATIC_W3C_OUTPUT_DIR})  # BasicHTTP event processor (optional - W3C SCXML 6.2)
rsm_generate_static_w3c_test(205 ${STATIC_W3C_OUTPUT_DIR})  # inline <content>
rsm_generate_static_w3c_test(207 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> (invoke cancellation across sessions)
rsm_generate_static_w3c_test(208 ${STATIC_W3C_OUTPUT_DIR})  # <cancel sendid="..."/> for delayed send cancellation (W3C SCXML 6.3)
rsm_generate_static_w3c_test(210 ${STATIC_W3C_OUTPUT_DIR})  # sendidexpr (cancel with dynamic expression)
rsm_generate_static_w3c_test(215 ${STATIC_W3C_OUTPUT_DIR})  # typeexpr + inline <content>
rsm_generate_static_w3c_test(216 ${STATIC_W3C_OUTPUT_DIR})  # srcexpr
rsm_generate_static_w3c_test(220 ${STATIC_W3C_OUTPUT_DIR})  # inline <content>
rsm_generate_static_w3c_test(223 ${STATIC_W3C_OUTPUT_DIR})  # inline <content>
rsm_generate_static_w3c_test(224 ${STATIC_W3C_OUTPUT_DIR})  # inline <content>
rsm_generate_static_w3c_test(225 ${STATIC_W3C_OUTPUT_DIR})  # inline <content>

rsm_generate_static_w3c_test(226 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 6.2/6.4: auto-discovers test226sub1.txml

rsm_generate_static_w3c_test(228 ${STATIC_W3C_OUTPUT_DIR})  # inline <content>
rsm_generate_static_w3c_test(229 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + autoforward
rsm_generate_static_w3c_test(230 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + autoforward (manual)
rsm_generate_static_w3c_test(232 ${STATIC_W3C_OUTPUT_DIR})  # inline <content>
rsm_generate_static_w3c_test(233 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + finalize
rsm_generate_static_w3c_test(234 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + finalize + parallel
rsm_generate_static_w3c_test(235 ${STATIC_W3C_OUTPUT_DIR})  # inline <content>
rsm_generate_static_w3c_test(236 ${STATIC_W3C_OUTPUT_DIR})  # inline <content>
rsm_generate_static_w3c_test(237 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + cancellation
rsm_generate_static_w3c_test(240 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + namelist + params
rsm_generate_static_w3c_test(241 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + namelist/param consistency
rsm_generate_static_w3c_test(242 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + static src (mixed invoke - needs test242sub1.scxml)
rsm_generate_static_w3c_test(243 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + param
rsm_generate_static_w3c_test(244 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + namelist
rsm_generate_static_w3c_test(245 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + namelist (non-existent var)
rsm_generate_static_w3c_test(247 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + done.invoke
rsm_generate_static_w3c_test(250 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + cancellation (manual test)
rsm_generate_static_w3c_test(252 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + event cancellation after invoke termination
rsm_generate_static_w3c_test(253 ${STATIC_W3C_OUTPUT_DIR})  # inline <content> + bidirectional SCXML event processor

rsm_generate_static_w3c_test(276 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 6.2/6.4: auto-discovers test276sub1.txml
rsm_generate_static_w3c_test(277 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 5.3: datamodel init error.execution
rsm_generate_static_w3c_test(278 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 5.10: datamodel in non-entered state (requires wrapper)
rsm_generate_static_w3c_test(279 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 5.2.2: early binding variable initialization
rsm_generate_static_w3c_test(280 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 5.3: late binding variable initialization
rsm_generate_static_w3c_test(286 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 5.3: error.execution for invalid assignment location
rsm_generate_static_w3c_test(287 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 5.4: valid assignment to valid location
rsm_generate_static_w3c_test(294 ${STATIC_W3C_OUTPUT_DIR})  # dynamic donedata (param/content requires runtime evaluation)
rsm_generate_static_w3c_test(298 ${STATIC_W3C_OUTPUT_DIR})  # dynamic donedata (invalid param location requires runtime evaluation)
rsm_generate_static_w3c_test(301 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 5.8: script element download timeout rejection (manual)
rsm_generate_static_w3c_test(302 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 5.8: script evaluation at load time
rsm_generate_static_w3c_test(303 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 5.9: script execution in entry actions
rsm_generate_static_w3c_test(304 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 5.8: script-declared variables accessible in data model

rsm_generate_static_w3c_test(355 ${STATIC_W3C_OUTPUT_DIR})  # no initial state (default to first child in document order)
rsm_generate_static_w3c_test(364 ${STATIC_W3C_OUTPUT_DIR})  # parallel initial state format (space-separated IDs)
rsm_generate_static_w3c_test(372 ${STATIC_W3C_OUTPUT_DIR})  # no initial state (done.state.parentid after final)
rsm_generate_static_w3c_test(375 ${STATIC_W3C_OUTPUT_DIR})  # no initial state (onentry execution order)
rsm_generate_static_w3c_test(376 ${STATIC_W3C_OUTPUT_DIR})  # no initial state (onentry error handling)
rsm_generate_static_w3c_test(377 ${STATIC_W3C_OUTPUT_DIR})  # no initial state (onexit execution order)
rsm_generate_static_w3c_test(378 ${STATIC_W3C_OUTPUT_DIR})  # no initial state (onexit error handling)
rsm_generate_static_w3c_test(239 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 6.4: auto-discovers test239sub1.txml
rsm_generate_static_w3c_test(242 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 6.4: auto-discovers test242sub1.txml

# Full W3C Test Runner for all W3C SCXML 1.0 compliance tests
add_executable(w3c_test_cli
    w3c/W3CTestCLI.cpp
    w3c/W3CTestRunner.cpp
    w3c/impl/TXMLConverter.cpp
    w3c/impl/TestMetadataParser.cpp
    w3c/W3CHttpTestServer.cpp
    w3c/impl/W3CTestEventDispatcher.cpp
    mocks/MockEventRaiser.cpp
    ${GENERATED_W3C_HEADERS}  # Trigger generation of static test headers for JIT engine
)

target_link_libraries(w3c_test_cli
    PRIVATE
        rsm_unified
        ${LIBXMLXX_LIBRARIES}
        xml2
        pthread
)

target_include_directories(w3c_test_cli
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/tests/w3c  # JIT test registry headers
        ${STATIC_W3C_OUTPUT_DIR}  # Include generated static test headers
        ${LIBXMLXX_INCLUDE_DIRS}
        /usr/include/libxml2
)

add_dependencies(w3c_test_cli libxmlxx_external rsm_unified)

# ============================================================================
# 9. Static Code Generation Tests (TDD for Static Compiler)
# ============================================================================
add_executable(static_codegen_tests
    codegen/test_static_codegen.cpp
    ../tools/codegen/StaticCodeGenerator.cpp
)

target_include_directories(static_codegen_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/rsm/include
        ${CMAKE_SOURCE_DIR}/tools/codegen
        ${CMAKE_SOURCE_DIR}/tests
        ${LIBXMLXX_INCLUDE_DIRS}
        /usr/include/libxml2
)

target_link_libraries(static_codegen_tests
    PRIVATE
        rsm_unified
        GTest::gtest_main
        ${LIBXMLXX_LIBRARIES}
        xml2
        pthread
)

add_dependencies(static_codegen_tests libxmlxx_external rsm_unified)

# ============================================================================
# CTest 실행 순서 최적화 (빠른 피드백 → 종합 검증 → 성능 측정)
# ============================================================================

# Phase 1: 빠른 단위 테스트 (개발 중 빠른 피드백, <1초)
add_test(NAME CoreParserTests COMMAND core_parser_tests)
add_test(NAME StatesTests COMMAND states_tests)
add_test(NAME HistoryTests COMMAND history_tests)

# Phase 2: 핵심 컴포넌트 테스트 (기본 기능 검증, ~1-5초)
add_test(NAME StateMachineTests COMMAND state_machine_tests)
add_test(NAME JSEngineTests COMMAND js_engine_tests)
add_test(NAME ActionTests COMMAND action_tests)
add_test(NAME RSMSCXMLTests COMMAND rsm_scxml_tests)
add_test(NAME StaticCodegenTests COMMAND static_codegen_tests)
# add_test(NAME ReadySCXMLEngineTests COMMAND simple_scxml_engine_tests)  # Temporarily disabled

# Phase 3: 통합 및 동시성 테스트 (시스템 레벨 검증, ~5-15초)
add_test(NAME TXMLConverterTests COMMAND txml_converter_tests)
add_test(NAME ConcurrencyTests COMMAND concurrency_tests)
add_test(NAME IntegrationTests COMMAND integration_tests)

# Phase 4: W3C 표준 준수 검증 (종합 검증, ~40초)
add_test(NAME W3CFullComplianceTests COMMAND w3c_test_cli)

# ============================================================================
# CTest 실행 순서 제어: COST 속성으로 우선순위 설정
# CTest는 높은 COST를 가진 테스트를 먼저 실행하므로,
# 벤치마크(COST=1000)보다 높은 값을 메인 테스트에 부여하여 순서 보장
# ============================================================================

# Phase 1: COST 5000 (가장 먼저 실행)
set_tests_properties(CoreParserTests PROPERTIES COST 5000)
set_tests_properties(StatesTests PROPERTIES COST 5000)
set_tests_properties(HistoryTests PROPERTIES COST 5000)

# Phase 2: COST 4000
set_tests_properties(StateMachineTests PROPERTIES COST 4000)
set_tests_properties(JSEngineTests PROPERTIES COST 4000)
set_tests_properties(ActionTests PROPERTIES COST 4000)
set_tests_properties(RSMSCXMLTests PROPERTIES COST 4000)
set_tests_properties(StaticCodegenTests PROPERTIES COST 4000)

# Phase 3: COST 3000
set_tests_properties(TXMLConverterTests PROPERTIES COST 3000)
set_tests_properties(ConcurrencyTests PROPERTIES COST 3000)
set_tests_properties(IntegrationTests PROPERTIES COST 3000)

# Phase 4: COST 2000 (벤치마크보다 높음)
set_tests_properties(W3CFullComplianceTests PROPERTIES COST 2500)

# Phase 5: 성능 벤치마크 (기능 검증 후 성능 측정, 마지막 실행)
# Benchmark subdirectory must be included AFTER all main tests to ensure proper execution order
option(BUILD_BENCHMARKS "Build performance benchmarks" ON)
if(BUILD_BENCHMARKS)
    message(STATUS "Building performance benchmarks")
    add_subdirectory(benchmarks)
endif()

# ============================================================================
# 테스트 의존성 및 우선순위 설정
# ============================================================================

# 핵심 테스트들이 먼저 통과해야 고급 테스트 실행
set_tests_properties(ActionTests PROPERTIES DEPENDS "JSEngineTests;StateMachineTests")
set_tests_properties(HistoryTests PROPERTIES DEPENDS "StateMachineTests")
set_tests_properties(StatesTests PROPERTIES DEPENDS "StateMachineTests")

# 통합 테스트는 핵심 컴포넌트들이 통과한 후 실행
set_tests_properties(IntegrationTests PROPERTIES DEPENDS "JSEngineTests;StateMachineTests;ActionTests")
set_tests_properties(ConcurrencyTests PROPERTIES DEPENDS "StateMachineTests;StatesTests")

# W3C 테스트는 모든 기본 기능이 검증된 후 실행
set_tests_properties(TXMLConverterTests PROPERTIES DEPENDS "CoreParserTests")
set_tests_properties(W3CFullComplianceTests PROPERTIES DEPENDS "IntegrationTests;TXMLConverterTests")

# 테스트 타임아웃 설정 (초 단위)
set_tests_properties(RSMSCXMLTests PROPERTIES TIMEOUT 30)
set_tests_properties(StaticCodegenTests PROPERTIES TIMEOUT 30)
# set_tests_properties(ReadySCXMLEngineTests PROPERTIES TIMEOUT 30)  # Temporarily disabled
set_tests_properties(JSEngineTests CoreParserTests StateMachineTests PROPERTIES TIMEOUT 60)
set_tests_properties(ActionTests HistoryTests StatesTests PROPERTIES TIMEOUT 90)
set_tests_properties(IntegrationTests ConcurrencyTests PROPERTIES TIMEOUT 180)
set_tests_properties(TXMLConverterTests PROPERTIES TIMEOUT 120)
set_tests_properties(W3CFullComplianceTests PROPERTIES TIMEOUT 300)

# 병렬 실행 제한 (리소스 집약적 테스트)
set_tests_properties(W3CFullComplianceTests PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(ConcurrencyTests PROPERTIES RUN_SERIAL TRUE)

# 모든 테스트 타겟에 RSM 정책 일괄 적용
foreach(test_target ${RSM_TEST_TARGETS})
    if(TARGET ${test_target})
        apply_rsm_test_policies(${test_target})
        message(STATUS "✅ Applied RSM policies (cout prohibition) to ${test_target}")
    endif()
endforeach()

message(STATUS "✅ RSM SCXML tests enabled")
message(STATUS "✅ JavaScript Engine tests enabled")
message(STATUS "✅ Core Parser tests enabled")
message(STATUS "✅ Integration tests enabled")
message(STATUS "🚫 std::cout usage prohibited in all test targets")
