#!/bin/bash

# Development script for Autocaption Tauri app

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/.."

echo "=========================================="
echo "Starting Autocaption Tauri (Dev Mode)"
echo "=========================================="
echo ""
echo "Prerequisites:"
echo "  1. Backend must be running: ./build/autocaption -m models/ggml-base.bin"
echo "  2. Rust toolchain installed"
echo ""
echo "Features:"
echo "  - System tray icon (click to show/hide)"
echo "  - Overlay window (always on top, no decorations)"
echo "  - Click-through support"
echo ""
echo "=========================================="

cd "$PROJECT_ROOT/src-tauri"

# Check if cargo is available
if ! command -v cargo &> /dev/null; then
    source "$HOME/.cargo/env"
fi

# Run in dev mode (no bundling, faster)
echo "Building and running..."
echo ""

# Build and run directly
cargo run
