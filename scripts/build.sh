#!/bin/bash

# Modern build script for reactive-state-machine
# Usage: ./scripts/build.sh [BUILD_TYPE] [OPTIONS]

set -e

# Default values
BUILD_TYPE="Debug"
BUILD_TESTS="ON"
BUILD_COVERAGE="OFF"
CLEAN_BUILD="OFF"
PARALLEL_JOBS=$(nproc 2>/dev/null || echo "4")
BUILD_DIR="build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Find project root directory (where this script's parent directory contains CMakeLists.txt)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." &> /dev/null && pwd )"

# Verify we found the right directory
if [[ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]]; then
    echo -e "${RED}Error: Could not find project root with CMakeLists.txt${NC}"
    echo -e "Expected at: $PROJECT_ROOT/CMakeLists.txt"
    exit 1
fi

# Change to project root
cd "$PROJECT_ROOT"
echo -e "${BLUE}📁 Working in project root: $PROJECT_ROOT${NC}"

# Print usage
usage() {
    echo "Usage: $0 [BUILD_TYPE] [OPTIONS]"
    echo ""
    echo "BUILD_TYPE:"
    echo "  Debug      - Debug build (default)"
    echo "  Release    - Release build"
    echo "  RelWithDebInfo - Release with debug info"
    echo ""
    echo "OPTIONS:"
    echo "  --no-tests      Disable tests"
    echo "  --coverage      Enable code coverage"
    echo "  --clean         Clean build directory first"
    echo "  --sanitizers    Enable address sanitizers (Debug only)"
    echo "  -j N            Use N parallel jobs (default: auto-detect)"
    echo "  --help          Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                           # Debug build with tests"
    echo "  $0 Release --no-tests        # Release build without tests"
    echo "  $0 Debug --coverage --clean  # Debug build with coverage, clean first"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        Debug|Release|RelWithDebInfo)
            BUILD_TYPE="$1"
            shift
            ;;
        --no-tests)
            BUILD_TESTS="OFF"
            shift
            ;;
        --coverage)
            BUILD_COVERAGE="ON"
            shift
            ;;
        --clean)
            CLEAN_BUILD="ON"
            shift
            ;;
        --sanitizers)
            if [[ "$BUILD_TYPE" == "Debug" ]]; then
                CMAKE_ARGS="$CMAKE_ARGS -DENABLE_SANITIZERS=ON"
            else
                echo -e "${YELLOW}Warning: Sanitizers only work in Debug mode${NC}"
            fi
            shift
            ;;
        -j)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
            exit 1
            ;;
    esac
done

# Print configuration
echo -e "${BLUE}🔧 Build Configuration:${NC}"
echo -e "  Build Type: ${GREEN}$BUILD_TYPE${NC}"
echo -e "  Tests: ${GREEN}$BUILD_TESTS${NC}"
echo -e "  Coverage: ${GREEN}$BUILD_COVERAGE${NC}"
echo -e "  Parallel Jobs: ${GREEN}$PARALLEL_JOBS${NC}"
echo ""

# Clean build directory if requested
if [[ "$CLEAN_BUILD" == "ON" ]] && [[ -d "$BUILD_DIR" ]]; then
    echo -e "${YELLOW}🧹 Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo -e "${BLUE}⚙️  Configuring...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_TESTS="$BUILD_TESTS" \
    -DBUILD_WITH_COVERAGE="$BUILD_COVERAGE" \
    $CMAKE_ARGS

# Build
echo -e "${BLUE}🔨 Building...${NC}"
cmake --build . --parallel "$PARALLEL_JOBS"

# Success message
echo -e "${GREEN}✅ Build completed successfully!${NC}"

if [[ "$BUILD_TESTS" == "ON" ]]; then
    echo -e "${BLUE}💡 To run tests:${NC}"
    echo -e "  cd $BUILD_DIR && ctest"
    echo -e "  cd $BUILD_DIR && ctest --parallel $PARALLEL_JOBS"
    echo -e "  ./scripts/test.sh"
fi