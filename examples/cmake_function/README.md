# RSM CMake Function Example

This example demonstrates how to use the `rsm_add_state_machine()` CMake function to automatically generate C++ state machine code from SCXML files.

## Overview

The `rsm_add_state_machine()` function integrates SCXML code generation into your CMake build process:
- Automatically generates C++ code from SCXML files
- Tracks dependencies (regenerates when SCXML changes)
- Adds generated files to your target
- Sets up include directories

## Usage

```cmake
# Create your executable
add_executable(my_app main.cpp)

# Generate state machine code from SCXML
rsm_add_state_machine(
    TARGET my_app
    SCXML_FILE simple_light.scxml
    # OUTPUT_DIR is optional, defaults to ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Link with RSM library
target_link_libraries(my_app PRIVATE rsm_unified)
```

## Function Parameters

- `TARGET` (required): The CMake target to add the generated code to
- `SCXML_FILE` (required): Path to the SCXML file (relative or absolute)
- `OUTPUT_DIR` (optional): Directory for generated files (defaults to `${CMAKE_CURRENT_BINARY_DIR}/generated`)

## Generated Output

For an SCXML file named `simple_light.scxml` with `name="SimpleLight"`:
- Generated file: `SimpleLight_sm.h`
- Location: `${OUTPUT_DIR}/SimpleLight_sm.h`
- Namespace: `RSM::Generated`

## Implementation Pattern

The generated code uses template-based inheritance for zero-overhead state machines:

```cpp
#include "SimpleLight_sm.h"

using namespace RSM::Generated;

// Implement your state machine logic
class LightController : public SimpleLightBase<LightController> {
public:
    // Implement required methods (guards, actions, entry/exit handlers)
    void onLightOff() { /* ... */ }
    void onLightOn() { /* ... */ }
    void turnOn() { /* ... */ }
    void turnOff() { /* ... */ }

    // Friend declaration for base class access
    friend class SimpleLightBase<LightController>;
};

int main() {
    LightController light;
    light.initialize();
    light.processEvent(Event::Switch_on);
    return 0;
}
```

## Building This Example

From the project root:

```bash
cd build
cmake .. -DBUILD_EXAMPLES=ON
make light_example
./examples/cmake_function/light_example
```

## Benefits

1. **Automatic Generation**: Code regenerates when SCXML changes
2. **Dependency Tracking**: CMake knows when to rebuild
3. **Zero Overhead**: Template-based design, no virtual functions
4. **Type Safety**: Compile-time checks for state machine logic
5. **Clean Integration**: No manual build steps required

## Roadmap: Full W3C SCXML Support

The code generator is being enhanced to support all W3C SCXML 1.0 features while maintaining zero-overhead principles:

### Current Support (~25-30%)
- Atomic states with event-based transitions
- Guards and actions (simple C++ function calls)
- Entry/exit handlers

### Coming Soon (3-5 weeks)
- **Compound States**: Hierarchical state structures
- **Parallel States**: Multiple active states simultaneously
- **History States**: State restoration (shallow and deep)
- **Final States**: Automatic done event generation
- **Eventless Transitions**: Condition-based automatic transitions
- **Internal Transitions**: Transitions without exit/entry
- **Full W3C Compliance**: 202/202 test suite passing

### Architecture: Pay for What You Use
- **Tier 0 (Zero Overhead)**: Structural features - always free
- **Tier 1 (Minimal Overhead)**: Parallel/History - small data structures only when needed
- **Tier 2 (Conditional Overhead)**: JavaScript/HTTP/Timers - linked only if SCXML uses them

See [ARCHITECTURE.md](/home/coin/reactive-state-machine/ARCHITECTURE.md) for detailed design
