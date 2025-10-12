cmake_minimum_required(VERSION 3.14)
project(RSMCMakeFunctionExample)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This example demonstrates using rsm_add_state_machine() CMake function
# The function is automatically available when using RSM as a subproject

# Create executable
add_executable(light_example main.cpp)

# Use RSM CMake function to automatically generate state machine code
# This will:
#   1. Run scxml-codegen to generate SimpleLight_sm.h from simple_light.scxml
#   2. Add generated file to target sources
#   3. Set up dependency tracking (regenerates when SCXML changes)
#   4. Add generated directory to include paths
rsm_add_state_machine(
    TARGET light_example
    SCXML_FILE simple_light.scxml
    # OUTPUT_DIR is optional, defaults to ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Link with RSM library
target_link_libraries(light_example PRIVATE rsm_unified)

# Add RSM include directories
target_include_directories(light_example PRIVATE
    ${CMAKE_SOURCE_DIR}/rsm/include
)
