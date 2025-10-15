# RSM Unified Module
# Complete RSM implementation with domain models, runtime engine, and scripting

# Build libxml2 from submodule using ExternalProject + CMake
include(ExternalProject)

set(LIBXML2_INSTALL_DIR ${CMAKE_BINARY_DIR}/libxml2_install)
set(LIBXML2_BUILD_DIR ${CMAKE_BINARY_DIR}/libxml2_build)

ExternalProject_Add(libxml2_external
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/libxml2
    BUILD_IN_SOURCE 0
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${LIBXML2_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DBUILD_SHARED_LIBS=ON
        -DLIBXML2_WITH_PYTHON=OFF
        -DLIBXML2_WITH_PROGRAMS=OFF
        -DLIBXML2_WITH_TESTS=OFF
        -DLIBXML2_WITH_LZMA=OFF
        -DLIBXML2_WITH_ZLIB=ON
    BUILD_BYPRODUCTS
        ${LIBXML2_INSTALL_DIR}/lib/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${LIBXML2_INSTALL_DIR}/include/libxml2/libxml/tree.h
)

# Set libxml2 variables for linking
set(LIBXML2_INCLUDE_DIRS ${LIBXML2_INSTALL_DIR}/include/libxml2)
set(LIBXML2_LIBRARIES ${LIBXML2_INSTALL_DIR}/lib/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX})
set(LIBXML2_PREFIX ${LIBXML2_INSTALL_DIR})

# Create include directories at configure time
file(MAKE_DIRECTORY ${LIBXML2_INCLUDE_DIRS})

message(STATUS "LIBXML2_INSTALL_DIR: ${LIBXML2_INSTALL_DIR}")
message(STATUS "LIBXML2_INCLUDE_DIRS: ${LIBXML2_INCLUDE_DIRS}")

# Find pkg-config for libxml++ (which may need it)
find_package(PkgConfig REQUIRED)

# Build libxml++ from submodule using ExternalProject + Meson

set(LIBXMLXX_INSTALL_DIR ${CMAKE_BINARY_DIR}/libxmlxx_install)
set(LIBXMLXX_BUILD_DIR ${CMAKE_BINARY_DIR}/libxmlxx_build)

