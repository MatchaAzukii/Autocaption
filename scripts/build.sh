#!/bin/bash

# Build script for Autocaption

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/.."
BUILD_DIR="$PROJECT_ROOT/build"

echo "=========================================="
echo "Building Autocaption"
echo "=========================================="

# Detect platform
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macOS"
    CORES=$(sysctl -n hw.ncpu)
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
    CORES=$(nproc)
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    PLATFORM="Windows"
    CORES=$NUMBER_OF_PROCESSORS
else
    PLATFORM="Unknown"
    CORES=4
fi

echo "Platform: $PLATFORM"
echo "Cores: $CORES"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo ""
echo "Configuring..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo ""
echo "Building..."
cmake --build . --parallel "$CORES"

echo ""
echo "=========================================="
echo "Build complete!"
echo "Binary: $BUILD_DIR/autocaption"
echo "=========================================="
