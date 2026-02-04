#!/bin/bash

# Get script directory properly
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=========================================="
echo "Tauri Diagnostic Script"
echo "=========================================="
echo ""
echo "Script dir: $SCRIPT_DIR"
echo "Project root: $PROJECT_ROOT"
echo ""

# Change to project root
cd "$PROJECT_ROOT"

# 1. Check Rust installation
echo "1. Checking Rust..."
if command -v rustc &> /dev/null; then
    echo "   ✅ Rust installed: $(rustc --version)"
else
    echo "   ❌ Rust not found"
    exit 1
fi

# 2. Check Tauri CLI
echo ""
echo "2. Checking Tauri CLI..."
if command -v cargo-tauri &> /dev/null; then
    echo "   ✅ Tauri CLI: $(cargo-tauri --version)"
else
    echo "   ⚠️  Tauri CLI not in PATH, trying cargo..."
fi

# 3. Check webui exists
echo ""
echo "3. Checking WebUI files..."
if [ -f "$PROJECT_ROOT/webui/index.html" ]; then
    echo "   ✅ webui/index.html exists"
else
    echo "   ❌ webui/index.html not found"
fi

# 4. Check icons
echo ""
echo "4. Checking icons..."
if [ -f "$PROJECT_ROOT/src-tauri/icons/icon.png" ]; then
    echo "   ✅ icon.png exists"
else
    echo "   ❌ icon.png missing"
fi

# 5. Check backend port
echo ""
echo "5. Checking backend (port 8765)..."
if nc -z localhost 8765 2>/dev/null; then
    echo "   ✅ Backend is running on port 8765"
else
    echo "   ⚠️  Backend not running on port 8765"
    echo "      Start with: ./build/autocaption -m models/ggml-base.bin"
fi

# 6. Test build
echo ""
echo "6. Testing Tauri build..."
if [ -f "$PROJECT_ROOT/src-tauri/target/debug/autocaption-tauri" ]; then
    echo "   ✅ Debug build exists"
    ls -lh "$PROJECT_ROOT/src-tauri/target/debug/autocaption-tauri"
else
    echo "   ⚠️  No debug build found"
fi

# 7. Check bundle
echo ""
echo "7. Checking app bundle..."
if [ -d "$PROJECT_ROOT/src-tauri/target/debug/bundle/macos/Autocaption.app" ]; then
    echo "   ✅ App bundle exists"
    ls -lh "$PROJECT_ROOT/src-tauri/target/debug/bundle/macos/Autocaption.app"
else
    echo "   ⚠️  No app bundle found"
fi

echo ""
echo "=========================================="
echo "Diagnostic complete"
echo ""
echo "To run Tauri in debug mode (with console output):"
echo "  cd src-tauri && cargo run"
echo ""
echo "To run the built app:"
echo "  open src-tauri/target/debug/bundle/macos/Autocaption.app"
echo "=========================================="
