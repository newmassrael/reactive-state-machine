cmake_minimum_required(VERSION 3.14)

# Set policy for Boost before project()
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(ReactiveStateMachine VERSION 1.0.0 LANGUAGES C CXX)

# =============================================================================
# Build Directory Validation
# =============================================================================

# Prevent in-source builds
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

# Validate build directory name
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

message(STATUS "Building in correct directory: ${BUILD_DIR_NAME}")

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sanitizer options for Debug builds
# Docker TSAN environment auto-detection
if(DEFINED ENV{IN_DOCKER_TSAN})
    set(ENABLE_TSAN ON CACHE BOOL "Auto-enabled in Docker TSAN environment" FORCE)
    message(STATUS "Docker TSAN environment detected - TSAN auto-enabled")
endif()

option(ENABLE_TSAN "Enable ThreadSanitizer instead of AddressSanitizer (Debug only)" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(ENABLE_TSAN)
            message(STATUS "Enabling ThreadSanitizer for Debug build")
            message(STATUS "  Note: Use docker_tsan/ environment for TSAN with system library workarounds")

            add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
            add_link_options(-fsanitize=thread)
        else()
            message(STATUS "Enabling AddressSanitizer for Debug build")
            add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
            add_link_options(-fsanitize=address)
        endif()
    endif()
endif()

# Include GNUInstallDirs for installation paths
include(GNUInstallDirs)

# Include RSM code generation utilities
include(cmake/RSMCodegen.cmake)

# Position independent code generation (for shared libraries)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Disable cpp-httplib zstd support
# Reason: Current project uses only small HTTP messages, compression unnecessary
# Avoids cpp-httplib CMake zstd integration issues
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "Disable zstd compression support" FORCE)

# Compilation options
if(MSVC)
    add_compile_options(/W4 /WX)  # /WX: treat warnings as errors
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)  # -Werror: treat warnings as errors
endif()

# libxml++5.0 is handled in rsm/CMakeLists.txt
find_package(PkgConfig REQUIRED)

# Find Boost library
find_package(Boost 1.65 REQUIRED)

# Modular Architecture - Add subdirectories
add_subdirectory(rsm)                 # Unified RSM library with integrated parsing
add_subdirectory(tools/codegen)       # Code generation tool
add_subdirectory(tools/txml_converter)  # TXML to SCXML converter tool

# Test build option (enabled by default)
option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Example build option (disabled by default)
option(BUILD_EXAMPLES "Build the examples" OFF)
if(BUILD_EXAMPLES)
    add_subdirectory(examples/cmake_function)
endif()

# Installation configuration (using GNUInstallDirs variables)
install(TARGETS rsm_unified scxml-codegen txml-converter
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

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/StateMachineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Generate StateMachineConfig.cmake file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/StateMachineConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/StateMachineConfig.cmake
    @ONLY
)

# Package installation
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/StateMachineConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/StateMachineConfigVersion.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RSMCodegen.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/StateMachine
)
