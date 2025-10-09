#!/bin/bash
# Build (if needed) and run TSAN Docker environment

set -e

# Get project root directory (parent of docker_tsan/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Check if image exists
if ! docker image inspect rsm-tsan-env:latest &> /dev/null; then
    echo "=================================================="
    echo "TSAN Docker Image Not Found"
    echo "=================================================="
    echo ""
    echo "Building lightweight Docker image with TSAN..."
    echo ""
    echo "This will:"
    echo "  1. Download Ubuntu 22.04 base image"
    echo "  2. Install build tools and dependencies"
    echo "  3. Configure nscd workarounds for TSAN"
    echo ""
    echo "Estimated time: 5-10 minutes"
    echo "Disk space required: ~1GB"
    echo ""

    read -p "Continue with build? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
    fi

    echo ""
    echo "Starting Docker build..."
    docker build -f "$SCRIPT_DIR/Dockerfile.tsan" -t rsm-tsan-env:latest "$SCRIPT_DIR" 2>&1 | tee "$SCRIPT_DIR/docker-tsan-build.log"

    echo ""
    echo "=================================================="
    echo "Docker image build complete!"
    echo "=================================================="
    echo ""
fi

echo "Starting TSAN development environment..."
echo ""
echo "Mounting project: $PROJECT_ROOT"
echo ""

# Run Docker container with:
# - Project root mounted to /workspace
# - Interactive terminal
# - Removed after exit (--rm)
# - SYS_PTRACE capability for gdb
# - Auto-build on startup
docker run -it --rm \
    --cap-add=SYS_PTRACE \
    --security-opt seccomp=unconfined \
    -v "$PROJECT_ROOT:/workspace" \
    -w /workspace \
    rsm-tsan-env:latest \
    /bin/bash -c "
        echo '=================================================='
        echo 'Auto-building project with TSAN...'
        echo '=================================================='
        echo ''

        cd /workspace
        rm -rf build
        mkdir build && cd build

        echo 'Configuring with CMake (Debug + auto-TSAN via IN_DOCKER_TSAN)...'
        cmake -DCMAKE_BUILD_TYPE=Debug ..

        echo ''
        echo 'Building project...'
        make -j\$(nproc)

        echo ''
        echo '=================================================='
        echo 'Build complete! Starting interactive shell...'
        echo '=================================================='
        echo ''
        echo 'You are now in: /workspace/build'
        echo 'Run tests with: ctest --output-on-failure'
        echo 'Or specific test: ./tests/w3c_test_cli 201'
        echo ''

        # Start interactive bash shell
        exec /bin/bash
    "
