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
)

# ============================================================================
# 1. RSM SCXML Builtin Tests (현재 작동하는 테스트)
# ============================================================================
add_executable(rsm_scxml_tests
    simple_scxml_test.cpp
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

# CTest에 작동하는 테스트 추가
add_test(NAME RSMSCXMLTests COMMAND rsm_scxml_tests)

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
# Full W3C Test Runner for all W3C SCXML 1.0 compliance tests
add_executable(w3c_test_cli
    w3c/W3CTestCLI.cpp
    w3c/W3CTestRunner.cpp
    w3c/impl/TXMLConverter.cpp
    w3c/impl/TestMetadataParser.cpp

    w3c/impl/W3CTestEventDispatcher.cpp
    mocks/MockEventRaiser.cpp
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
        ${LIBXMLXX_INCLUDE_DIRS}
        /usr/include/libxml2
)

add_dependencies(w3c_test_cli libxmlxx_external rsm_unified)

# CTest에 모든 테스트들 추가
add_test(NAME JSEngineTests COMMAND js_engine_tests)
add_test(NAME StateMachineTests COMMAND state_machine_tests)
add_test(NAME ActionTests COMMAND action_tests)
add_test(NAME HistoryTests COMMAND history_tests)
add_test(NAME CoreParserTests COMMAND core_parser_tests)
add_test(NAME TXMLConverterTests COMMAND txml_converter_tests)
# 테스트 등록이 제거되어 비활성화됨
# add_test(NAME ConcurrentFoundationTests COMMAND concurrent_foundation_tests)
# add_test(NAME ConcurrentOrchestrationTests COMMAND concurrent_orchestration_tests)
# add_test(NAME ConcurrentEventTests COMMAND concurrent_event_tests)
# add_test(NAME ConcurrentMonitoringTests COMMAND concurrent_monitoring_tests)
# add_test(NAME ExternalTransitionTests COMMAND external_transition_tests)
add_test(NAME IntegrationTests COMMAND integration_tests)
add_test(NAME ConcurrencyTests COMMAND concurrency_tests)

# W3C Compliance Test Categories
# W3C Tests는 가장 포괄적이고 시간이 오래 걸리므로 별도 카테고리로 분리
add_test(NAME W3CFullComplianceTests COMMAND w3c_test_cli)

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