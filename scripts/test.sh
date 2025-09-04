#!/bin/bash

# Modern test runner script for reactive-state-machine
# Usage: ./scripts/test.sh [OPTIONS]

set -e

# Default values
BUILD_DIR="build"
PARALLEL_JOBS=$(nproc 2>/dev/null || echo "4")
FILTER=""
VERBOSE="OFF"
OUTPUT_ON_FAILURE="ON"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

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
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "OPTIONS:"
    echo "  --filter PATTERN    Run tests matching pattern (e.g., 'Parser*')"
    echo "  --parallel N        Run N tests in parallel (default: auto-detect)"
    echo "  --verbose           Verbose output"
    echo "  --quiet             Minimal output"
    echo "  --list              List all available tests"
    echo "  --parser            Run only parser tests"
    echo "  --integration       Run only integration tests"
    echo "  --gtest             Run tests directly with gtest (not ctest)"
    echo "  --help              Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                        # Run all tests"
    echo "  $0 --parser               # Run parser tests only"
    echo "  $0 --filter '*Basic*'     # Run tests with 'Basic' in name"
    echo "  $0 --parallel 8           # Use 8 parallel jobs"
    echo "  $0 --gtest ParserBasicTest # Run specific test with gtest"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --filter)
            FILTER="$2"
            shift 2
            ;;
        --parallel)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE="ON"
            OUTPUT_ON_FAILURE="OFF"
            shift
            ;;
        --quiet)
            VERBOSE="OFF"
            OUTPUT_ON_FAILURE="OFF"
            shift
            ;;
        --list)
            if [[ ! -d "$BUILD_DIR" ]]; then
                echo -e "${RED}❌ Build directory not found. Run ./scripts/build.sh first${NC}"
                exit 1
            fi
            echo -e "${BLUE}📋 Available tests:${NC}"
            cd "$BUILD_DIR"
            ctest -N | grep -E "Test #[0-9]+:" | sed 's/.*Test #[0-9]*: */  - /'
            exit 0
            ;;
        --parser)
            FILTER="Parser*"
            shift
            ;;
        --integration)
            FILTER="*integration*"
            shift
            ;;
        --gtest)
            if [[ ! -d "$BUILD_DIR" ]]; then
                echo -e "${RED}❌ Build directory not found. Run ./scripts/build.sh first${NC}"
                exit 1
            fi
            TEST_NAME="$2"
            if [[ -z "$TEST_NAME" ]]; then
                echo -e "${RED}❌ Test name required for --gtest option${NC}"
                exit 1
            fi
            echo -e "${BLUE}🧪 Running $TEST_NAME directly...${NC}"
            cd "$BUILD_DIR/tests"
            if [[ -f "./$TEST_NAME" ]]; then
                "./$TEST_NAME"
            else
                echo -e "${RED}❌ Test executable not found: $TEST_NAME${NC}"
                exit 1
            fi
            exit 0
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

# Check if build directory exists
if [[ ! -d "$BUILD_DIR" ]]; then
    echo -e "${RED}❌ Build directory not found. Run ./scripts/build.sh first${NC}"
    exit 1
fi

# Go to build directory
cd "$BUILD_DIR"

# Prepare ctest command
CTEST_CMD="ctest"
if [[ -n "$FILTER" ]]; then
    CTEST_CMD="$CTEST_CMD -R '$FILTER'"
fi

CTEST_CMD="$CTEST_CMD --parallel $PARALLEL_JOBS"

if [[ "$VERBOSE" == "ON" ]]; then
    CTEST_CMD="$CTEST_CMD --verbose"
elif [[ "$OUTPUT_ON_FAILURE" == "ON" ]]; then
    CTEST_CMD="$CTEST_CMD --output-on-failure"
fi

# Print configuration
echo -e "${BLUE}🧪 Test Configuration:${NC}"
if [[ -n "$FILTER" ]]; then
    echo -e "  Filter: ${GREEN}$FILTER${NC}"
fi
echo -e "  Parallel Jobs: ${GREEN}$PARALLEL_JOBS${NC}"
echo -e "  Verbose: ${GREEN}$VERBOSE${NC}"
echo ""

# Run tests
echo -e "${BLUE}🚀 Running tests...${NC}"
eval "$CTEST_CMD"

# Check result
RESULT=$?
if [[ $RESULT -eq 0 ]]; then
    echo -e "${GREEN}✅ All tests passed!${NC}"
else
    echo -e "${RED}❌ Some tests failed (exit code: $RESULT)${NC}"
    echo -e "${YELLOW}💡 Tips:${NC}"
    echo -e "  - Run with --verbose for more details"
    echo -e "  - Run individual tests: ./scripts/test.sh --gtest TestName"
    echo -e "  - Check specific test: ./scripts/test.sh --filter '*TestName*'"
fi

exit $RESULT