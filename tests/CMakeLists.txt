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
    ${CMAKE_SOURCE_DIR}/generator/src/core/HistoryNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/ParallelNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/FinalNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/DoneDataNode.cpp
    ${CMAKE_SOURCE_DIR}/generator/src/core/ScriptNode.cpp

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
    ${CMAKE_SOURCE_DIR}/generator/src/core/ParamNode.cpp

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

# Add QuickJS engine source to tests if available
if(ENABLE_QUICKJS)
    set(QUICKJS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/quickjs")
    set(QUICKJS_LIBRARY "${QUICKJS_SOURCE_DIR}/libquickjs.a")
    
    if(EXISTS "${QUICKJS_SOURCE_DIR}/quickjs.h" AND EXISTS "${QUICKJS_LIBRARY}")
        list(APPEND GENERATOR_SOURCES "${CMAKE_SOURCE_DIR}/generator/src/runtime/QuickJSEngine.cpp")
        message(STATUS "QuickJSEngine.cpp added to test sources")
    endif()
endif()

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

# Parser 테스트들 빌드
file(GLOB PARSER_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/parser/*ParserTest.cpp")

foreach(test_file ${PARSER_TEST_FILES})
    # 파일명에서 확장자 제거하여 테스트 이름 생성
    get_filename_component(test_name ${test_file} NAME_WE)

    # 각 테스트 파일에 대한 실행파일 생성
    add_executable(${test_name}
        ${test_file}
        ${CMAKE_CURRENT_SOURCE_DIR}/parser/ParserTestCommon.h
        ${GENERATOR_SOURCES}
    )

    # Include 디렉토리 설정
    target_include_directories(${test_name} PRIVATE 
        ${COMMON_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/parser
    )

    # 링크 라이브러리 설정
    target_link_libraries(${test_name} ${COMMON_LINK_LIBS})
    
    # Add QuickJS support for parser tests if available
    if(ENABLE_QUICKJS)
        set(QUICKJS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/quickjs")
        set(QUICKJS_LIBRARY "${QUICKJS_SOURCE_DIR}/libquickjs.a")
        
        if(EXISTS "${QUICKJS_SOURCE_DIR}/quickjs.h" AND EXISTS "${QUICKJS_LIBRARY}")
            target_compile_definitions(${test_name} PRIVATE HAS_QUICKJS=1 ENABLE_QUICKJS=1)
            target_include_directories(${test_name} SYSTEM PRIVATE ${QUICKJS_SOURCE_DIR})
            target_link_libraries(${test_name} ${QUICKJS_LIBRARY} m pthread dl)
            target_compile_options(${test_name} PRIVATE -Wno-error=unused-parameter -Wno-error=pedantic -Wno-error=sign-conversion)
            message(STATUS "QuickJS support enabled for parser test: ${test_name}")
        endif()
    endif()

    # CTest에 테스트 추가 (라벨링 포함)
    add_test(NAME ${test_name} COMMAND ${test_name})

    # 테스트 라벨 설정
    set_tests_properties(${test_name} PROPERTIES
        LABELS "parser;unit"
        TIMEOUT 30
    )

    # 특정 테스트별 추가 라벨
    if(${test_name} MATCHES ".*Document.*")
        set_property(TEST ${test_name} APPEND PROPERTY LABELS "document")
    endif()
    if(${test_name} MATCHES ".*StateNode.*")
        set_property(TEST ${test_name} APPEND PROPERTY LABELS "state")
    endif()
    if(${test_name} MATCHES ".*Transition.*")
        set_property(TEST ${test_name} APPEND PROPERTY LABELS "transition")
    endif()
    if(${test_name} MATCHES ".*Error.*")
        set_property(TEST ${test_name} APPEND PROPERTY LABELS "error_handling")
    endif()
    if(${test_name} MATCHES ".*History.*")
        set_property(TEST ${test_name} APPEND PROPERTY LABELS "history")
    endif()
    if(${test_name} MATCHES ".*Communication.*")
        set_property(TEST ${test_name} APPEND PROPERTY LABELS "communication")
    endif()
    if(${test_name} MATCHES ".*Invoke.*")
        set_property(TEST ${test_name} APPEND PROPERTY LABELS "invoke")
    endif()
endforeach()

# Core 모듈 테스트들
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core")
    file(GLOB_RECURSE CORE_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/core/*Test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/core/*/*Test.cpp")
    
    foreach(test_file ${CORE_TEST_FILES})
        get_filename_component(test_name ${test_file} NAME_WE)
        
        add_executable(${test_name}
            ${test_file}
            ${GENERATOR_SOURCES}
        )
        
        target_include_directories(${test_name} PRIVATE 
            ${COMMON_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/core
        )
        
        target_link_libraries(${test_name} ${COMMON_LINK_LIBS})
        
        # Add QuickJS support for core tests if available
        if(ENABLE_QUICKJS)
            set(QUICKJS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/quickjs")
            set(QUICKJS_LIBRARY "${QUICKJS_SOURCE_DIR}/libquickjs.a")
            
            if(EXISTS "${QUICKJS_SOURCE_DIR}/quickjs.h" AND EXISTS "${QUICKJS_LIBRARY}")
                target_compile_definitions(${test_name} PRIVATE HAS_QUICKJS=1 ENABLE_QUICKJS=1)
                target_include_directories(${test_name} SYSTEM PRIVATE ${QUICKJS_SOURCE_DIR})
                target_link_libraries(${test_name} ${QUICKJS_LIBRARY} m pthread dl)
                target_compile_options(${test_name} PRIVATE -Wno-error=unused-parameter -Wno-error=pedantic -Wno-error=sign-conversion)
                message(STATUS "QuickJS support enabled for core test: ${test_name}")
            endif()
        endif()
        
        add_test(NAME ${test_name} COMMAND ${test_name})
        
        set_tests_properties(${test_name} PROPERTIES
            LABELS "core;unit"
            TIMEOUT 30
        )
    endforeach()
endif()

# Runtime 모듈 테스트들
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime")
    file(GLOB_RECURSE RUNTIME_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*Test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*/*Test.cpp")
    
    foreach(test_file ${RUNTIME_TEST_FILES})
        get_filename_component(test_name ${test_file} NAME_WE)
        
        add_executable(${test_name}
            ${test_file}
            ${GENERATOR_SOURCES}
        )
        
        target_include_directories(${test_name} PRIVATE 
            ${COMMON_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/runtime
        )
        
        target_link_libraries(${test_name} ${COMMON_LINK_LIBS})
        
        # Add QuickJS support for runtime tests if available
        if(ENABLE_QUICKJS)
            set(QUICKJS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/quickjs")
            set(QUICKJS_LIBRARY "${QUICKJS_SOURCE_DIR}/libquickjs.a")
            
            if(EXISTS "${QUICKJS_SOURCE_DIR}/quickjs.h" AND EXISTS "${QUICKJS_LIBRARY}")
                target_compile_definitions(${test_name} PRIVATE HAS_QUICKJS=1 ENABLE_QUICKJS=1)
                target_include_directories(${test_name} SYSTEM PRIVATE ${QUICKJS_SOURCE_DIR})
                target_link_libraries(${test_name} ${QUICKJS_LIBRARY} m pthread dl)
                
                # Disable warnings for QuickJS header (C library in C++ context)
                target_compile_options(${test_name} PRIVATE -Wno-error=unused-parameter -Wno-error=pedantic -Wno-error=sign-conversion)
                
                message(STATUS "QuickJS support enabled for test: ${test_name}")
            endif()
        endif()
        
        add_test(NAME ${test_name} COMMAND ${test_name})
        
        set_tests_properties(${test_name} PROPERTIES
            LABELS "runtime;unit"
            TIMEOUT 60
        )
    endforeach()
endif()

# Integration 테스트들
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration")
    file(GLOB_RECURSE INTEGRATION_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/integration/*Test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/integration/*/*Test.cpp")
    
    foreach(test_file ${INTEGRATION_TEST_FILES})
        get_filename_component(test_name ${test_file} NAME_WE)
        
        add_executable(${test_name}
            ${test_file}
            ${GENERATOR_SOURCES}
        )
        
        target_include_directories(${test_name} PRIVATE 
            ${COMMON_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/integration
        )
        
        target_link_libraries(${test_name} ${COMMON_LINK_LIBS})
        
        # Add QuickJS support for integration tests if available
        if(ENABLE_QUICKJS)
            set(QUICKJS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/quickjs")
            set(QUICKJS_LIBRARY "${QUICKJS_SOURCE_DIR}/libquickjs.a")
            
            if(EXISTS "${QUICKJS_SOURCE_DIR}/quickjs.h" AND EXISTS "${QUICKJS_LIBRARY}")
                target_compile_definitions(${test_name} PRIVATE HAS_QUICKJS=1 ENABLE_QUICKJS=1)
                target_include_directories(${test_name} SYSTEM PRIVATE ${QUICKJS_SOURCE_DIR})
                target_link_libraries(${test_name} ${QUICKJS_LIBRARY} m pthread dl)
                target_compile_options(${test_name} PRIVATE -Wno-error=unused-parameter -Wno-error=pedantic -Wno-error=sign-conversion)
                message(STATUS "QuickJS support enabled for integration test: ${test_name}")
            endif()
        endif()
        
        add_test(NAME ${test_name} COMMAND ${test_name})
        
        set_tests_properties(${test_name} PROPERTIES
            LABELS "integration;system"
            TIMEOUT 120
        )
    endforeach()
endif()

# Examples 테스트들
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    file(GLOB_RECURSE EXAMPLE_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/examples/*Test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/examples/*/*Test.cpp")
    
    foreach(test_file ${EXAMPLE_TEST_FILES})
        get_filename_component(test_name ${test_file} NAME_WE)
        
        add_executable(${test_name}
            ${test_file}
            ${GENERATOR_SOURCES}
        )
        
        target_include_directories(${test_name} PRIVATE 
            ${COMMON_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/examples
        )
        
        target_link_libraries(${test_name} ${COMMON_LINK_LIBS})
        
        # Add QuickJS support for examples tests if available
        if(ENABLE_QUICKJS)
            set(QUICKJS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/quickjs")
            set(QUICKJS_LIBRARY "${QUICKJS_SOURCE_DIR}/libquickjs.a")
            
            if(EXISTS "${QUICKJS_SOURCE_DIR}/quickjs.h" AND EXISTS "${QUICKJS_LIBRARY}")
                target_compile_definitions(${test_name} PRIVATE HAS_QUICKJS=1 ENABLE_QUICKJS=1)
                target_include_directories(${test_name} SYSTEM PRIVATE ${QUICKJS_SOURCE_DIR})
                target_link_libraries(${test_name} ${QUICKJS_LIBRARY} m pthread dl)
                target_compile_options(${test_name} PRIVATE -Wno-error=unused-parameter -Wno-error=pedantic -Wno-error=sign-conversion)
                message(STATUS "QuickJS support enabled for examples test: ${test_name}")
            endif()
        endif()
        
        add_test(NAME ${test_name} COMMAND ${test_name})
        
        set_tests_properties(${test_name} PROPERTIES
            LABELS "examples;functional"
            TIMEOUT 90
        )
    endforeach()
endif()

# Performance 테스트들
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/performance")
    file(GLOB_RECURSE PERFORMANCE_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/performance/*Test.cpp")
    
    foreach(test_file ${PERFORMANCE_TEST_FILES})
        get_filename_component(test_name ${test_file} NAME_WE)
        
        add_executable(${test_name}
            ${test_file}
            ${GENERATOR_SOURCES}
        )
        
        target_include_directories(${test_name} PRIVATE 
            ${COMMON_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/performance
        )
        
        target_link_libraries(${test_name} ${COMMON_LINK_LIBS})
        
        # Add QuickJS support for performance tests if available
        if(ENABLE_QUICKJS)
            set(QUICKJS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/quickjs")
            set(QUICKJS_LIBRARY "${QUICKJS_SOURCE_DIR}/libquickjs.a")
            
            if(EXISTS "${QUICKJS_SOURCE_DIR}/quickjs.h" AND EXISTS "${QUICKJS_LIBRARY}")
                target_compile_definitions(${test_name} PRIVATE HAS_QUICKJS=1 ENABLE_QUICKJS=1)
                target_include_directories(${test_name} SYSTEM PRIVATE ${QUICKJS_SOURCE_DIR})
                target_link_libraries(${test_name} ${QUICKJS_LIBRARY} m pthread dl)
                target_compile_options(${test_name} PRIVATE -Wno-error=unused-parameter -Wno-error=pedantic -Wno-error=sign-conversion)
                message(STATUS "QuickJS support enabled for performance test: ${test_name}")
            endif()
        endif()
        
        add_test(NAME ${test_name} COMMAND ${test_name})
        
        set_tests_properties(${test_name} PROPERTIES
            LABELS "performance;benchmark"
            TIMEOUT 300
        )
    endforeach()
endif()

# Compliance 테스트들
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/compliance")
    file(GLOB_RECURSE COMPLIANCE_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/compliance/*Test.cpp")
    
    foreach(test_file ${COMPLIANCE_TEST_FILES})
        get_filename_component(test_name ${test_file} NAME_WE)
        
        add_executable(${test_name}
            ${test_file}
            ${GENERATOR_SOURCES}
        )
        
        target_include_directories(${test_name} PRIVATE 
            ${COMMON_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/compliance
        )
        
        target_link_libraries(${test_name} ${COMMON_LINK_LIBS})
        
        # Add QuickJS support for compliance tests if available
        if(ENABLE_QUICKJS)
            set(QUICKJS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/quickjs")
            set(QUICKJS_LIBRARY "${QUICKJS_SOURCE_DIR}/libquickjs.a")
            
            if(EXISTS "${QUICKJS_SOURCE_DIR}/quickjs.h" AND EXISTS "${QUICKJS_LIBRARY}")
                target_compile_definitions(${test_name} PRIVATE HAS_QUICKJS=1 ENABLE_QUICKJS=1)
                target_include_directories(${test_name} SYSTEM PRIVATE ${QUICKJS_SOURCE_DIR})
                target_link_libraries(${test_name} ${QUICKJS_LIBRARY} m pthread dl)
                target_compile_options(${test_name} PRIVATE -Wno-error=unused-parameter -Wno-error=pedantic -Wno-error=sign-conversion)
                message(STATUS "QuickJS support enabled for compliance test: ${test_name}")
            endif()
        endif()
        
        add_test(NAME ${test_name} COMMAND ${test_name})
        
        set_tests_properties(${test_name} PROPERTIES
            LABELS "compliance;w3c"
            TIMEOUT 180
        )
    endforeach()
endif()

# W3C Compliance 테스트들
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/w3c_compliance")
    file(GLOB_RECURSE W3C_COMPLIANCE_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/w3c_compliance/*Test.cpp")
    
    foreach(test_file ${W3C_COMPLIANCE_TEST_FILES})
        get_filename_component(test_name ${test_file} NAME_WE)
        
        add_executable(${test_name}
            ${test_file}
            ${GENERATOR_SOURCES}
        )
        
        target_include_directories(${test_name} PRIVATE 
            ${COMMON_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/w3c_compliance
        )
        
        target_link_libraries(${test_name} ${COMMON_LINK_LIBS})
        
        # Add QuickJS support for w3c_compliance tests if available
        if(ENABLE_QUICKJS)
            set(QUICKJS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/quickjs")
            set(QUICKJS_LIBRARY "${QUICKJS_SOURCE_DIR}/libquickjs.a")
            
            if(EXISTS "${QUICKJS_SOURCE_DIR}/quickjs.h" AND EXISTS "${QUICKJS_LIBRARY}")
                target_compile_definitions(${test_name} PRIVATE HAS_QUICKJS=1 ENABLE_QUICKJS=1)
                target_include_directories(${test_name} SYSTEM PRIVATE ${QUICKJS_SOURCE_DIR})
                target_link_libraries(${test_name} ${QUICKJS_LIBRARY} m pthread dl)
                target_compile_options(${test_name} PRIVATE -Wno-error=unused-parameter -Wno-error=pedantic -Wno-error=sign-conversion)
                message(STATUS "QuickJS support enabled for w3c_compliance test: ${test_name}")
            endif()
        endif()
        
        add_test(NAME ${test_name} COMMAND ${test_name})
        
        set_tests_properties(${test_name} PROPERTIES
            LABELS "w3c_compliance;scxml_specification;communication"
            TIMEOUT 120
        )
        
        # 특정 W3C 테스트별 추가 라벨링
        if(${test_name} MATCHES ".*SendReceive.*")
            set_property(TEST ${test_name} APPEND PROPERTY LABELS "send_receive;communication")
        endif()
        if(${test_name} MATCHES ".*Event.*")
            set_property(TEST ${test_name} APPEND PROPERTY LABELS "event_system;queue_management")
        endif()
        if(${test_name} MATCHES ".*Comprehensive.*")
            set_property(TEST ${test_name} APPEND PROPERTY LABELS "integration;comprehensive")
        endif()
    endforeach()
endif()

# Runtime 테스트들 (별도 서브디렉토리)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/CMakeLists.txt")
    add_subdirectory(runtime)
endif()
