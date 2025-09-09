cmake_minimum_required(VERSION 3.16)
project(ReactiveStateMachine VERSION 1.0.0 LANGUAGES CXX)

# C++17 표준 사용 (모던 CMake 방식으로 타겟에 직접 설정)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 빌드 타입 기본값 설정
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# 컴파일러별 최적화 플래그
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        add_compile_options(/O2 /DNDEBUG)
    else()
        add_compile_options(-O3 -DNDEBUG -march=native)
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        add_compile_options(/Od /Zi)
    else()
        add_compile_options(-O0 -g3 -fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()

# 설치 경로를 위한 GNUInstallDirs 포함
include(GNUInstallDirs)

# Export compile commands for clangd/LSP
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ccache 설정 (빌드 시간 단축)
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    message(STATUS "ccache found: ${CCACHE_FOUND}")
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_FOUND})
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_FOUND})
else()
    message(STATUS "ccache not found")
endif()

# 위치 독립적 코드 생성 (공유 라이브러리를 위해)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 컴파일러 특징 검출 및 경고 설정
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
    # UTF-8 소스 코드 처리
    add_compile_options(/utf-8)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror -Wno-maybe-uninitialized)
    # GCC/Clang 특정 최적화
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wformat=2 -Wno-conversion -Wsign-conversion)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Weverything -Wno-c++98-compat -Wno-padded)
    endif()
endif()

# 종속성 관리 현대화
find_package(PkgConfig REQUIRED)

# libxml++5.0 찾기 (CMake 3.16+ 모던 방식)
pkg_check_modules(LIBXMLPP REQUIRED IMPORTED_TARGET "libxml++-5.0 >= 5.0.0")

# Boost 라이브러리 찾기 (모던 CMake 타겟 방식)
find_package(Boost 1.65 REQUIRED)

# 선택적 종속성들
find_package(Threads REQUIRED)

# 개발 도구 (선택적)
option(BUILD_WITH_COVERAGE "Enable code coverage" OFF)
if(BUILD_WITH_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    endif()
endif()

# 제너레이터 디렉토리 추가
add_subdirectory(generator)

# 테스트 빌드 옵션 (기본값을 ON으로 변경)
option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    enable_testing()
endif()

# 테스트 서브디렉토리 추가
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Export 파일 설치
install(EXPORT ReactiveStateMachineTargets
    FILE ReactiveStateMachineTargets.cmake
    NAMESPACE ReactiveStateMachine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ReactiveStateMachine
)

install(FILES
    examples/StateMachineInterface.h
    examples/StateMachineImpl.h
    examples/Context.h
    examples/MyStateMachine.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# 모던 CMake 패키지 구성
include(CMakePackageConfigHelpers)

# 패키지 버전 파일 생성
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ReactiveStateMachineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# 패키지 설정 파일 생성
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReactiveStateMachineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ReactiveStateMachineConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ReactiveStateMachine
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)

# 패키지 설치
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ReactiveStateMachineConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ReactiveStateMachineConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ReactiveStateMachine
)
