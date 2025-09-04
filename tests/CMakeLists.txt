cmake_minimum_required(VERSION 3.16)

# GoogleTest 패키지 찾기
find_package(GTest REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBXMLPP REQUIRED libxml++-5.0)

# 컴파일 옵션 추가
add_compile_options(${LIBXMLPP_CFLAGS})

# 공통 소스 파일 (generator 구현 클래스)
set(GENERATOR_SOURCES
    # 모델 구현 클래스
    ${CMAKE_SOURCE_DIR}/generator/src/core/StateNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/TransitionNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/GuardNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/ActionNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/DataModelItem.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/InvokeNode.cpp

    # 액션 구현 클래스들 (모든 액션 포함)
    ${CMAKE_SOURCE_DIR}/generator/src/core/actions/IfActionNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/actions/SendActionNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/actions/RaiseActionNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/actions/LogActionNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/actions/AssignActionNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/actions/CancelActionNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/actions/ScriptActionNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/actions/ForeachActionNode.cpp

    # 모델 클래스
    ${CMAKE_SOURCE_DIR}/generator/src/model/DocumentModel.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/model/StateHierarchy.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/model/ParsingContext.cpp

    # 핵심 impl 클래스들
    ${CMAKE_SOURCE_DIR}/generator/src/core/DataNode.cpp

    # 파싱 클래스
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/DocumentParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/ExecutableContentParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/NamespaceAwareParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/StateNodeParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/TransitionParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/ActionParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/GuardParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/DataModelParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/XIncludeProcessor.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/ParsingCommon.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/InvokeParser.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/parsing/DoneDataParser.cpp


    # 이벤트 시스템
    ${CMAKE_SOURCE_DIR}/generator/src/events/Event.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/events/EventQueue.cpp

    # 런타임 클래스들 (핵심 구성 - 완전히 구현된 것들만)
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/RuntimeContext.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/GuardEvaluator.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/DataModelEngine.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/HistoryStateManager.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/HistoryStateTransitionHandler.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/ExpressionEvaluator.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/ECMAScriptEngineFactory.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/JavaScriptEvaluatorAdapter.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/impl/StateManager.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/impl/DataContextManager.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/impl/InvokeSessionManager.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/impl/EventManager.cpp
    # 런타임 구성요소들 - 누락된 메서드 구현 완료로 활성화
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/ExecutableContentProcessor.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/JavaScriptExpressionEvaluator.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/EventSystemBridge.cpp

    # 실행 처리기
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/ActionProcessor.cpp

    # ActionExecutor 패턴 구현
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/ActionExecutor.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/executors/AssignActionExecutor.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/executors/LogActionExecutor.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/executors/RaiseActionExecutor.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/executors/SendActionExecutor.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/executors/ScriptActionExecutor.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/executors/CancelActionExecutor.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/executors/ForeachActionExecutor.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/runtime/executors/IfActionExecutor.cpp

    # 팩토리 클래스
    ${CMAKE_SOURCE_DIR}/generator/src/core/NodeFactory.cpp

    # 공통 유틸리티
    ${CMAKE_SOURCE_DIR}/generator/src/common/IdGenerator.cpp

    # 유틸리티
    ${CMAKE_SOURCE_DIR}/generator/src/common/GuardUtils.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/common/Logger.cpp
)

# 공통 include 디렉토리
set(COMMON_INCLUDE_DIRS
    ${GTEST_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/examples
    ${CMAKE_SOURCE_DIR}/generator/include
    ${CMAKE_SOURCE_DIR}/generator/include/impl
    ${CMAKE_SOURCE_DIR}/generator/include/model
    ${CMAKE_SOURCE_DIR}/generator/include/parsing
    ${CMAKE_SOURCE_DIR}/generator/include/factory
    ${CMAKE_SOURCE_DIR}/generator/include/runtime
    ${CMAKE_SOURCE_DIR}/generator/src
    ${LIBXMLPP_INCLUDE_DIRS}
)

# 공통 링크 라이브러리
set(COMMON_LINK_LIBS
    ${LIBXMLPP_LIBRARIES}
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    gmock
    gmock_main
    pthread
)

# Generator 테스트들 빌드
file(GLOB GENERATOR_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/generator/Parser*.cpp")

foreach(test_file ${GENERATOR_TEST_FILES})
    # 파일명에서 확장자 제거하여 테스트 이름 생성
    get_filename_component(test_name ${test_file} NAME_WE)

    # 각 테스트 파일에 대한 실행파일 생성
    add_executable(${test_name}
        ${test_file}
        ${GENERATOR_SOURCES}
    )

    # Include 디렉토리 설정
    target_include_directories(${test_name} PRIVATE ${COMMON_INCLUDE_DIRS})

    # 링크 라이브러리 설정
    target_link_libraries(${test_name} ${COMMON_LINK_LIBS})

    # CTest에 테스트 추가 (라벨링 포함)
    add_test(NAME ${test_name} COMMAND ${test_name})

    # 테스트 라벨 설정
    set_tests_properties(${test_name} PROPERTIES
        LABELS "parser;generator;unit"
        TIMEOUT 30
    )

    # 특정 테스트별 추가 라벨
    if(${test_name} MATCHES ".*Basic.*")
        set_property(TEST ${test_name} APPEND PROPERTY LABELS "basic")
    endif()
    if(${test_name} MATCHES ".*Integration.*|.*Communication.*")
        set_property(TEST ${test_name} APPEND PROPERTY LABELS "integration")
    endif()
    if(${test_name} MATCHES ".*Error.*")
        set_property(TEST ${test_name} APPEND PROPERTY LABELS "error_handling")
    endif()
    if(${test_name} MATCHES ".*History.*")
        set_property(TEST ${test_name} APPEND PROPERTY LABELS "scxml_features")
    endif()
endforeach()

# 6. Runtime 테스트들 (별도 서브디렉토리)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/CMakeLists.txt")
    add_subdirectory(runtime)
endif()
