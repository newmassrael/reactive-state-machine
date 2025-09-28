#!/bin/bash

set -e  # Exit on any error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== RSM Build Script ===${NC}"

# Check if we're in the correct directory
if [[ ! -f "CMakeLists.txt" ]]; then
    echo -e "${RED}Error: CMakeLists.txt not found. Please run from the project root.${NC}"
    exit 1
fi
echo -e "${GREEN}✅ Building in correct directory: $(pwd)${NC}"

# 빌드 디렉토리 생성
mkdir -p build
cd build

# 빌드 타입 (기본값: Debug)
BUILD_TYPE=${1:-Debug}
echo -e "${YELLOW}Building in $BUILD_TYPE mode${NC}"

# 테스트는 항상 빌드
BUILD_TESTS="ON"

# 시스템 정보 출력
echo -e "${YELLOW}Building with $(gcc --version | head -n 1) on $(uname -sr)${NC}"

# CMake 구성
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DBUILD_TESTS=$BUILD_TESTS \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# 빌드 실행
echo -e "${YELLOW}Building project...${NC}"
cmake --build . --parallel $(nproc)

echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}Run the example with: ./state_machine_example${NC}"