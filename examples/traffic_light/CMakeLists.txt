# Traffic Light Example - CMake Integration Demo

# Create executable
add_executable(traffic_light_example main.cpp)

# Auto-generate state machine code from SCXML
# This demonstrates the recommended way to integrate RSM into your project
rsm_add_state_machine(
    TARGET traffic_light_example
    SCXML_FILE traffic_light.scxml
)

# Link RSM library
target_link_libraries(traffic_light_example PRIVATE rsm_unified)
