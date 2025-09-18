cmake_minimum_required(VERSION 3.14)
project(ReactiveStateMachine VERSION 1.0.0 LANGUAGES C CXX)

# =============================================================================
# 빌드 디렉토리 검증
# =============================================================================

# 소스 디렉토리 내 빌드 방지
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR 
        "
❌ ERROR: In-source builds are not allowed!
"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
"
        "Please create a separate build directory:
"
        "  mkdir build
"
        "  cd build
"
        "  cmake ..
"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
")
endif()

# build 디렉토리 이름 검증
get_filename_component(BUILD_DIR_NAME "${CMAKE_BINARY_DIR}" NAME)
if(NOT BUILD_DIR_NAME MATCHES "^(build|Build|BUILD|Debug|Release|_build)$")
    message(FATAL_ERROR 
        "
❌ ERROR: Please use standard build directory name!
"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
"
        "Current directory: '${BUILD_DIR_NAME}'
"
        "Allowed names: build, Build, BUILD, Debug, Release, _build
"
        "
"
        "Correct usage:
"
        "  mkdir build
"
        "  cd build
"
        "  cmake ..
"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
")
endif()

message(STATUS "✅ Building in correct directory: ${BUILD_DIR_NAME}")

# C++20 표준 사용
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 설치 경로를 위한 GNUInstallDirs 포함
include(GNUInstallDirs)

# 위치 독립적 코드 생성 (공유 라이브러리를 위해)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 컴파일 옵션 설정
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# libxml++5.0 찾기
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBXMLPP REQUIRED "libxml++-5.0 >= 5.0.0")

# Boost 라이브러리 찾기
find_package(Boost 1.65 REQUIRED)

# 📍 Modular Architecture - Add subdirectories
add_subdirectory(rsm)            # Unified RSM library with integrated parsing
add_subdirectory(tools/codegen)  # Code generation tool

# 테스트 빌드 옵션 (기본적으로 활성화)
option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 설치 설정 (GNUInstallDirs 변수 사용)
install(TARGETS rsm_unified scxml-codegen
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY
        rsm/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# 패키지 구성
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/StateMachineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# StateMachineConfig.cmake 파일 생성
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/StateMachineConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/StateMachineConfig.cmake
    @ONLY
)

# 패키지 설치
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/StateMachineConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/StateMachineConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/StateMachine
)
