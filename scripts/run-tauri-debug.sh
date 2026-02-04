#!/bin/bash

# Debug script for Tauri with console output

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/.."

echo "=========================================="
echo "Autocaption Tauri - Debug Mode"
echo "=========================================="
echo ""

# Check if backend is running
if ! nc -z localhost 8765 2>/dev/null; then
    echo "⚠️  Warning: Backend is not running on ws://localhost:8765"
    echo "   Please start it first: ./build/autocaption -m models/ggml-base.bin"
    echo ""
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

cd "$PROJECT_ROOT/src-tauri"

# Check if cargo is available
if ! command -v cargo &> /dev/null; then
    source "$HOME/.cargo/env"
fi

echo "Building and running Tauri with console output..."
echo "Press Ctrl+C to stop"
echo ""

# Run with console visible (dev mode)
cargo run 2>&1 | tee /tmp/autocaption-tauri.log
