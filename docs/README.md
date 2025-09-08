# reactive-state-machine

A modern C++ implementation of SCXML-based reactive state machine framework. This project provides a comprehensive SCXML parser and runtime system that supports W3C SCXML (State Chart XML) specifications with reactive context guard systems and dependency injection capabilities.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Quick Start](#quick-start)
- [Build Requirements](#build-requirements)
- [Building the Project](#building-the-project)
- [Running Tests](#running-tests)
- [Project Structure](#project-structure)
- [Usage](#usage)
- [Development Goals](#development-goals)
- [Contributing](#contributing)
- [License](#license)

## Overview

This project consists of a complete SCXML framework implementation with three main components:

1. **SCXML Parser**: Complete W3C SCXML specification parser that converts SCXML documents into memory models
2. **Runtime System**: Full-featured runtime engine with action executors, event processors, and state managers
3. **Code Generation**: Framework for generating C++ state machine implementations from SCXML documents

The framework supports advanced features including reactive context guards, dependency injection, parallel states, history states, data models, and comprehensive event handling.

## Features

### Core SCXML Support
- Complete W3C SCXML specification implementation
- State nodes with compound and atomic states
- Parallel states and history states
- Transition guards with conditional expressions
- Action execution (assign, log, raise, send, cancel, script)
- Event handling and processing
- Data model integration with ECMAScript support
- Invoke functionality for external service communication

### Advanced Runtime Features
- **Reactive Context Guards**: Automatic re-evaluation of guard conditions when dependent data changes
- **Event System**: Comprehensive internal and external event processing
- **HTTP/WebSocket IO Processors**: Built-in communication capabilities
- **JavaScript Integration**: ECMAScript data model engine with QuickJS
- **Parallel Processing**: Support for parallel state execution
- **History Management**: Deep and shallow history state support
- **Module Loading**: Dynamic module loading and inter-module communication

### Build System & Tools
- Modern CMake 3.16+ with presets support
- Cross-platform compatibility (Linux, macOS, Windows)
- Comprehensive test suite with GoogleTest
- Code coverage support
- Sanitizer integration for development builds
- Clang-format configuration for consistent code style

## Quick Start

### Prerequisites

#### System Dependencies
Before building the project, ensure you have the following dependencies installed on your system:

**Ubuntu/Debian:**
```bash
# Update package list
sudo apt update

# Install build essentials and CMake
sudo apt install -y build-essential cmake pkg-config

# Install required libraries
sudo apt install -y libboost-all-dev libgtest-dev libgmock-dev

# Install libxml++-5.0 if available, otherwise manual build required
sudo apt install -y libxml++-5.0-dev || echo "libxml++-5.0 not available, manual build required"

# Install development tools
sudo apt install -y git doxygen curl
```

**Fedora/RHEL/CentOS:**
```bash
# Install build essentials
sudo dnf install -y gcc-c++ cmake pkgconfig

# Install required libraries
sudo dnf install -y boost-devel gtest-devel gmock-devel

# Install libxml++ dependencies
sudo dnf install -y docbook-style-xsl docbook5-schemas libxml2-devel

# Install development tools
sudo dnf install -y git doxygen curl
```

**macOS (with Homebrew):**
```bash
# Install required libraries
brew install cmake boost libxml++ googletest pkg-config

# Install development tools
brew install git doxygen curl
```

#### Required Dependencies
- **CMake 3.16+** - Build system
- **C++17 compatible compiler** (GCC 7+, Clang 5+, MSVC 2017+)
- **pkg-config** - For dependency detection
- **libxml++-5.0** - XML parsing library (version 5.0+ required)
- **Boost 1.65+** - For Signals2 and other utilities
- **GoogleTest/GMock** - For unit testing

### Build and Test

```bash
# Clone the repository
git clone <repository-url>
cd reactive-state-machine

# Build with default configuration (will automatically handle libxml++ if needed)
./scripts/build.sh

# Run all tests
./scripts/test.sh

# Build release version
./scripts/build.sh Release
```

#### Note on libxml++ 
This project requires libxml++-5.0 (not older versions). If not available in your package manager, you'll need to build it from source (see Installation Notes above). Most distributions don't package 5.0 yet, so manual build is common.

### Using CMake Presets

```bash
# Debug build with tests
cmake --preset=debug
cmake --build --preset=debug

# Release build
cmake --preset=release
cmake --build --preset=release
```

## Build Requirements

### Required Dependencies
- **CMake 3.16+** (3.19+ for preset support) - Build system
- **C++17 compiler** (GCC 7+, Clang 5+, MSVC 2017+) - Compilation
- **pkg-config** - For dependency detection
- **libxml++-5.0** - XML parsing library (version 5.0+ required)
- **Boost 1.65+** - For Signals2 and other utilities
- **GoogleTest/GMock** - For unit testing

### Optional Dependencies
- **QuickJS** - For ECMAScript data model support
- **libcurl** - For HTTP IO processor
- **WebSocket++** - For WebSocket IO processor
- **Doxygen** - For documentation generation

### Installation Notes

#### libxml++ Installation
This project requires **libxml++-5.0** (version 5.0+). Older versions are not supported.

**Ubuntu/Debian:**
```bash
# Ubuntu 24.04+ has libxml++-5.0 in repositories
sudo apt install libxml++-5.0-dev

# For older distributions, manual build is required
```

**Fedora/RHEL:**
```bash
# Check if 5.0+ version is available
sudo dnf search libxml++
sudo dnf install libxml++-devel  # If 5.0+ available
```

**Manual Build (Required for most distributions):**
Most distributions don't package libxml++-5.0 yet, so manual build is typically needed:
```bash
# Install build dependencies first
sudo apt install build-essential cmake pkg-config meson ninja-build
sudo apt install docbook-xsl docbook5-xml libxml2-utils doxygen

# Clone and build libxml++ 5.0
git clone https://github.com/libxmlplusplus/libxmlplusplus.git
cd libxmlplusplus
meson setup --prefix=/usr/local -Dmm-common:use-network=true builddir
ninja -C builddir
sudo ninja -C builddir install
```

## Building the Project

### Using Build Scripts (Recommended)

```bash
# Basic debug build
./scripts/build.sh

# Release build
./scripts/build.sh Release

# Debug build with sanitizers
./scripts/build.sh Debug --sanitizers

# Clean build
./scripts/build.sh --clean

# Build with code coverage
./scripts/build.sh Debug --coverage
```

### Manual CMake Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build . --parallel $(nproc)
```

### Build Options

- `BUILD_TESTS=ON/OFF` - Enable/disable test building (default: ON)
- `BUILD_WITH_COVERAGE=ON/OFF` - Enable code coverage (default: OFF)
- `CMAKE_BUILD_TYPE` - Debug, Release, RelWithDebInfo, MinSizeRel

## Running Tests

### Using Test Script

```bash
# Run all tests
./scripts/test.sh

# Run parser tests only
./scripts/test.sh --parser

# Run with specific pattern
./scripts/test.sh --filter "*Basic*"

# Parallel execution
./scripts/test.sh --parallel 8
```

### Using CTest Directly

```bash
cd build

# Run all tests
ctest

# Run with parallel execution
ctest --parallel $(nproc)

# Run specific test categories
ctest -L "parser"           # Parser tests
ctest -L "runtime"          # Runtime tests
ctest -L "integration"      # Integration tests

# Run with pattern matching
ctest -R "Parser.*"         # Tests starting with "Parser"
ctest -R ".*Basic.*"        # Tests containing "Basic"

# Verbose output on failure
ctest --output-on-failure
```

### Available Test Suites

**Parser Tests** (generator/parsing module):
- `ParserBasicTest` - Basic SCXML parsing functionality
- `ParserCommunicationTest` - Event communication features
- `ParserDataModelTest` - Data model processing
- `ParserErrorTest` - Error handling and validation
- `ParserEventTest` - Event system parsing
- `ParserExecutableTest` - Executable content parsing
- `ParserHistoryTest` - History state support
- `ParserInvokeTest` - Invoke functionality
- `ParserStateTest` - State node parsing
- `ParserTransitionTest` - Transition parsing

## Project Structure

```
reactive-state-machine/
├── CMakeLists.txt              # Root CMake configuration
├── CMakePresets.json           # CMake preset definitions
├── scripts/                    # Build and test scripts
│   ├── build.sh               # Build automation script
│   └── test.sh                # Test execution script
│
├── generator/                  # SCXML framework implementation
│   ├── include/               # Public headers
│   │   ├── common/           # Common utilities and types
│   │   ├── core/             # Core SCXML node implementations
│   │   ├── events/           # Event system
│   │   ├── model/            # Document model interfaces
│   │   ├── parsing/          # SCXML parsing components
│   │   └── runtime/          # Runtime execution engine
│   │       ├── executors/    # Action executors
│   │       ├── impl/         # Runtime implementations
│   │       └── interfaces/   # Runtime interfaces
│   └── src/                   # Implementation files
│       ├── codegen/          # Code generation engine
│       ├── common/           # Common utilities implementation
│       ├── core/             # Core node implementations
│       ├── events/           # Event system implementation
│       ├── model/            # Document model implementation
│       ├── parsing/          # SCXML parser implementation
│       └── runtime/          # Runtime system implementation
│
├── tests/                     # Test suite
│   ├── generator/            # Parser and generator tests
│   └── mocks/                # Mock objects for testing
│
├── docs/                      # Documentation
│   └── README.md             # This file
│
└── cmake/                     # CMake configuration files
    ├── ReactiveStateMachineConfig.cmake.in
    └── HttpProcessorConfig.cmake.in
```

## Usage

### SCXML Parser Usage

```cpp
#include "parsing/DocumentParser.h"
#include "core/NodeFactory.h"

// Create node factory for model objects
auto factory = std::make_shared<NodeFactory>();

// Create SCXML parser
DocumentParser parser(factory);

// Parse SCXML from file
auto document = parser.parseFile("state_machine.scxml");

// Parse SCXML from string
std::string scxmlContent = R"(
<?xml version="1.0"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" 
       datamodel="ecmascript" initial="idle">
  <state id="idle">
    <transition event="start" target="running"/>
  </state>
  <state id="running">
    <transition event="stop" target="idle"/>
  </state>
</scxml>
)";
auto documentFromString = parser.parseContent(scxmlContent);
```

### Runtime Engine Usage

```cpp
#include "runtime/StateMachineFactory.h"
#include "runtime/RuntimeContext.h"

// Create runtime context
auto context = std::make_shared<RuntimeContext>();

// Create state machine from parsed document
auto stateMachine = StateMachineFactory::createStateMachine(document, context);

// Initialize and start
stateMachine->initialize();
stateMachine->start();

// Send events
Event startEvent("start");
stateMachine->sendEvent(startEvent);
```

### Advanced Features

#### Reactive Guards

```cpp
// Guard conditions automatically re-evaluate when data changes
auto guard = std::make_shared<ReactiveGuard>("temperature > 30");
transition->setGuard(guard);

// When temperature data changes, guard is automatically re-evaluated
context->setData("temperature", 35); // Triggers guard evaluation
```

#### Event Processing

```cpp
// Internal event processing
InternalEventProcessor processor(context);
processor.processEvent(Event("internal.event"));

// HTTP IO processor for external communication
HTTPIOProcessor httpProcessor;
httpProcessor.send(Event("http.request"), "http://example.com/api");
```

## Development Goals

### Completed Features
- Complete SCXML parser supporting W3C specification
- Comprehensive runtime system with action executors
- Event system with internal and external processors
- HTTP and WebSocket IO processors
- ECMAScript data model integration
- Parallel state processing
- History state management
- Modern CMake build system with presets
- Extensive test suite with GoogleTest

### In Progress
- Code generation from parsed SCXML documents
- Performance optimizations for large state machines
- Enhanced debugging and introspection tools

### Future Enhancements
- Visual state machine designer integration
- Additional IO processor implementations
- State machine composition and modularity features
- Real-time performance monitoring
- Cloud deployment tools

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/new-feature`)
3. Make your changes with appropriate tests
4. Ensure all tests pass (`./scripts/test.sh`)
5. Run code formatting (`clang-format -i **/*.{h,cpp}`)
6. Commit your changes (`git commit -am 'Add new feature'`)
7. Push to the branch (`git push origin feature/new-feature`)
8. Create a Pull Request

### Development Workflow

```bash
# Set up development environment
./scripts/build.sh Debug --sanitizers

# Set up automatic code formatting (one-time setup)
./scripts/setup-hooks.sh

# Run tests during development
./scripts/test.sh --filter "*YourFeature*"

# Check code coverage
./scripts/build.sh Debug --coverage
./scripts/test.sh
# Coverage report will be in build/coverage/

# Manual code formatting (if needed)
find . -name "*.h" -o -name "*.cpp" | xargs clang-format -i
```

#### Automatic Code Formatting
The project includes a pre-commit hook that automatically formats C++ code using clang-format:

```bash
# Install the pre-commit hook (one-time setup)
./scripts/setup-hooks.sh

# Now every commit will automatically format staged C++ files
git add modified_file.cpp
git commit -m "feat: add new feature"  # Files auto-formatted before commit

# To skip formatting for a specific commit (not recommended)
git commit --no-verify -m "your message"
```

## License

This project is licensed under the MIT License. See the LICENSE file for details.