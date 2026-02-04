#!/bin/bash

# Simple script to start Tauri app

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Load Rust environment
if [ -f "$HOME/.cargo/env" ]; then
    source "$HOME/.cargo/env"
fi

cd "$PROJECT_ROOT/src-tauri"

echo "Starting Autocaption Tauri..."
echo ""
echo "Note: Make sure backend is running first:"
echo "  ./build/autocaption -m models/ggml-base.bin"
echo ""

# Run Tauri
cargo run
