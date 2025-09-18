# SCXML Runtime Library - JavaScript Engine

# QuickJS paths
set(QUICKJS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/quickjs")
set(QUICKJS_INCLUDE_DIR "${QUICKJS_DIR}")
set(QUICKJS_LIB "${QUICKJS_DIR}/libquickjs.a")

# Check if QuickJS library exists
if(NOT EXISTS "${QUICKJS_LIB}")
    message(STATUS "QuickJS library not found at ${QUICKJS_LIB}")
    message(STATUS "Building QuickJS...")

    # Build QuickJS
    execute_process(
        COMMAND make
        WORKING_DIRECTORY "${QUICKJS_DIR}"
        RESULT_VARIABLE QUICKJS_BUILD_RESULT
    )

    if(NOT QUICKJS_BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build QuickJS")
    endif()
endif()

# SCXML Engine Shared Library
add_library(scxml_engine SHARED
    JSEngine.cpp
    JSEngine_impl.cpp
    SCXMLEngineImpl.cpp
)

# Link with QuickJS and required system libraries
target_link_libraries(scxml_engine
    PRIVATE
        ${QUICKJS_LIB}
        pthread  # for std::thread
        dl       # for dynamic loading (required by QuickJS)
        m        # math library (required by QuickJS)
)

# Set C++17 standard
target_compile_features(scxml_engine
    PUBLIC cxx_std_17
)

# JSEngine Example Application
add_executable(jsengine_example
    JSEngine_example.cpp
)

target_link_libraries(jsengine_example
    PRIVATE
        scxml_engine
)

# Shared library properties
set_target_properties(scxml_engine PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    PUBLIC_HEADER "../include/SCXMLEngine.h;../include/SCXMLTypes.h"
)

# Add compile flags for QuickJS compatibility
target_compile_definitions(scxml_engine
    PRIVATE
        _GNU_SOURCE        # Required for QuickJS on Linux
        SCXML_ENGINE_EXPORTS  # For symbol visibility
)

# Update include directories to expose public API
target_include_directories(scxml_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${QUICKJS_INCLUDE_DIR}
)