ExternalProject_Add(libxmlxx_external
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/libxml++
    BUILD_IN_SOURCE 0
    DEPENDS libxml2_external
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
        PKG_CONFIG_PATH=${LIBXML2_INSTALL_DIR}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}
        meson setup ${LIBXMLXX_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/libxml++
        --prefix=${LIBXMLXX_INSTALL_DIR}
        --libdir=lib
        --default-library=shared
        -Dbuild-examples=false
        -Dbuild-tests=false
        -Dbuild-documentation=false
        -Dmaintainer-mode=false
    BUILD_COMMAND meson compile -C ${LIBXMLXX_BUILD_DIR}
    INSTALL_COMMAND meson install -C ${LIBXMLXX_BUILD_DIR}
    BUILD_BYPRODUCTS
        ${LIBXMLXX_INSTALL_DIR}/lib/libxml++-5.0${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${LIBXMLXX_INSTALL_DIR}/include/libxml++-5.0/libxml++/libxml++.h
)

# Set libxml++ variables for linking
set(LIBXMLXX_INCLUDE_DIRS
    ${LIBXMLXX_INSTALL_DIR}/include/libxml++-5.0
    ${LIBXMLXX_INSTALL_DIR}/lib/libxml++-5.0/include
)
set(LIBXMLXX_LIBRARIES ${LIBXMLXX_INSTALL_DIR}/lib/libxml++-5.0${CMAKE_SHARED_LIBRARY_SUFFIX})

# Create include directories at configure time so CMake doesn't drop them
file(MAKE_DIRECTORY ${LIBXMLXX_INSTALL_DIR}/include/libxml++-5.0)
file(MAKE_DIRECTORY ${LIBXMLXX_INSTALL_DIR}/lib/libxml++-5.0/include)

message(STATUS "LIBXMLXX_INCLUDE_DIRS: ${LIBXMLXX_INCLUDE_DIRS}")
message(STATUS "LIBXMLXX_INSTALL_DIR: ${LIBXMLXX_INSTALL_DIR}")

# Add QuickJS subdirectory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/quickjs ${CMAKE_CURRENT_BINARY_DIR}/quickjs)

# Add spdlog subdirectory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/spdlog ${CMAKE_CURRENT_BINARY_DIR}/spdlog)

# Add cpp-httplib subdirectory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cpp-httplib ${CMAKE_CURRENT_BINARY_DIR}/cpp-httplib)

# Add nlohmann/json (header-only library)
# Modern C++ JSON library with full C++20 support
set(NLOHMANN_JSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann_json/include)

add_library(rsm_unified
    # Model implementation
    src/model/SCXMLModel.cpp
    src/model/StateNode.cpp
    src/model/TransitionNode.cpp

    src/model/GuardNode.cpp
    src/model/InvokeNode.cpp
    src/model/StateHierarchy.cpp
    src/model/DataModelItem.cpp
    src/model/SCXMLContext.cpp

    # Common utilities
    src/common/EventDataHelper.cpp
    src/common/GuardUtils.cpp
    src/common/Logger.cpp
    src/common/NodeFactory.cpp
    src/common/TypeRegistry.cpp
    src/common/UniqueIdGenerator.cpp
    src/common/JsonUtils.cpp
    src/common/HttpResponseUtils.cpp

    # Runtime implementation
    src/runtime/SCXMLEngineImpl.cpp
    src/runtime/StateMachine.cpp
    src/runtime/StateMachineContext.cpp
    src/runtime/StateMachineFactory.cpp
    src/runtime/ActionExecutorImpl.cpp
    src/runtime/StateMachineEventRaiser.cpp
    src/runtime/ExecutionContextImpl.cpp
    src/runtime/StateHierarchyManager.cpp
    src/runtime/EventRaiserImpl.cpp
    src/runtime/InvokeExecutor.cpp
    src/runtime/SessionManagerImpl.cpp
    src/runtime/DataContentHelpers.cpp
    
    # History state management (SOLID architecture)
    src/runtime/HistoryManager.cpp
    src/runtime/HistoryStateAutoRegistrar.cpp
    src/runtime/ShallowHistoryFilter.cpp
    src/runtime/DeepHistoryFilter.cpp
    src/runtime/HistoryValidator.cpp

    # Actions implementation
    src/actions/BaseAction.cpp
    src/actions/ScriptAction.cpp
    src/actions/AssignAction.cpp
    src/actions/LogAction.cpp
    src/actions/RaiseAction.cpp
    src/actions/IfAction.cpp
    src/actions/SendAction.cpp
    src/actions/CancelAction.cpp
    src/actions/ForeachAction.cpp

    # Events implementation
    src/events/InternalEventTarget.cpp
    src/events/ParentEventTarget.cpp
    src/events/HttpEventTarget.cpp
    src/events/HttpEventReceiver.cpp
    src/events/HttpEventBridge.cpp
    src/events/HttpEventCoordinator.cpp
    src/events/InvokeEventTarget.cpp
    src/events/EventSchedulerImpl.cpp
    src/events/EventDispatcherImpl.cpp
    src/events/EventTargetFactoryImpl.cpp
    src/events/EventRaiserRegistry.cpp
    src/events/EventRaiserService.cpp


    # Concurrent states implementation
    src/states/ConcurrentStateNode.cpp
    src/states/ConcurrentRegion.cpp
    src/states/ParallelRegionOrchestrator.cpp
    src/states/ConcurrentEventTypes.cpp
    src/states/ConcurrentEventBroadcaster.cpp
    src/states/ConcurrentCompletionMonitor.cpp
    src/states/ExternalTransitionHandler.cpp
    src/states/StateExitExecutor.cpp

    # Scripting implementation
    src/scripting/JSEngine.cpp
    src/scripting/JSEngineImpl.cpp
    src/scripting/JSExecutionEngineImpl.cpp
    src/scripting/XMLDOMWrapper.cpp
    src/scripting/DOMBinding.cpp

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
    
    # High-level API
    src/ReadySCXMLEngineImpl.cpp
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
        include/actions
        include/events
        include/states
        ${NLOHMANN_JSON_INCLUDE_DIR}
        ${LIBXMLXX_INSTALL_DIR}/include/libxml++-5.0
        ${LIBXMLXX_INSTALL_DIR}/lib/libxml++-5.0/include
        ${LIBXML2_INCLUDE_DIRS}
    PRIVATE
        ${Boost_INCLUDE_DIRS}
)

# Make rsm_unified depend on the external libxml2 and libxml++ builds
add_dependencies(rsm_unified libxml2_external libxmlxx_external)

# RSM 코드베이스에서 std::cout 사용 금지 설정  
# printf 계열은 시스템 헤더 충돌로 인해 비활성화됨

# DisableStdOut.h를 전역적으로 강제 포함
target_compile_options(rsm_unified 
    PRIVATE
        -include common/DisableStdOut.h
)

target_link_libraries(rsm_unified
    PUBLIC
        qjs                          # QuickJS library
        spdlog::spdlog               # spdlog library
        httplib::httplib             # cpp-httplib library
        # nlohmann/json is header-only, no linking required
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

