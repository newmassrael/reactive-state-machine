# SCXML 코드 생성기 라이브러리를 위한 CMakeLists.txt

# 소스 파일들을 변수로 정의 (가독성 및 유지보수성 향상)
set(SCXML_GENERATOR_SOURCES
    src/common/Logger.cpp
    src/core/ParallelNode.cpp
)

# 모던 CMake 생성기 라이브러리 정의
add_library(scxml_generator ${SCXML_GENERATOR_SOURCES})

# 네임스페이스 별명 추가
add_library(ReactiveStateMachine::Generator ALIAS scxml_generator)

# 모던 CMake 헤더 및 인터페이스 설정
target_include_directories(scxml_generator
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/scxml_generator>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include/model
        ${CMAKE_CURRENT_SOURCE_DIR}/include/impl
        ${CMAKE_CURRENT_SOURCE_DIR}/include/parsing
        ${CMAKE_CURRENT_SOURCE_DIR}/include/factory
)

# C++17 기능 명시적 설정
target_compile_features(scxml_generator PUBLIC cxx_std_17)

# QuickJS 지원 설정 (서브모듈 사용)
option(ENABLE_QUICKJS "Enable QuickJS JavaScript engine support" ON)
if(ENABLE_QUICKJS)
    # QuickJS 서브모듈 경로 확인
    set(QUICKJS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/quickjs")
    set(QUICKJS_LIBRARY "${QUICKJS_SOURCE_DIR}/libquickjs.a")

    if(EXISTS "${QUICKJS_SOURCE_DIR}/quickjs.h" AND EXISTS "${QUICKJS_LIBRARY}")
        message(STATUS "QuickJS submodule found, using pre-built library")

        # QuickJS 라이브러리 임포트
        add_library(quickjs_imported STATIC IMPORTED)
        set_target_properties(quickjs_imported PROPERTIES
            IMPORTED_LOCATION "${QUICKJS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${QUICKJS_SOURCE_DIR}"
        )

        # scxml_generator에 QuickJS 연결
        target_compile_definitions(scxml_generator PRIVATE HAS_QUICKJS=1 ENABLE_QUICKJS=1)
        target_include_directories(scxml_generator PRIVATE ${QUICKJS_SOURCE_DIR})
        target_link_libraries(scxml_generator PRIVATE quickjs_imported m pthread dl)

        message(STATUS "QuickJS support enabled (using pre-built library)")
    else()
        message(WARNING "QuickJS library not found, using JavaScriptEvaluatorAdapter fallback")
        message(STATUS "Expected library at: ${QUICKJS_LIBRARY}")
        target_compile_definitions(scxml_generator PRIVATE HAS_QUICKJS=0)
    endif()
else()
    target_compile_definitions(scxml_generator PRIVATE HAS_QUICKJS=0)
    message(STATUS "QuickJS support disabled, using JavaScriptEvaluatorAdapter fallback")
endif()

# 모던 타겟 링킹
target_link_libraries(scxml_generator
    PUBLIC
        PkgConfig::LIBXMLPP
    PRIVATE
        Threads::Threads
        cpp-httplib
)

# 라이브러리 속성 설정
set_target_properties(scxml_generator PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME Generator
)

# 모던 CMake 설치 설정
install(TARGETS scxml_generator
    EXPORT ReactiveStateMachineTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/scxml_generator
)

# 헤더 파일 설치 (더 구체적인 패턴 매칭)
install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/scxml_generator
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
    PATTERN "private" EXCLUDE
)
