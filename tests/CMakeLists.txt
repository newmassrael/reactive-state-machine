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

# 컴파일 옵션 추가
add_compile_options(${LIBXMLPP_CFLAGS})

# 공통 소스 파일 (generator 구현 클래스)
set(GENERATOR_SOURCES
    # 모델 구현 클래스
    ${CMAKE_SOURCE_DIR}/generator/src/impl/StateNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/impl/TransitionNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/impl/GuardNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/impl/ActionNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/impl/DataModelItem.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/impl/InvokeNode.cpp

    # 모델 클래스
    ${CMAKE_SOURCE_DIR}/generator/src/model/SCXMLContext.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/model/SCXMLModel.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/model/StateHierarchy.cpp

    # 파싱 클래스
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/SCXMLParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/StateNodeParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/TransitionParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/ActionParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/GuardParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/DataModelParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/XIncludeProcessor.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/ParsingCommon.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/InvokeParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/DoneDataParser.cpp

    # 팩토리 클래스
    ${CMAKE_SOURCE_DIR}/generator/src/factory/NodeFactory.cpp

    # 유틸리티
    ${CMAKE_SOURCE_DIR}/generator/src/GuardUtils.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/Logger.cpp
)

# 1. 상태 머신 예제 테스트 (examples 디렉토리 테스트)
add_executable(state_machine_example_tests
    examples/StateMachineTest.cpp
)

target_include_directories(state_machine_example_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/examples
        ${CMAKE_SOURCE_DIR}/generator/include
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(state_machine_example_tests
    PRIVATE
        state_machine_lib
        scxml_generator
        GTest::gtest_main
        GTest::gmock_main
        pthread
)

add_test(NAME StateMachineExampleTests COMMAND state_machine_example_tests)

# 2. 제너레이터 테스트 (generator 디렉토리 테스트)
add_executable(generator_tests
    generator/SCXMLParserBasicTest.cpp
    generator/SCXMLParserCommunicationTest.cpp
    generator/SCXMLParserDataModelTest.cpp
    generator/SCXMLParserErrorTest.cpp
    generator/SCXMLParserEventTest.cpp
    generator/SCXMLParserExecutableTest.cpp
    generator/SCXMLParserHistoryTest.cpp
    generator/SCXMLParserInvokeTest.cpp
    generator/SCXMLParserStateTest.cpp
    generator/SCXMLParserTransitionTest.cpp
)

target_include_directories(generator_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/tests/mocks
        ${LIBXMLPP_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/generator/src
        ${CMAKE_SOURCE_DIR}/generator/src/impl
        ${CMAKE_SOURCE_DIR}/generator/src/model
        ${CMAKE_SOURCE_DIR}/generator/src/parsing
        ${CMAKE_SOURCE_DIR}/generator/src/factory
        ${CMAKE_SOURCE_DIR}/generator/include
        ${CMAKE_SOURCE_DIR}/generator/include/impl
        ${CMAKE_SOURCE_DIR}/generator/include/model
        ${CMAKE_SOURCE_DIR}/generator/include/parsing
        ${CMAKE_SOURCE_DIR}/generator/include/factory
)

# generator_tests 빌드에 소스 코드를 별도로 추가 (객체 파일 중복 방지)
target_sources(generator_tests
    PRIVATE
        ${GENERATOR_SOURCES}
)

target_link_libraries(generator_tests
    PRIVATE
        ${LIBXMLPP_LIBRARIES}
        GTest::gtest_main
        GTest::gmock_main
        pthread
)

# CTest에 generator 테스트 추가
add_test(NAME GeneratorTests COMMAND generator_tests)

# 3. JavaScript Engine 테스트 (js_engine 디렉토리 테스트)
add_executable(js_engine_tests
    js_engine/SessionManagementTest.cpp
    js_engine/DataModelTest.cpp
    js_engine/EventSystemTest.cpp
    js_engine/SimpleDebugTest.cpp
)

target_include_directories(js_engine_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/scxml/include
        ${CMAKE_SOURCE_DIR}/scxml/runtime
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(js_engine_tests
    PRIVATE
        scxml_engine
        GTest::gtest_main
        pthread
)

# CTest에 js_engine 테스트 추가
add_test(NAME JSEngineTests COMMAND js_engine_tests)

# 4. 기존 통합 테스트 (옵션으로 유지)
option(BUILD_INTEGRATED_TESTS "Build integrated tests that include both examples and generator" OFF)

if(BUILD_INTEGRATED_TESTS)
    # 모든 테스트 소스 파일 설정
    add_executable(integrated_tests
        examples/state_machine_test.cpp
        generator/SCXMLParserBasicTest.cpp
        generator/SCXMLParserCommunicationTest.cpp
        generator/SCXMLParserDataModelTest.cpp
        generator/SCXMLParserErrorTest.cpp
        generator/SCXMLParserEventTest.cpp
        generator/SCXMLParserExecutableTest.cpp
        generator/SCXMLParserHistoryTest.cpp
        generator/SCXMLParserInvokeTest.cpp
        generator/SCXMLParserStateTest.cpp
        generator/SCXMLParserTransitionTest.cpp
    )

    # integrated_tests 빌드에 소스 코드를 별도로 추가
    target_sources(integrated_tests
        PRIVATE
            ${GENERATOR_SOURCES}
    )

    # include 디렉토리 추가 - integrated_tests 전용
    target_include_directories(integrated_tests
        PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/tests/mocks
            ${LIBXMLPP_INCLUDE_DIRS}
            ${CMAKE_SOURCE_DIR}/generator/src
            ${CMAKE_SOURCE_DIR}/generator/src/impl
            ${CMAKE_SOURCE_DIR}/generator/src/model
            ${CMAKE_SOURCE_DIR}/generator/src/parsing
            ${CMAKE_SOURCE_DIR}/generator/src/factory
            ${CMAKE_SOURCE_DIR}/generator/include
            ${CMAKE_SOURCE_DIR}/generator/include/impl
            ${CMAKE_SOURCE_DIR}/generator/include/model
            ${CMAKE_SOURCE_DIR}/generator/include/parsing
            ${CMAKE_SOURCE_DIR}/generator/include/factory
    )

    # 테스트에 필요한 라이브러리 링크
    target_link_libraries(integrated_tests
        PRIVATE
            state_machine_lib
            scxml_generator
            ${LIBXMLPP_LIBRARIES}
            GTest::gtest_main
            GTest::gmock_main
            pthread
    )

    # 통합 테스트 실행 파일을 CTest에 추가
    add_test(NAME IntegratedTests COMMAND integrated_tests)
endif()
