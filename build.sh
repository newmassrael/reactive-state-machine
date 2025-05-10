#!/bin/bash

# 빌드 디렉토리 생성
mkdir -p build
cd build

# 빌드 타입 (기본값: Debug)
BUILD_TYPE=${1:-Debug}

# 테스트 빌드 여부 (기본값: OFF)
BUILD_TESTS=${2:-OFF}

# CMake 구성
cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DBUILD_TESTS=$BUILD_TESTS

# 빌드 실행
cmake --build . -- -j$(nproc)

# 테스트 실행 (활성화된 경우)
if [ "$BUILD_TESTS" = "ON" ]; then
    ctest --output-on-failure
fi

# 완료 메시지
echo "Build completed successfully!"
echo "Run the example with: ./state_machine_example"
