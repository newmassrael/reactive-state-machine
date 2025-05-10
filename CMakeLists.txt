cmake_minimum_required(VERSION 3.14)
project(ReactiveStateMachine VERSION 1.0.0 LANGUAGES CXX)

# C++17 표준 사용
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 설치 경로를 위한 GNUInstallDirs 포함
include(GNUInstallDirs)

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

# 상태 머신 샘플 라이브러리 정의
add_library(state_machine_lib
    examples/StateMachineImpl.cpp
)

# 상태 머신 샘플 라이브러리 헤더 설정
target_include_directories(state_machine_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/examples
        ${CMAKE_CURRENT_SOURCE_DIR}/generator/include
    PRIVATE
        ${LIBXMLPP_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
)

# 라이브러리 링크
target_link_libraries(state_machine_lib
    PRIVATE
        ${LIBXMLPP_LIBRARIES}
        ${Boost_LIBRARIES}
)

# 메인 실행 파일 정의
add_executable(state_machine_example
    examples/main.cpp
)

# 실행 파일에 샘플 라이브러리 링크
target_link_libraries(state_machine_example
    PRIVATE
        state_machine_lib
        scxml_generator
)

# 제너레이터 디렉토리 추가
add_subdirectory(generator)

# 테스트 빌드 옵션 (선택적)
option(BUILD_TESTS "Build the tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 설치 설정 (GNUInstallDirs 변수 사용)
install(TARGETS state_machine_lib state_machine_example
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES
    examples/StateMachineInterface.h
    examples/StateMachineImpl.h
    examples/Context.h
    examples/MyStateMachine.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
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
