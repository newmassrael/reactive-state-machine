# RSM Unified Module
# Complete RSM implementation with domain models, runtime engine, and scripting

# Find required system dependencies  
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)

# Build libxml++ from submodule using ExternalProject + Meson
include(ExternalProject)

set(LIBXMLXX_INSTALL_DIR ${CMAKE_BINARY_DIR}/libxmlxx_install)
set(LIBXMLXX_BUILD_DIR ${CMAKE_BINARY_DIR}/libxmlxx_build)

ExternalProject_Add(libxmlxx_external
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/libxml++
    BUILD_IN_SOURCE 0
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
        PKG_CONFIG_PATH=${LIBXML2_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}
        meson setup ${LIBXMLXX_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/libxml++
        --prefix=${LIBXMLXX_INSTALL_DIR}
        --libdir=lib
        --default-library=shared
        -Dbuild-examples=false
        -Dbuild-tests=false
        -Dbuild-documentation=false
    BUILD_COMMAND meson compile -C ${LIBXMLXX_BUILD_DIR}
    INSTALL_COMMAND meson install -C ${LIBXMLXX_BUILD_DIR}
    BUILD_BYPRODUCTS 
        ${LIBXMLXX_INSTALL_DIR}/lib/libxml++-5.0${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${LIBXMLXX_INSTALL_DIR}/include/libxml++-5.0/libxml++/libxml++.h
)

# Set libxml++ variables for linking
set(LIBXMLXX_INCLUDE_DIRS 
    ${LIBXMLXX_INSTALL_DIR}/include/libxml++-5.0
    ${LIBXMLXX_INSTALL_DIR}/include/libxml++-5.0/libxml++config
)
set(LIBXMLXX_LIBRARIES ${LIBXMLXX_INSTALL_DIR}/lib/libxml++-5.0${CMAKE_SHARED_LIBRARY_SUFFIX})

# Add QuickJS subdirectory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/quickjs ${CMAKE_CURRENT_BINARY_DIR}/quickjs)

# Add spdlog subdirectory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/spdlog ${CMAKE_CURRENT_BINARY_DIR}/spdlog)

add_library(rsm_unified
    # Model implementation
    src/model/SCXMLModel.cpp
    src/model/StateNode.cpp
    src/model/TransitionNode.cpp
    src/model/ActionNode.cpp
    src/model/GuardNode.cpp
    src/model/InvokeNode.cpp
    src/model/StateHierarchy.cpp
    src/model/DataModelItem.cpp
    src/model/SCXMLContext.cpp

    # Common utilities
    src/common/GuardUtils.cpp
    src/common/Logger.cpp
    src/common/NodeFactory.cpp

    # Runtime implementation
    src/runtime/SCXMLEngineImpl.cpp
    src/runtime/StateMachine.cpp
    src/runtime/StateMachineFactory.cpp
    src/runtime/SCXMLModelExtractor.cpp

    # Scripting implementation
    src/scripting/JSEngine.cpp
    src/scripting/JSEngine_impl.cpp

    # Parsing implementation
    src/parsing/StateNodeParser.cpp
    src/parsing/TransitionParser.cpp
    src/parsing/SCXMLParser.cpp
    src/parsing/ActionParser.cpp
    src/parsing/GuardParser.cpp
    src/parsing/InvokeParser.cpp
    src/parsing/DataModelParser.cpp
    src/parsing/DoneDataParser.cpp
    src/parsing/ParsingCommon.cpp
    src/parsing/XIncludeProcessor.cpp
)

target_include_directories(rsm_unified
    PUBLIC
        include
        include/model
        include/runtime
        include/scripting
        include/factory
        include/common
        include/parsing
    PRIVATE
        ${LIBXMLPP_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
)

# Make rsm_unified depend on the external libxml++ build
add_dependencies(rsm_unified libxmlxx_external)

target_link_libraries(rsm_unified
    PUBLIC
        qjs                          # QuickJS library
        spdlog::spdlog               # spdlog library
    PRIVATE
        ${LIBXMLXX_LIBRARIES}
        ${LIBXML2_LIBRARIES}         # libxml2 dependency for libxml++
        pthread
        dl
        m
)

# Set C++20 standard
target_compile_features(rsm_unified
    PUBLIC cxx_std_20
)

