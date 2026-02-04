#!/bin/bash

# Build script for Autocaption Tauri app

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/.."

echo "=========================================="
echo "Building Autocaption Tauri App"
echo "=========================================="

# Check if Rust is installed
if ! command -v rustc &> /dev/null; then
    echo "Rust not found. Installing..."
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
    source "$HOME/.cargo/env"
fi

# Check if cargo-tauri is installed
if ! command -v cargo-tauri &> /dev/null; then
    echo "Installing cargo-tauri..."
    cargo install tauri-cli
fi

cd "$PROJECT_ROOT/src-tauri"

# Build the Tauri app (release mode)
echo ""
echo "Building Tauri application (Release)..."
echo ""

cargo tauri build

echo ""
echo "=========================================="
echo "Build complete!"
echo ""
echo "macOS app location:"
echo "  $PROJECT_ROOT/src-tauri/target/release/bundle/macos/Autocaption.app"
echo ""
echo "DMG installer:"
echo "  $PROJECT_ROOT/src-tauri/target/release/bundle/dmg/"
echo ""
echo "To run:"
echo "  open '$PROJECT_ROOT/src-tauri/target/release/bundle/macos/Autocaption.app'"
echo "=========================================="
